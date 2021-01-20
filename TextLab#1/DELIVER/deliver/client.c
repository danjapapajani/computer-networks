/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: papajan6
 *
 * Created on November 18, 2018, 6:45 PM
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#define MAXSIZE 1024
#define MAXNAME 100
#define MAXSEND 2048
#define MAXPASSWORD 100 
#define MAXSESSIONNAME 100
int buffer_read(char *r_buffer, char *type, char *source, char *data);
int connect_auth(const char *server_IP, const int server_port, const char *client_ID, const char *password);
int create_session(const int socket, const char* new_session);
int list(const int socket);
int join_session(const int socket, char *session_ID);
int leave_session(const int socket);
int logout(const int socket);
int send_text(const int socket, char *text);
int recvline(const int sock, char *buf, const size_t buflen);
struct message{
	unsigned int type;
	unsigned int size;
	unsigned char source[MAXNAME];
	unsigned char data[MAXSIZE];
};
char current_user[MAXNAME];
int main(int argc, char **argv){
	int conn = -1;
	int input_size = MAXSIZE;
	
	bzero((char *) &current_user, sizeof(current_user));
	char *input_string;
	input_string = (char *) malloc(MAXSIZE);
	printf("> Text Conference V0.1\n");
	bool connected = false;
	bool insession = false;
	fd_set rfds;  
	int maxfd;
	while(1){
		//printf("hi\n");
		// If no connection established, wait for user input to login
		if (!connected || conn <0){
			// Read user input
			bzero((char *) &input_string, sizeof(input_string));
			int bytes_read;
			bytes_read = getline (&input_string, &input_size, stdin);
			if (input_string[bytes_read - 1] == '\n') 
  			{
      			input_string[bytes_read - 1] = '\0';
      			bytes_read--;
  			}
		
  			// If input is a comand, keep parsing, else report error
			if (input_string[0]=='/')	//program cmd
			{
				char *space_bar = " ";
				char *cmd;
				cmd = strtok(input_string, space_bar);
				
				// Only login cmd is alowed in this stage
				if(!strcmp(cmd, "/login"))
				{
					char *client_Id;
					char *password;
					char *server_IP;
					char *server_port_s;
					char* temp;
					bool valid_arg = false;
					intmax_t port_num;
					client_Id = strtok(NULL, space_bar);
					if (client_Id != NULL){
						password = strtok(NULL, space_bar);
						if (password != NULL){
							server_IP = strtok(NULL, space_bar);
							if (server_IP != NULL){
								server_port_s = strtok(NULL, space_bar);
								if(server_port_s != NULL){
									temp = strtok(NULL, space_bar);
									if (temp == NULL) {
										// Correct number of input argument
										// Check if port_num is valid as int;

										bool valid_port=true;
										for(int i=0; i<strlen(server_port_s); i++){
											if(!isdigit(server_port_s[i])){
												valid_port=false;
												break;
											}	
										}
										if(valid_port){						
											port_num  = strtoimax(server_port_s, NULL, 10);
											if (!(port_num == UINTMAX_MAX && errno == ERANGE)){
   											valid_arg = true;
   											}
   										}
   									}
   								}		
							}
						}
					}
								
					//
					if(valid_arg && !connected)
					{
						//printf("hi\n");
						conn = connect_auth(server_IP, port_num, client_Id, password);
						//printf("conn %d\n", conn);
						if (conn>0){
							connected = true;
							strcpy(current_user, client_Id);
						}
						else
							connected = false;				
					}
					else if (!valid_arg){
						// Error handling: invalid argument
						fprintf(stderr, "Invalid command! Use \"/login <client ID> <password> <server IP> <server port>\" to exit the server.\n");
					}
					else if (connected){
						fprintf(stderr, "Invalid command! Program is connected to a server!\n");
					}
					else{
						fprintf(stderr, "Unknown Error. Error code: 00");
					}
				}
				else if(!strcmp(cmd, "/quit")){
					return;
				}
			
				// Other cmd not allowed
				else{
					fprintf(stderr, "Invalid command! Program is NOT connected to a server!\n");
				} 
			}
			else{
					fprintf(stderr, "Invalid command! Program is NOT connected to a server!\n");
			} 
		}


		// Once connected, use select to achieve I/O reuse
		else if (connected && (conn > 0)){
			//printf("yo %d\n", conn);
			// Empty readable file descriptors
			FD_ZERO(&rfds);
			// Add stdin to file descriptors
			FD_SET(0, &rfds);
			maxfd = 0;    
			// Add socket to file descriptors
			FD_SET(conn, &rfds);  
			if(maxfd < conn)  
            	maxfd = conn; 
        	int retval = select(maxfd+1, &rfds, NULL, NULL, NULL); 
        	if (retval == -1){
        		printf("Error in select\n");
        		return 0;
        	}
        	else if (retval == 0){
        		continue;
        	}
        	else{
        		// server send message to socket
        		if(FD_ISSET(conn,&rfds)){  
        			//printf("incoming\n");
        			char r_buffer[MAXSEND];
					memset(r_buffer, 0, sizeof(r_buffer));
					recvline(conn, r_buffer, MAXSEND);
					int len = strlen(r_buffer);
					if (r_buffer[len - 1] == '\n') 
      					r_buffer[len - 1] = '\0';
					int r_size;
					char type[MAXNAME];
					memset(type, 0, sizeof(type));
					char source[MAXNAME];
					memset(source, 0, sizeof(source));
					char data[MAXSIZE];
					memset(data, 0, sizeof(data));
					r_size = buffer_read(r_buffer, type, source, data);
					if(!strcmp(type, "MESSAGE"))
        				printf("%s: %s\n", source, data);
        		}

        		// user types into stdin
        		else if(FD_ISSET(0, &rfds)){ 
        			// Read input string
        			bzero((char *) &input_string, sizeof(input_string));
					int bytes_read = getline (&input_string, &input_size, stdin);
					if (input_string[bytes_read - 1] == '\n') 
  					{
      					input_string[bytes_read - 1] = '\0';
      					bytes_read--;
  					}

        			// If input is a comand, keep parsing, else send if passible
					if (input_string[0]=='/')	//program cmd
					{
						//rintf("cmd: %s\n", input_string);
						char *space_bar = " ";
						char *cmd = strtok(input_string, space_bar);
						if(!strcmp(cmd, "/login")){
							fprintf(stderr, "Invalid command! Program is connected to a server!\n");
							while (cmd != NULL){
								cmd = strtok(NULL, space_bar);
							}
						}
						else if (!strcmp(cmd, "/logout"))
						{
							bool valid_arg = false;
							cmd = strtok(NULL, space_bar);
							if (cmd == NULL){
								// Correct number of input argument
								valid_arg = true;
							}
							// 
							if(valid_arg && connected){
								// log out
								if((logout(conn))>0){
									
								connected = false;
								insession = false;
								}

							}
							else if (!valid_arg){
								// Error handling: invalid argument
								fprintf(stderr, "Invalid command! Use \"/logout\" to exit the server.\n");
							}
							else if (!connected){
								fprintf(stderr, "Invalid command! Program is not connected to server!\n");
							}
							else{
								fprintf(stderr, "Unknown Error. Error code: 01");
							}
						}
				
						else if (!strcmp(cmd, "/joinsession"))
						{
							char *session_ID;
							bool valid_arg = false;
							session_ID = strtok(NULL, space_bar);
							if(session_ID != NULL){
								cmd = strtok(NULL, space_bar);
								if (cmd == NULL){
									// Correct number of input argument
									valid_arg = true;
								}
							}
				
							//
							if (valid_arg&&connected&&!insession){
								if((join_session(conn, session_ID))>0)
									insession = true;
								else
									insession = false;
								
							}				
							else if (!valid_arg){
								// Error handling: invalid argument
								fprintf(stderr, "Invalid command! Use \"/joinsession <session ID>\" to join a session.\n");
							}
							else if (!connected){
								fprintf(stderr, "Invalid command! Program is not connected to server!\n");
							}
							else if (insession){
								fprintf(stderr, "Invalid command! Joining multiple sessions is not allowed!\n");
							}
							else{
								fprintf(stderr, "Unknown Error. Error code: 02");
							}
						} 
			
						else if (!strcmp(cmd, "/leavesession"))
						{
							bool valid_arg = false;
							cmd = strtok(NULL, space_bar);
							if (cmd == NULL){
								valid_arg = true;
							}
				
							//
							if (valid_arg && insession && connected){
								if((leave_session(conn))>0)
									insession = false;
							}
							else if (!valid_arg){
								// Error handling: invalid argument
								fprintf(stderr, "Invalid command! Use \"/leavesession\" to leave current session.\n");
							}
							else if (!connected){
								fprintf(stderr, "Invalid command! Program is not connected to server!\n");
							}
							else if (!insession){
								fprintf(stderr, "Invalid command! program is not in any session!\n");
							}
							else{
								fprintf(stderr, "Unknown Error. Error code: 03");
							}
						}
			
						else if (!strcmp(cmd, "/createsession"))
						{
							bool valid_arg = false;
							char *session_ID;
							session_ID = strtok(NULL, space_bar);
							if(session_ID != NULL){
								cmd = strtok(NULL, space_bar);
								if (cmd == NULL){
									valid_arg = true;
								}
							}
				
							//
							if (valid_arg && connected && !insession){
								if((create_session(conn, session_ID))>0)
									insession = true;
								else
									insession = false;
							}
							else if (!valid_arg)	{
								// Error handling: invalid argument
								fprintf(stderr, "Invalid command! Use \"/createsession <session ID>\" to leave current session.\n");
							}
							else if (!connected){
								fprintf(stderr, "Invalid command! Program is not connected to server!\n");
							}
							else if (insession){
								fprintf(stderr, "Invalid command! Joining multiple sessions is not allowed!\n");
							}
							else{
								fprintf(stderr, "Unknown Error. Error code: 04");
							}
						}
			
						else if (!strcmp(cmd, "/list"))
						{
							bool valid_arg = false;
							cmd = strtok(NULL, space_bar);
							if (cmd == NULL){
								valid_arg = true;
							}
				
							//
							if(valid_arg && connected){
								list(conn);
							}
							else if (!valid_arg)	{
								// Error handling: invalid argument
								fprintf(stderr, "Invalid command! Use \"/list\" to list available clients and sessions.\n");
							}
							else if (!connected){
								fprintf(stderr, "Invalid command! Program is not connected to server!\n");
							}
							else{
								fprintf(stderr, "Unknown Error. Error code: 05");
							}
						}

						else if (!strcmp(cmd, "/quit"))
						{
							bool valid_arg = false;
							cmd = strtok(NULL, space_bar);
							if (cmd == NULL){
								valid_arg = true;
							}
				
							//
							if (valid_arg){
								while((logout(conn))<0);
								free(input_string);
								return 0;
							}
							else if (!valid_arg)	{
								// Error handling: invalid argument
								fprintf(stderr, "Invalid command! Use \"/quit\" to terminate program\n");
							}
							else{
								fprintf(stderr, "Unknown Error. Error code: 06");
							}		
						} 
						else
						{
							fprintf(stderr, "Command is not recognized!\n");
						}					
					}

					// send as text
					else {
						if(connected && insession){
							send_text(conn, input_string);
						}
						else if (!connected){
							fprintf(stderr, "Program is not connected to server!\n");
						}
						else if (!insession){
							fprintf(stderr, "Invalid command! program is not in any session!\n");
						}
						else{
							fprintf(stderr, "Unknown Error. Error code: 07");
						}
					}				
        		} 
        		///////////////////////////////////
        	}
        }
	}

return 0;
}

int send_text(const int socket, char *text)
{
	if (text ==NULL || text == '\0') {
		fprintf(stderr, "Unable to send: Invalid text\n");
		return -1;
	}
	if(socket <0){
		fprintf(stderr, "Unable to send: Invalid socket\n");
		return -1;
	}
	
	struct message new_message;
	memset(new_message.data, 0, sizeof(new_message.data));
	memset(new_message.source, 0, sizeof(new_message.source));

	new_message.type = 11;
	strcpy(new_message.data, text);
	new_message.size = strlen(new_message.data);
	strcpy(new_message.source, current_user);
	char data_size[4];
	memset(data_size, 0, sizeof(data_size));
	sprintf(data_size, "%d", new_message.size);
	char w_buffer[MAXSEND];
	memset(w_buffer, 0, sizeof(w_buffer));
	snprintf(w_buffer, sizeof(w_buffer), "MESSAGE %s %s %s\n", data_size, new_message.source, new_message.data);
	if(write(socket, w_buffer, strlen(w_buffer)) <0){
		fprintf(stderr, "Error sending message\n");
		return -1;
	}
	return 1;


}



int logout(const int socket)
{
	
	if(socket <0){
		fprintf(stderr, "Unable to join session: Invalid socket\n");
		return -1;
	}
	struct message new_message;
	memset(new_message.data, 0, sizeof(new_message.data));
	memset(new_message.source, 0, sizeof(new_message.source));

	new_message.type = 4;
	new_message.size = 0;
	strcpy(new_message.source, current_user);
	
	char w_buffer[MAXSEND];
	memset(w_buffer, 0, sizeof(w_buffer));
	snprintf(w_buffer, sizeof(w_buffer), "EXIT 0 %s\n", new_message.source);
	if(write(socket, w_buffer, strlen(w_buffer)) <0){
		fprintf(stderr, "Error sending message\n");
		return -1;
	}
	return 1;

}
int leave_session(const int socket)
{
	
	if(socket <0){
		fprintf(stderr, "Unable to join session: Invalid socket\n");
		return -1;
	}
	struct message new_message;
	memset(new_message.data, 0, sizeof(new_message.data));
	memset(new_message.source, 0, sizeof(new_message.source));

	new_message.type = 8;
	new_message.size = 0;
	strcpy(new_message.source, current_user);
	
	char w_buffer[MAXSEND];
	memset(w_buffer, 0, sizeof(w_buffer));
	snprintf(w_buffer, sizeof(w_buffer), "LEAVE_SESS 0 %s\n", new_message.source);
	if(write(socket, w_buffer, strlen(w_buffer)) <0){
		fprintf(stderr, "Error sending message\n");
		return -1;
	}
	return 1;

}


int join_session(const int socket, char *session_ID)
{
	if (session_ID ==NULL || session_ID[0] == '\0') {
		fprintf(stderr, "Unable to join session: Invalid session ID\n");
		return -1;
	}
	if(socket <0){
		fprintf(stderr, "Unable to join session: Invalid socket\n");
		return -1;
	}
	struct message new_message;
	memset(new_message.data, 0, sizeof(new_message.data));
	memset(new_message.source, 0, sizeof(new_message.source));

	new_message.type = 5;
	strcpy(new_message.data, session_ID);
	new_message.size = strlen(new_message.data);
	strcpy(new_message.source, current_user);
	char data_size[4];
	memset(data_size, 0, sizeof(data_size));
	sprintf(data_size, "%d", new_message.size);
	char w_buffer[MAXSEND];
	memset(w_buffer, 0, sizeof(w_buffer));
	snprintf(w_buffer, sizeof(w_buffer), "JOIN %s %s %s\n", data_size, new_message.source, new_message.data);
	if(write(socket, w_buffer, strlen(w_buffer)) <0){
		fprintf(stderr, "Error sending message\n");
		return -1;
	}
	
	char r_buffer[MAXSEND];
	memset(r_buffer, 0, sizeof(r_buffer));
	//read(socket, r_buffer, sizeof(r_buffer)); 
	while(recvline(socket, r_buffer, MAXSEND) != 0);
	int len = strlen(r_buffer);
	if (r_buffer[len - 1] == '\n') 
      	r_buffer[len - 1] = '\0';
	//printf("%s\n", r_buffer);
	int r_size;
	char type[MAXNAME];
	memset(type, 0, sizeof(type));
	char source[MAXNAME];
	memset(source, 0, sizeof(source));
	char data[MAXSIZE];
	memset(data, 0, sizeof(data));
	r_size = buffer_read(r_buffer, type, source, data);
	if (!strcmp(type, "JN_ACK")){
		printf("Join %s success!\n", data);
		return 1;
	}
	else{
		if(!strcmp(type, "JN_NAK")&&r_size >0){
			printf("Create Session fail, reason: %s\n", data);
			return -1;
			
		}
		else{
			printf("Create Session error: %s, %s\n", type, source);
			return -1;
		}
	}


}



int list(const int socket)
{
	if(socket <0){
		fprintf(stderr, "Unable to query: Invalid socket\n");
		return -1;
	}
	struct message new_message;
	memset(new_message.data, 0, sizeof(new_message.data));
	memset(new_message.source, 0, sizeof(new_message.source));

	new_message.type = 12;
	new_message.size = 0;
	strcpy(new_message.source, current_user);
	char w_buffer[MAXSEND];
	memset(w_buffer, 0, sizeof(w_buffer));
	snprintf(w_buffer, sizeof(w_buffer), "QUERY 0 %s\n", new_message.source);
	if(write(socket, w_buffer, strlen(w_buffer)) <0){
		fprintf(stderr, "Error sending message\n");
		return -1;
	}
	
	
	int r_size;
	char r_buffer[MAXSEND];
	char type[MAXNAME];
	char source[MAXNAME];
	char data[MAXSIZE];
			
	memset(r_buffer, 0, sizeof(r_buffer));
	//read(socket, r_buffer, sizeof(r_buffer)); 
	while(recvline(socket, r_buffer, MAXSEND) != 0);
	int len = strlen(r_buffer);
	if (r_buffer[len - 1] == '\n') 
    	r_buffer[len - 1] = '\0';
	//fprintf(stderr,"%s\n", r_buffer);		
	memset(type, 0, sizeof(type));
	memset(source, 0, sizeof(source));
	memset(data, 0, sizeof(data));
	r_size = buffer_read(r_buffer, type, source, data);
	if (!strcmp(type, "QU_ACK")&&r_size>0){
		printf("%s\n", data);
	}
	else {
		fprintf(stderr, "query unknown error\n");
		return -1;
	}
	
}



int create_session(const int socket, const char* new_session)
{
	if (new_session ==NULL || new_session[0] == '\0') {
		fprintf(stderr, "Unable to create session: Invalid session ID\n");
		return -1;
	}
	if(socket <0){
		fprintf(stderr, "Unable to create session: Invalid socket\n");
		return -1;
	}
	struct message new_message;
	memset(new_message.data, 0, sizeof(new_message.data));
	memset(new_message.source, 0, sizeof(new_message.source));

	new_message.type = 9;
	strcpy(new_message.data, new_session);
	new_message.size = strlen(new_message.data);
	strcpy(new_message.source, current_user);
	char data_size[4];
	memset(data_size, 0, sizeof(data_size));
	sprintf(data_size, "%d", new_message.size);
	char w_buffer[MAXSEND];
	memset(w_buffer, 0, sizeof(w_buffer));
	snprintf(w_buffer, sizeof(w_buffer), "NEW_SESS %s %s %s\n", data_size, new_message.source, new_message.data);
	if(write(socket, w_buffer, strlen(w_buffer)) <0){
		fprintf(stderr, "Error sending message\n");
		return -1;
	}
	
	char r_buffer[MAXSEND];
	memset(r_buffer, 0, sizeof(r_buffer));
	//read(socket, r_buffer, sizeof(r_buffer)); 
	while(recvline(socket, r_buffer, MAXSEND) != 0);
	int len = strlen(r_buffer);
	if (r_buffer[len - 1] == '\n') 
      	r_buffer[len - 1] = '\0';
	//printf("%s\n", r_buffer);
	int r_size;
	char type[MAXNAME];
	memset(type, 0, sizeof(type));
	char source[MAXNAME];
	memset(source, 0, sizeof(source));
	char data[MAXSIZE];
	memset(data, 0, sizeof(data));
	r_size = buffer_read(r_buffer, type, source, data);
	if (!strcmp(type, "NS_ACK")){
		printf("Create Session success!\n");
		return 1;
	}
	else{
		if(!strcmp(type, "NS_NAK")&&r_size >0){
			printf("Create Session fail, reason: %s\n", data);
			return -1;
			
		}
		else{
			printf("Create Session error: %s, %s\n", type, source);
			return -1;
		}
	}
}

int connect_auth(const char *server_IP, const int server_port, const char *client_ID, const char *password) 
{
	if (server_IP ==NULL || server_IP[0] == '\0') {
		fprintf(stderr, "Unable to login: Invalid server address\n");
		return -1;
	}
	if (!(server_port >= 0 && server_port <= 65535)) {
		fprintf(stderr, "Unable to login: Invalid server port number\n");
		return -1;
	} 
	if (client_ID==NULL||client_ID[0]=='\0'){
		fprintf(stderr, "Unable to login: Invalid client_ID\n");
		return -1;
	}
	if (password==NULL||password=='\0'){
		fprintf(stderr, "Unable to login: Invalid password\n");
		return -1;
	}
	if (strlen(client_ID)>MAXNAME){
		fprintf(stderr, "client_ID too long\n");
		return -1;
	}
	if (strlen(password)>MAXPASSWORD){
		fprintf(stderr, "password too long\n");
		return -1;
	}

	// Create a socket.
	// The server is using IPv4,TCP,default protocol
	int sock;
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		fprintf(stderr, "Error creating socket\n");
		return -1;
	}
	struct sockaddr_in server;
	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(server_port);
	struct hostent *hp;
	if((hp=gethostbyname(server_IP))==NULL){
		fprintf(stderr, "Error obtaining server's address\n");
		return -1;
	}
	bcopy(hp->h_addr_list[0], (char *)&server.sin_addr, hp->h_length);

	//	Connecting to server
	if(connect(sock, (struct sockaddr *)&server, sizeof(server)) != 0){
		fprintf(stderr, "Error connecting to server\n");
		return -1;
	}
	

	// Auth
	struct message new_message;
	memset(new_message.data, 0, sizeof(new_message.data));
	memset(new_message.source, 0, sizeof(new_message.source));

	new_message.type = 1;
	strcpy(new_message.data, password);
	new_message.size = strlen(new_message.data);
	strcpy(new_message.source, client_ID);
	char data_size[4];
	memset(data_size, 0, sizeof(data_size));
	sprintf(data_size, "%d", new_message.size);
	char w_buffer[MAXSEND];
	memset(w_buffer, 0, sizeof(w_buffer));
	snprintf(w_buffer, sizeof(w_buffer), "LOGIN %s %s %s\n", data_size, new_message.source, new_message.data);
	if(write(sock, w_buffer, strlen(w_buffer)) <0){
		fprintf(stderr, "Error sending message\n");
		close(sock);
		return -1;
	}

	char r_buffer[MAXSEND];
	memset(r_buffer, 0, sizeof(r_buffer));
	//read(sock, r_buffer, sizeof(r_buffer)); 
	while(recvline(sock, r_buffer, MAXSEND) != 0);
	int len = strlen(r_buffer);
	if (r_buffer[len - 1] == '\n') 
      	r_buffer[len - 1] = '\0';
	//printf("%s\n", r_buffer);
	int r_size;
	char type[MAXNAME];
	memset(type, 0, sizeof(type));
	char source[MAXNAME];
	memset(source, 0, sizeof(source));
	char data[MAXSIZE];
	memset(data, 0, sizeof(data));
	r_size = buffer_read(r_buffer, type, source, data);
	if (!strcmp(type, "LO_ACK")){
		printf("Login success!\n");
		return sock;
	}
	else{
		if(!strcmp(type, "LO_NAK")&&r_size >0){
			printf("Login fail, reason: %s\n", data);
			close(sock);
			return -1;
			
		}
		else{
			printf("Login error: %s, %s\n", type, source);
			close(sock);
			return -1;
		}
	}
}



// return value is size of data
int buffer_read(char *r_buffer, char *type, char *source, char *data)
{
	int curser;

	for(curser =0; curser < strlen(r_buffer); curser++){
		if (r_buffer[curser] == ' ')
			break;
	}

	if(curser >= strlen(r_buffer)){
		fprintf(stderr, "Error read message1\n");
		return -1;
	}
	strncpy (type, r_buffer, curser);
	r_buffer = r_buffer+curser+1;
	
	char size[4];
	bzero((char *)&size, sizeof(size));
	for(curser =0; curser < strlen(r_buffer); curser++){
		if (r_buffer[curser] == ' ')
			break;
	}
	if(curser >= strlen(r_buffer)){
		fprintf(stderr, "Error read message2\n");
		return -1;
	}
	strncpy (size, r_buffer, curser);
	r_buffer = r_buffer+curser+1;
		
	int data_size=0;
	bool valid_size=true;
	for(int i=0; i<strlen(size); i++){
		if(!isdigit(size[i])){
			valid_size=false;
			break;
		}	
	}
	if(valid_size){						
		data_size  = strtoimax(size, NULL, 10);
		if (data_size == UINTMAX_MAX && errno == ERANGE){
			valid_size = false;
			fprintf(stderr, "Error read size from message1\n");
			return -1;
		}
	}
	else{
		fprintf(stderr, "Error read size from message\n");
		return -1;
	}	
		
	if (data_size != 0){
	
		for(curser =0; curser < strlen(r_buffer); curser++){
			if (r_buffer[curser] == ' ')
				break;
		}
		if(curser>= strlen(r_buffer)){
			fprintf(stderr, "Error read message3\n");
			return -1;
		}
		strncpy (source, r_buffer, curser);
		r_buffer = r_buffer+curser+1;
		strncpy (data, r_buffer, strlen(r_buffer));	
		//fprintf(stderr,"check: %s\n", data);
		//fprintf(stderr,"check: %d, %d\n", data_size, strlen(data));
		if(data_size!=strlen(data)){
			fprintf(stderr, "Error read data from message\n");
			return -1;
		}	
		return data_size;
	}
	else{
		strncpy (source, r_buffer, strlen(r_buffer));	
		data = NULL;
		return data_size;
	}
}


int recvline(const int sock, char *buf, const size_t buflen) {
	int status = 0; // Return status.
	size_t bufleft = buflen;
	
	while (bufleft > 1) {
		// Read one byte from scoket.
		ssize_t bytes = recv(sock, buf, 1, 0);
		if (bytes <= 0) {
			// recv() was not successful, so stop.
			status = -1;
			break;
		} else if (*buf == '\n') {
			// Found end of line, so stop.
			status = 0;
			break;
		} else {
			// Keep going.
			bufleft -= 1;
			buf += 1;
		}
	}

	return status;
}
