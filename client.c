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
    #include<stdbool.h>
    pthread_t threads[2];
   
     
    // Create a connection to the server
    int create_connection(char* addr, int port) 
    {	
    	int client_sockfd;
    	// 1. SOCKET
        if((client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)		
    	{
            perror("client: socket");
            fflush(stdout);
            exit(1);
        }
     
    	// NO BIND NECESSARY IN CLIENT!!!
     
    	struct sockaddr_in server_addrinfo;			
        server_addrinfo.sin_family = AF_INET;		
        server_addrinfo.sin_port = htons(port);	
     
        if(inet_pton(AF_INET, addr, &server_addrinfo.sin_addr) <= 0)
    	{
            printf("\nInvalid address/ Address not supported \n");
            close(client_sockfd);
            exit(1);
        }
     
     
    	// 2. CONNECT
        if(connect(client_sockfd, (struct sockaddr*)&server_addrinfo, sizeof(server_addrinfo)) == -1)
    	{ // client connects if server port has started listen()ing and queue is non-full; however server connects to client only when it accept()s
            
    		printf("Could not find server");	
            close(client_sockfd);
            exit(1);
        }
        
     
    	return client_sockfd;	
    }
     
    void* send_data(void* args) 
    {
    	while(true){
            char msg[1024];
            memset(msg, 0, 1024);
            int socket_id=*(int*)args;
        
            fgets(msg, 1024, stdin);
            // Has new line with it
            int send_count;
            if((send_count = send(socket_id, msg, 1024, 0)) == -1)	
            {
                perror("send");
                fflush(stdout);
                exit(1);
    	    }

            if(strcmp(msg, "EXIT\n")==0){
                break;
            }
        }
        pthread_exit(NULL);
    }
     
    // Receive input from the server
    void* recv_data(void *args) 
    {
    	while(true)
        {
            char reply[1024];
            memset(reply, 0, 1024);
            int socket_id=*(int*)args;
        
            int recv_count;
            if((recv_count = recv(socket_id, reply, 1024, 0)) == -1)	
            {
                perror("recv");
                fflush(stdout);
                exit(1);
            }
            if(recv_count == 0)
            {
                break;
            }
            printf("%s", reply);
        }
        pthread_exit(NULL);
    }
     
    int main(int argc, char *argv[])
    {
        if (argc != 3)
    	{
    		printf("Use 2 cli arguments\n");
    		return -1;
    	}
        
    	// extract the address and port from the command line arguments
     
    	char addr[INET6_ADDRSTRLEN];	
    	strcpy(addr, argv[1]);
     
    	unsigned int port;				
    	port = atoi(argv[2]);
     
    	int socket_id = create_connection(addr, port);
        
            int *client_sockfd_ptr = malloc(sizeof(int));;
            *client_sockfd_ptr = socket_id;	
        
            if(pthread_create(&threads[0], NULL, send_data, client_sockfd_ptr)!=0){
                perror("pthread_create failed");
                fflush(stdout);
                close(socket_id);
            }
            
            
            if(pthread_create(&threads[1], NULL, recv_data, client_sockfd_ptr)!=0){
                perror("pthread_create failed");
                fflush(stdout);
                close(socket_id);

            }

            for (int i = 0; i < 2; i++) {
                if(pthread_join(threads[i], NULL) != 0) {
                    perror("pthread_join failed");
                    fflush(stdout);
                    return -1;
                }
            }

        // Close socket and free memory
            close(socket_id);
            free(client_sockfd_ptr);
            return 0;
       
            
           
        
    }
