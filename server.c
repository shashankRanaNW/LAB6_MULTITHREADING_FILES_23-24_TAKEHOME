#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>

#define NUM_CLIENTS 1024
#define NUM_GROUPS 2048

struct client_info
{
    int client_id;
    char username[64];
    int active; // 0 for no client, 1 for active client, -1 for deactivated client
    char auth;
    char ip_add[64];
    pthread_t *client_thread;
};

char pwds[NUM_CLIENTS][64];
char pwd[64];

struct group_info
{
    char groupname[64];
    struct client_info *users[NUM_CLIENTS];
    int num_users;
};

struct client_info *clients;
struct group_info *groups;

int server_id;
int group_count = 0;
char *history;

sem_t *history_sem;
sem_t *group_sem;

// create connction
int create_connection(char *addr, int port)
{
    int server_sockfd;
    int yes = 1;
    struct sockaddr_in server_addrinfo;

    // 1. SOCKET
    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    // SockOptions
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        perror("setsockopt");
        close(server_sockfd);
        exit(1);
    }

    server_addrinfo.sin_family = AF_INET;
    server_addrinfo.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &server_addrinfo.sin_addr) <= 0)
    {
        perror("addr");
        close(server_sockfd);
        exit(1);
    }

    // 2. BIND
    if (bind(server_sockfd, (struct sockaddr *)&server_addrinfo, sizeof(server_addrinfo)) == -1)
    {
        perror("bind");
        close(server_sockfd);
        exit(1);
    }

    // 3. LISTEN
    if (listen(server_sockfd, NUM_CLIENTS) == -1)
    {
        perror("listen");
        close(server_sockfd);
        exit(1);
    }

    return server_sockfd;
}

// Accept incoming connections
int client_connect(int socket_id)
{
    int new_server_sockfd;
    socklen_t sin_size;
    struct sockaddr_in client_addrinfo;

    sin_size = sizeof(client_addrinfo);

    // 4. ACCEPT
    new_server_sockfd = accept(socket_id, (struct sockaddr *)&client_addrinfo, &sin_size);
    if (new_server_sockfd == -1)
    {
        perror("accept");
        close(socket_id);
        exit(1);
    }

    return new_server_sockfd;
}

// receive input from client
void recv_data(struct client_info client, char *msg)
{
    int recv_count;
    // memset(msg, 0, 1024);

    // 5. RECEIVE
    if ((recv_count = recv(client.client_id, msg, 1024, 0)) == -1)
    {
        perror("recv");
        close(client.client_id);
        exit(1);
    }
}

// send reply to client
void send_data(struct client_info client, char *reply)
{
    int send_count;

    // 6. SEND
    if ((send_count = send(client.client_id, reply, 1024, 0)) == -1)
    {
        perror("send");
        close(client.client_id);
        exit(1);
    }
}

void handle_list()
{
    char list[1024];
    memset(list, 0, 1024);
    strcpy(list, "LIST-");
    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        if (clients[i].active == 1)
        {
            strcat(list, clients[i].username);
            strcat(list, "@");
            strcat(list, clients[i].ip_add);
            strcat(list, "|");
            if (clients[i].auth == 'r')
            {
                strcat(list, "r");
            }
            else
            {
                strcat(list, "n");
            }
            strcat(list, ":");
        }
    }

    int len = strlen(list);
    if (len > 0)
        list[len - 1] = '\n';
    printf("%s", list);
    fflush(stdout);
    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        if (1 == clients[i].active)
        {
            send_data(clients[i], list);
        }
    }
}

void handle_msgc(char msg[1024], struct client_info *client);

void handle_mcst(char msg[1024], struct client_info *client);

void * handle_bcst(char  msg[1024], struct client_info * client, int* retFlag);

void handle_grps(char msg[1024], struct client_info *client);

void *recv_worker(void *arg)
{
    struct client_info *client;
    client = (struct client_info *)arg;

    // new client joined, send the list
    handle_list();

    char msg[1024];
    while (1)
    {
        memset(msg, 0, 1024);
        recv_data(*client, msg);

        char *log = malloc(1024 * sizeof(char));
        memset(log, 0, 1024);
        strcat(log, client->username);
        strcat(log, "-");
        strcat(log, msg);

        sem_wait(history_sem);
        int len = strlen(history);
        history = realloc(history, len + strlen(log) + 1);
        memset(history + len, 0, strlen(log) + 1);
        strcat(history, log);
        sem_post(history_sem);

        if (!strcmp(msg, "EXIT\n"))
        {
            client->active = -1;
            close(client->client_id);
            handle_list();

            pthread_exit(0);
        }

        /*
         * When the server receives this command it sends a string containing the names of all the clients that are currently connected to in a new format: LIST-<name1>@<ip_addr1>|<authority1>:<name2>@<ip_addr2>|<authority2>:<name3>@<ip_addr3>|<authority3>\n
         * The server will also broadcast this list to all exiting connected clients when any new client joins, or when an existing client exits. (1 mark)
         */
        else if (!strcmp(msg, "LIST\n"))
        {
            handle_list();
        }

        else if (!strncmp(msg, "MSGC:", 5))
        {
            handle_msgc(msg, client);
        }

        else if (!strncmp(msg, "BCST:", 5))
        {
            int retFlag;
            void * retVal = handle_bcst(msg, client, &retFlag);
            if (retFlag == 1) return retVal;    
        }

        else if (!strncmp(msg, "GRPS:", 5))
        {
            handle_grps(msg, client);
        }

        else if (!strncmp(msg, "MCST:", 5))
        {
            handle_mcst(msg, client);
        }

        else if (!strcmp(msg, "HIST\n"))
        {
            send_data(*client, history);
        }

        else if (!strncmp(msg, "CAST:", 5))
        {
            if (client->auth == 'r')
            {
                char *token = strtok(msg, ":");
                token = strtok(0, ":");
                char mesg[1024] = {0};
                strcpy(mesg, client->username);
                strcat(mesg, ":");
                strcat(mesg, token);
                for (int i = 0; i < NUM_CLIENTS; i++)
                {
                    if ((client->client_id != clients[i].client_id) && clients[i].active == 1 && clients[i].auth == 'r')
                        send_data(clients[i], mesg);
                }
            }
            else
            {
                char err[1024] = {0};
                strcpy(err, "UNAUTHORIZED\n");
                send_data(*client, err);
            }
        }

        else if (!strncmp(msg, "HISF:", 5)){
            if (client->auth == 'r')
            {
                char *token = strtok(msg, ":");
                token = strtok(0, "\n");
                char opts[1024] = {0};
                strcpy(opts, token);
                char *opt_token;
                char name[1024] = {0};
                int file_type = 0;

                opt_token = strtok(opts, "|");
                while (opt_token != NULL)
                {
                    if (strncmp(opt_token, "-t ", 3) == 0)
                    {
                        file_type = atoi(opt_token + 3);
                    }
                    else if (strncmp(opt_token, "-n ", 3) == 0)
                    {
                        strcpy(name, opt_token + 3);
                    }
                    opt_token = strtok(NULL, "|");
                }

                // fprintf(stdout, "Received file_type: %d\n", file_type);
                // fflush(stdout);
                // if (name[0] == '\0')
                // {
                //     fprintf(stdout, "No name received, name is NULL\n");
                //     fflush(stdout);
                // }
                // else
                // {
                //     fprintf(stdout, "Received name: %s\n", name);
                //     fflush(stdout);
                // }

                if (file_type == 1 || file_type == 2)
                {
                    if (name == NULL)
                    {
                        send_data(*client, "Bad format HISF command\n");
                    }
                    else
                    {
                        // Process the command with the given name
                    }
                }
                else if (file_type == 3)
                {
                    // Process the command for broadcast
                }
                else
                {
                    send_data(*client, "Bad format HISF command\n");
                }
            }
            else
            {
                char err[1024] = {0};
                strcpy(err, "EROR:UNAUTHORIZED\n");
                send_data(*client, err);
            }
        }
        else{
            send_data(*client, "INVALID COMMAND\n");
        }
    }
}

void handle_grps(char msg[1024], struct client_info *client)
{
    sem_wait(group_sem);
    char *token;
    char *names;
    char *groupname;

    token = strtok(msg, ":");
    token = strtok(NULL, ":");

    names = strdup(token);

    token = strtok(NULL, ":");
    groupname = strdup(token);
    groupname[strlen(groupname) - 1] = '\0'; // remove the \n

    token = strdup(names);
    token = strtok(token, ",");

    int valid = 1;
    while (token)
    {
        char usrn[64] = {0};
        char ip_addr[64] = {0};
        strcpy(usrn, strtok(token, "@"));
        strcpy(ip_addr, strtok(NULL, "~"));
        int found = 0;
        for (int i = 0; i < NUM_CLIENTS; i++)
        {
            if (!strcmp(clients[i].username, usrn) && clients[i].active == 1)
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            send_data(*client, "INVALID USERS LIST\n");
            valid = 0;
            break;
        }
        token = strtok(NULL, ",");
    }

    if (valid)
    {
        strcpy(groups[group_count].groupname, groupname);

        token = strtok(names, ",");
        int user_count = 0;

        while (token)
        {
            char usrn[64] = {0};
            char ip_addr[64] = {0};
            sscanf(token, "%[^@]@%s", usrn, ip_addr);
            for (int i = 0; i < NUM_CLIENTS; i++)
            {
                if (!strcmp(clients[i].username, usrn))
                {
                    groups[group_count].users[user_count] = &clients[i];
                }
            }
            token = strtok(NULL, ",");
            user_count++;
        }

        groups[group_count].num_users = user_count;

        char data[100];
        sprintf(data, "GROUP %s CREATED\n", groups[group_count].groupname);
        for (int i = 0; i < groups[group_count].num_users; i++)
        {
            fprintf(stdout, "Member %d: %s\n", i + 1, groups[group_count].users[i]->username);
            fflush(stdout);
        }

        send_data(*client, data);
        group_count++;
    }
    sem_post(group_sem);
}

void * handle_bcst(char  msg[1024], struct client_info * client, int* retFlag){
    retFlag = 1;
    char *token = strtok(msg, ":");
    token = strtok(NULL, ":");

    char *reply;
    reply = strdup(client->username);
    strcat(reply, ":");
    strcat(reply, token);

    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        if ((client->client_id != clients[i].client_id) && clients[i].active == 1)
            send_data(clients[i], reply);
    }

    // Handle broadcast message
    char bcst_file[] = "03_bcst.txt";
    FILE *file = fopen(bcst_file, "a");
    if (file == NULL)
    {
        file = fopen(bcst_file, "w");
        if (file == NULL)
        {
            perror("Error opening file");
            return NULL;
        }
    }
    fprintf(file, "%s", reply );
    fclose(file);        
    retFlag = 0;
    return NULL;
}

void handle_mcst(char msg[1024], struct client_info *client){
    sem_wait(group_sem);
    char *token = strtok(msg, ":");
    token = strtok(NULL, ":");

    int grpidx = -1;

    for (int i = 0; i < group_count; i++)
    {
        if (!strcmp(token, groups[i].groupname))
            grpidx = i;
    }

    if (grpidx == -1)
    {
        char data[100];
        sprintf(data, "GROUP %s NOT FOUND\n", token);
        send_data(*client, data);
    }

    if (grpidx != -1)
    {
        char data[1024] = {0};
        strcpy(data, client->username);
        strcat(data, "@");
        strcat(data, client->ip_add);
        strcat(data, ":");

        token = strtok(NULL, ":");
        strcat(data, token);

        for (int i = 0; i < groups[grpidx].num_users; i++)
        {
            struct client_info *recv_client = groups[grpidx].users[i];
            if (recv_client->active == 1)
                send_data(*recv_client, data);
        }

        FILE *file;
        char filename[1024] = "02_";
        strcat(filename, groups[grpidx].groupname);
        strcat(filename, ".txt");

        file = fopen(filename, "a");
        if (file == NULL)
        {
            file = fopen(filename, "w");
        }

        if (file != NULL)
        {
            fprintf(file, "%s", data);
            fflush( file);
            fclose(file);
        }
        else
        {
            printf("Error opening file!\n");   
        }
    }    
    sem_post(group_sem);
}

void handle_msgc(char* msg, struct client_info *client)
{
    char *token = strtok(msg, ":");
    token = strtok(NULL, "@");

    char username[1024] = {0};
    strcpy(username, token);

    token = strtok(NULL, ":");
    char ip_addr[1024] = {0};
    strcpy(ip_addr, token);
    token = strtok(NULL, "\n");
    char mess[1024] = {0};
    strcpy(mess, token);

    // fprintf( stdout, "username:%s\nip_addr:%s\nmsg:%s\n", username, ip_addr, msg);
    // fflush( stdout );

    char reply[1024] = {0};
    strcpy(reply, client->username);
    strcat(reply, "@");
    strcat(reply, ip_addr);
    strcat(reply, ":");
    strcat(reply, mess);
    strcat(reply, "\n");

    fprintf(stdout, "reply:%s", reply);

    int user_found = 0;

    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        if (!strcmp(username, clients[i].username) && clients[i].active == 1)
        {
            send_data(clients[i], reply);
            if( strcmp( client->username, clients[i].username ) < 0 ){
                char file_n[1024] = {0};
                strcpy( file_n, "01_");
                strcat( file_n, client->username );
                strcat( file_n, "_" );
                strcat( file_n, clients[i].username );
                strcat( file_n, ".txt" );
                FILE *file = fopen( file_n, "a" );
                fprintf( file, "%s", reply );
                fclose( file );
            }
            else{
                char file_n[1024] = {0};
                strcpy( file_n, "01_");
                strcat( file_n, clients[i].username );
                strcat( file_n, "_" );
                strcat( file_n, client->username );
                strcat( file_n, ".txt" );
                FILE *file = fopen( file_n, "a" );
                fprintf( file, "%s", reply );
                fclose( file );
            }
            user_found = 1;
        }
    }

    if (user_found == 0)
        send_data(*client, "USER NOT FOUND\n");

    char file_n[1024] = {0};
    strcpy( file_n, "01_");
    

    
}

void *connect_thread(void *args)
{
    for (int count = 0; count < NUM_CLIENTS; count++)
    {
        char username[64] = {0};
        int client_id = client_connect(server_id);

        clients[count].client_id = client_id;
        clients[count].active = 1;
        // <username>@<ip_address>|<authority>\n
        recv_data(clients[count], username);
        char *token = strtok(username, "@");
        // printf("%s ", token);
        // fflush(stdout);
        strcpy(clients[count].username, token);
        strcpy(clients[count].ip_add, strtok(NULL, "|"));
        token = strtok(NULL, "\n");
        printf("Username:\t%s\n", clients[count].username);
        printf("IP:\t%s\n", clients[count].ip_add);
        printf("Token:\t%s\n", token);
        // printf("%s\n", token);
        // fflush(stdout);
        clients[count].auth = token[0];

        clients[count].client_thread = (pthread_t *)malloc(sizeof(pthread_t *));

        if (token[0] == 'r') // root user
        {
            char password[1024] = {0};
            char msg[1024] = {0};
            recv_data(clients[count], password);
            password[strlen(password) - 1] = 0;
            if (!strcmp(pwd, password))
            {
                // printf("PASSWORD ACCEPTED\n");
                // fflush(stdout);
                strcpy(msg, "PASSWORD ACCEPTED\n");
                send_data(clients[count], msg);
            }
            else
            {
                // printf("Correct pass was:\t%s\n", pwd);
                // printf("You entered:\t%s\n", password);
                // printf("PASSWORD REJECTED\n");
                // fflush(stdout);
                strcpy(msg, "ERROR:PASSWORD REJECTED\n");
                send_data(clients[count], msg);
                close(clients[count].client_id);
                clients[count].active = -1;
                continue;
            }
        }

        if (pthread_create(clients[count].client_thread, NULL, recv_worker, &clients[count]) != 0)
        {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        if (pthread_join(*clients[i].client_thread, NULL) != 0)
        {
            perror("pthread_join");
        }
    }

    pthread_exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Use 3 cli arguments\n");
        return -1;
    }

    // extract the address and port from the command line arguments
    char addr[INET6_ADDRSTRLEN];
    unsigned int port;

    strcpy(addr, argv[1]);
    port = atoi(argv[2]);
    strcpy(pwd, argv[3]);

    // NUM_CLIENTS = atoi(argv[3]);

    clients = (struct client_info *)malloc(NUM_CLIENTS * sizeof(struct client_info));
    groups = (struct group_info *)malloc(NUM_GROUPS * sizeof(struct group_info));
    history = (char *)malloc(sizeof(char *));

    // memset( clients, 0, )

    for (int i = 0; i < NUM_CLIENTS; i++)
        clients[i].active = 0;

    server_id = create_connection(addr, port);

    // group_sem = sem_open("/group", O_CREAT, 0644, 1);
    group_sem = (sem_t *)malloc(sizeof(sem_t));
    sem_init(group_sem, 0, 1);
    // history_sem = sem_open("/group", O_CREAT, 0644, 1);
    history_sem = (sem_t *)malloc(sizeof(sem_t));
    sem_init(history_sem, 0, 1);

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, connect_thread, NULL) != 0)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    // Wait for the thread to finish
    if (pthread_join(thread_id, NULL) != 0)
    {
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    close(server_id);

    printf("SERVER TERMINATED: EXITING......\n");
    fflush(stdout);

    return 0;
}