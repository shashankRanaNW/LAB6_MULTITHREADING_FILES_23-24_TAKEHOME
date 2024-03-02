# LAB6_MULTITHREADING_FILES_23-24_TAKEHOME
## Important
- If you are facing issues such as inconsistent evaluation, and are using WSL, please try using a VM, native Linux or the CC lab machines.
## **Instructions**

- This lab will be evaluated automatically.

- Any strings which are required to be sent by the client or server must be exactly as specified. (This includes the number of bytes that are being sent)

- Submit a zip file with the name `<id_no>_lab6.zip` containing your  implementation of client and server. It should be a zip of this repo. Ensure the impl folder is present and contains server.c and client.c.
  
- You may write your own test cases. No scripts or test cases will be provided.

- All 
Example Submission of necessary files, (you may have extra folders/files, it wont change anything):
```
<id_no>_lab6.zip
└── impl
    └── server.c
    └── client.c

```


<br>

The purpose of this lab is to implement a client and server program that
uses multi-threading to communicate and share files.

## **Take Home**

For this lab, implement both client and server code, they will both be
utilizing **multithreading** to send and receive messages in parallel. 
We will continue using the two types of clients – root clients and normal clients that we introduced in the previous lab.

### **client.c**

- The client will take in IP address, port through CLI arguments in the format: `./a.out <ip_address> <port>` and connect to the server.






- A `<username>@<ip_address>|<authority>\n` will be sent to the server as the first message after the server accepts the connection (Details in the example at the bottom). Note carefully that the client  includes `\n` along with the `<username>@<ip_address>|<authority>\n` sent!
  * Assume that this is ALWAYS the first communication that any client would compulsorily do to make your life easier :)
  * The <username> will be a single word without spaces or special characters.
  * The <ip_address> is the machine's ip address.
  * The <authority> specifies whether it's a root user identified by sending "r" or a normal user identified by sending "n".

  
- The client will be referred to by this `<username>@<ip_address>`.

- The client will take input through stdin and send it to the server.

- The client will receive data from the server and print it to stdout.

- **Note:** that taking input from stdin and receiving data are both
  blocking calls. Ensure these 2 can run in parallel using
  multithreading.

### **server.c**

- The server will take in IP address and port through CLI arguments in
  the format: `./a.out <ip_address> <port>` and run on that ip and
  port.

- The server will listen to a max of 1024 connections, although it can accept any amount of
  client connections. (Requirement for server evaluation)

- The server must be able to receive data from all clients in parallel
  using multithreading. **Prerequisite for further evaluation**

- The client will send messages that start with a 4 letter command. The server's behaviour will depend on this command.

- Based on the data received from the client, the server will perform
  different functions.

- In addition to the above details, the server will maintain various types of files. (2 marks)
  * There will be files for individual message exchange, which will be identified as `01_<username1>@<ip_address>-<username2>@<ip_address>.txt` (Note that there should be only one file for two client communication. Thus if `01_<username1>@<ip_address>-<username2>@<ip_address>.txt` is created, a new file `01_<username2>@<ip_address>-<username1>@<ip_address>.txt` should never get created).
  * There are files for group communication that will be named as `02_<groupname>.txt'
  * There are files to handle broadcasts that will be named '03_bcst.txt'.
  * Data in all these files will be stored in the following format:
    1.  <username1>@<ip_address>:<message>\n<username2>@<ip_address>:<message>.
    2.  individual files will only contain data exchanged by the two clients using MSGC
    3.  group communication file will only store MCST messages for that group
    4.  broadcast file will only contain messages sent using BCST


### **Commands available to client**
1. #### **LIST\n**
  * When the server receives this command it sends a string containing the names of all the clients that are currently connected to in a new format: LIST-<name1>@<ip_addr1>|<authority1>:<name2>@<ip_addr2>|<authority2>:<name3>@<ip_addr3>|<authority3>\n 
  * The server will broadcast this list to all connected clients when any **new client joins, or when an existing client exits**.

2. #### **`MSGC:<receiver_username>@<ip_address>:<message>\n`**
   [Same as what was done in the previous lab's take home]

3. #### **`GRPS:<user1>@<ip_address>,<user2>@<ip_address>,...,<usern>@<ip_address>:<groupname>\n`**
   [Same as what was done in the previous lab's take home]

4. #### **`MCST:<groupname>:<message>\n`**
   [Same as what was done in the previous lab's take home]

5. #### **`BCST:<message>\n`**
   [Same as what was done in the previous lab's take home]

6. #### **`HISF:<options>\n`**
   * This command can only be issued by root users. In case a normal user issues this command, the server responds with 'EROR:UNAUTHORIZED\n' (1 mark -- note that its is EROR, not ERROR)
   * When a server receives the HISF command, it will return back a specific type of file.
   * The type of file is identified based on options. Options are separated by `|`.
   * -t option indicates the type of file: individual(01), group communication(02), or broadcast(03).
   * When -t is 01 or 02, then -n option is also expected. -n will be the <username> in case of individual, and <groupname> in case of group communication.
   * Example commands are `HISF:-t01|-nalice@10.0.0.10\n` or `HISF:-t 02|-n GoodOnes\n` or `HISF:-t 03\n`.
     1. In the first case, if bob issued this command, then a file with all communication between bob and alice will be returned to bob. In case no communication between alice and bob has occured, a blank file with the appropriate name will be created on the server and returned back to bob. (1 +0.5 mark)
     2. In the second case, if bob issued this command and is part of GoodOnes, then the file 02_GoodOnes.txt will be returned. If there has been no MCST for GoodOnes, then a blank file with the appropriate name will be created on the server and returned back to bob (0.5 marks). If bob is not part of GoodOnes, then 'EROR:UNAUTHORIZED\n' will be returned.
     3. In the third case, all 03_bcst.txt file will be returned back to bob. 
