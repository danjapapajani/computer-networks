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
#define MAXSEND 2048
#define MAXNAME 100
#define MAXPASSWORD 100 
#define MAXSESSIONNAME 100
#define MAXQUEUE 20
#define LOGIN 1;
#define LO_ACK 2;
#define LO_NAK 3;
#define EXIT 4;
#define JOIN 5;
#define JN_ACK 6;
#define JN_NAK  7;
#define LEAVE_SESS 8;
#define NEW_SESS 9;
#define NS_ACK 10;
#define NS_NAK 11;
#define MESSAGE 12;
#define QUERY 13;
#define QU_ACK 14;

struct message{
	unsigned int type;
	unsigned int size;
	unsigned char source[MAXNAME];
	unsigned char data[MAXSIZE];
};


struct client{
	char client_ID[MAXNAME];
	char password[MAXNAME];
	bool connected;
	bool insession;
	char session_ID[MAXSESSIONNAME];
	int client_socket;
	struct client *next;
};

struct session_client_list{
	char client_ID[MAXNAME];
	struct session_client_list *next;
};

struct session{
	char session_ID[MAXSESSIONNAME];
	struct session_client_list *client_list;
	struct session *next;
};

int buffer_read(char *r_buffer, char *type, char *source, char *data);
void new_connection_handler(struct sockaddr_in client, int listen_socket);
void incoming_client_handler(struct client *incoming_client, int incoming_socket);
void create_session_handler(struct client *incoming_client, int incoming_socket, char *new_session_name);
void query_handler(int incoming_socket);
void join_session_handler(int incoming_socket, char *session_name, struct client *incoming_client);
void leave_session_handler(struct client *incoming_client);
void exit_handler(struct client *incoming_client);
void broadcast_handler (char *text, struct client *incoming_client);
struct client *client_list;
struct session *session_list;

int main (int argc, char **argv)
{
	int port;
	switch(argc){
		case 2:
			port = atoi(argv[1]);
			break;
		default:
			fprintf(stderr, "Usage: %s [port]\n", argv[0]);
			return 0;
	}

	FILE *fp;
	char *client_list_file = "client_list.txt";
	fp = fopen(client_list_file, "r");
	char cvs_line[MAXSIZE];
	memset(cvs_line, 0, sizeof(cvs_line));

	client_list = NULL;
	struct client *curr = NULL;
	session_list = NULL;

	while ((fgets(cvs_line, MAXSIZE, fp)) != NULL){
		int bytes_read = strlen(cvs_line);
		if (cvs_line[bytes_read - 1] == '\n') {
      		cvs_line[bytes_read - 1] = '\0';
      		bytes_read = bytes_read -1;
  		}

		char r_name[MAXNAME];
		char r_pwd[MAXPASSWORD];
		int count = sscanf(cvs_line, "%s %s", r_name, r_pwd);
		if (count != 2){
			fprintf(stderr, "Error reading cvs\n");
			return 0;
		}

		struct client *new_client;
		new_client = (struct client *)malloc(sizeof(struct client));
		memset(new_client->client_ID, 0, sizeof(new_client->client_ID));
		memset(new_client->password, 0, sizeof(new_client->password));
		memset(new_client->session_ID, 0, sizeof(new_client->session_ID));
		strcpy(new_client->client_ID, r_name);
		strcpy(new_client->password, r_pwd);
		new_client->connected = false;
		new_client->insession = false;
		new_client->client_socket = -1;
		new_client->next = NULL;
		

		if(client_list == NULL){
			client_list = new_client;
			curr = new_client;
		}
		else{
			curr->next = new_client;
			curr = new_client;
		}
	}
	if(client_list == NULL){
		fprintf(stderr, "Error loading cvs\n");
		return 0;
	}

//////////////////////////////////////

/*
	struct client *current = client_list;
	while(current != NULL)
	{
		printf("id: %s\n",current->client_ID);
		printf("pw: %s\n", current->password);
		current = current->next;
	}
	return 0;
*/
///////////////////////////////////////


	// Create a stream socket
	int listen_socket;
	if ((listen_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		fprintf(stderr, "Error creating socket\n");
		return 0;
	}

	// Bind an address to the socket
	struct sockaddr_in server, client;
	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	int status;
	status = bind(listen_socket, (struct sockaddr *)&server, sizeof(server));
	if (status < 0){
		fprintf(stderr, "Error binding name to socket\n");
		return 0;
	}

	// queue up to MAXQUEUE requests
	status = listen(listen_socket, MAXQUEUE);
	if (status < 0){
		fprintf(stderr, "Error listening socket\n");
		return 0;
	}
	
	
	fd_set rfds;
	int maxfd = 0;
	while(1){
	
		// Empty readable file descriptors
		FD_ZERO(&rfds);
		// Add listening socket to file descriptors
		FD_SET(listen_socket, &rfds); 
		maxfd = listen_socket;
		// Traverse client list. Add all available socket to rfds;
		struct client *current = client_list;
		while (current != NULL){
			if(current->connected == true){
				//printf("select: %s\n", current->client_ID);
				FD_SET(current->client_socket, &rfds);
				if(maxfd < current->client_socket)
					maxfd = current->client_socket;
			}
			current = current->next;
		}
		
		int retval = select(maxfd+1, &rfds, NULL, NULL, NULL); 
		if (retval == -1){
			fprintf(stderr, "Error in select\n");
			return 0;
		}
		else if (retval == 0){
        		continue;
        }
        else{
        	// Handling New Client Connection Request
        	if(FD_ISSET(listen_socket,&rfds)){
        		//printf("new conn\n");
        		new_connection_handler(client, listen_socket);
			}
			/////////////////////////////////////////////////////
			else{
			//printf("ha\n");
				// Tracerse client list, find socket trigered FD
				int incoming_socket=0;
				struct client *incoming_client=NULL;
				current = client_list;
				while(current != NULL){
					if(current->connected && FD_ISSET(current->client_socket,&rfds)){
						incoming_socket = current->client_socket;
						incoming_client = current;
						break;
					}
					current = current->next;
				}
				if(incoming_client != NULL){
					//printf("incoming: %s\n", incoming_client->client_ID);
					incoming_client_handler(incoming_client, incoming_socket);
				}
				else{
					fprintf(stderr, "Fatal: unknown socket\n");
					return 0;
				}
			}
		}
	}
	
	
return 0;

}

void incoming_client_handler(struct client *incoming_client, int incoming_socket)
{
	//printf("Start Handling\n");
	char r_buffer[MAXSEND];
	bzero((char *) &r_buffer, sizeof(r_buffer));
	int byte_read = read(incoming_socket, r_buffer, MAXSEND);
	if (byte_read < 0){
		fprintf(stderr, "Error read socket\n");
		return;
	}
	else{
		struct message new_message;
		char type[MAXNAME];
		bzero((char *)&type, sizeof(type));
		char source[MAXNAME];
		bzero((char *)&source, sizeof(source));
		char data[MAXSIZE];
		bzero((char *)&data, sizeof(data));
		int len = strlen(r_buffer);
		if (r_buffer[len - 1] == '\n') 
      		r_buffer[len - 1] = '\0';
      		
		printf("%s\n", r_buffer);
		int size = buffer_read(r_buffer, type, source, data);
		if (size < 0){
			fprintf(stderr, "Error read buffer\n");
			return;
		}
		// source and client name diff>>>WRONG!
		if(strcmp(source, incoming_client->client_ID)){
			fprintf(stderr, "Error name dont match\n");
			return;
		}
		if(!strcmp(type, "LOGIN")){
			char new_data[MAXSIZE];
			memset(new_data, 0, sizeof(new_data));
			strcpy(new_data, "This user has already connected to this server!");
			int new_size = strlen(new_data);
			char new_data_size[4];
			memset(new_data_size, 0, sizeof(new_data_size));
			sprintf(new_data_size, "%d", new_size);
			char w_buffer[MAXSEND];
			memset(w_buffer, 0, sizeof(w_buffer));
			snprintf(w_buffer, sizeof(w_buffer), "LO_NAK %s SERVER %s\n", new_data_size, new_data);
			while(write(incoming_socket, w_buffer, strlen(w_buffer)) <= 0);
			return;
		}
		else if(!strcmp(type, "EXIT")){
			leave_session_handler(incoming_client);
			exit_handler(incoming_client);
			return;
		}
		else if(!strcmp(type, "JOIN")){
			join_session_handler(incoming_socket, data, incoming_client);
			return;
		}
		else if(!strcmp(type, "LEAVE_SESS")){
			leave_session_handler(incoming_client);
			return;
		}
		else if(!strcmp(type, "NEW_SESS")){
			create_session_handler(incoming_client, incoming_socket, data);
			return;
		}
		else if(!strcmp(type, "QUERY")){
			query_handler(incoming_socket);
			return;
		}
		else if(!strcmp(type, "MESSAGE")){
			broadcast_handler(data, incoming_client);
			return;
		}
		else{
			fprintf(stderr, "Error cmd\n");
			return;
		}	
	}
	
}


void broadcast_handler (char *text, struct client *incoming_client)
{	
	
	char session_name[MAXSESSIONNAME];
	memset(session_name, 0, sizeof(session_name));
	strcpy(session_name, incoming_client->session_ID);
	struct client *curr_client = client_list;
	while(curr_client != NULL){
		if(curr_client->insession && strcmp(curr_client->session_ID, session_name)==0 && strcmp(curr_client->client_ID, incoming_client->client_ID) !=0){
			char new_data[MAXSIZE];
			memset(new_data, 0, sizeof(new_data));
			snprintf(new_data, sizeof(new_data), "%s", text);
			int new_size = strlen(new_data);
			char new_data_size[4];
			memset(new_data_size, 0, sizeof(new_data_size));
			sprintf(new_data_size, "%d", new_size);
			char w_buffer[MAXSEND];
			memset(w_buffer, 0, sizeof(w_buffer));
			snprintf(w_buffer, sizeof(w_buffer), "MESSAGE %s %s %s\n", new_data_size, incoming_client->client_ID, new_data);
			printf("write: %d, %s to %s\n", curr_client->client_socket, w_buffer, curr_client->client_ID);
			while(write(curr_client->client_socket, w_buffer, strlen(w_buffer)) <= 0);
		}
		curr_client = curr_client->next;
	}
	return;
	
}
				
				
			


void exit_handler(struct client *incoming_client)
{
	incoming_client->connected = false;
	close(incoming_client->client_socket);
	return;

}


void leave_session_handler(struct client *incoming_client)
{
	if(!incoming_client->insession){
		return;
	}
	char session_name[MAXSESSIONNAME];
	memset(session_name, 0, sizeof(session_name));
	strcpy(session_name, incoming_client->session_ID);
	if(session_list != NULL){
		struct session *current_session = session_list;
		struct session *previous_session = NULL;
		while(current_session != NULL){
			if(!strcmp(current_session->session_ID, session_name)){
				// find session looking for, handling
				incoming_client->insession = false;
				memset(incoming_client->session_ID, 0, sizeof(incoming_client->session_ID));
				struct session_client_list *curr_node = current_session->client_list;
				struct session_client_list *prev_node = NULL;
				while(curr_node != NULL){
					if(!strcmp(curr_node->client_ID, incoming_client->client_ID))
						break;		
					else{	
						prev_node = curr_node;
						curr_node = curr_node->next;
					}
				} 
				//del first
				if(prev_node == NULL && curr_node!= NULL){
					current_session->client_list = current_session->client_list->next;
					free(curr_node);
				}
				// del other
				else if(prev_node != NULL && curr_node != NULL){
					prev_node->next = curr_node->next;
					free(curr_node);
				}
				break;
			}
			else{
				previous_session = current_session;
				current_session = current_session->next;
			}
		}
		
		// if session_client_list empty, delete session
		if(current_session != NULL && current_session->client_list == NULL){
			// del first
			if(previous_session == NULL){
				session_list = session_list->next;
				free(current_session);
			}
			// del other
			else {
				previous_session->next = current_session->next;
				free(current_session);
			}
		}
		return;		
	}
	return;
}


void join_session_handler(int incoming_socket, char *session_name, struct client *incoming_client)
{

	if(incoming_client->insession){
		// not allowed to join multiple session, handling 
		char new_data[MAXSIZE];
		memset(new_data, 0, sizeof(new_data));
		strcpy(new_data, "Not allowed to join multiple session!");
		int new_size = strlen(new_data);
		char new_data_size[4];
		memset(new_data_size, 0, sizeof(new_data_size));
		sprintf(new_data_size, "%d", new_size);
		char w_buffer[MAXSEND];
		memset(w_buffer, 0, sizeof(w_buffer));
		snprintf(w_buffer, sizeof(w_buffer), "JN_NAK %s SERVER %s\n", new_data_size, new_data);
		while(write(incoming_socket, w_buffer, strlen(w_buffer)) <= 0);
		return;
	}
	if(session_list != NULL){
		// check for existing name
		struct session *current_session = session_list;
		while(current_session != NULL){
			if(!strcmp(current_session->session_ID, session_name)){
				// find session looking for, handling
				incoming_client->insession = true;
				strcpy(incoming_client->session_ID, current_session->session_ID);
				struct session_client_list *curr_node = current_session->client_list;
				while(curr_node->next != NULL){
					curr_node = curr_node->next;
				} 
				struct session_client_list *new= (struct session_client_list *)malloc(sizeof(struct session_client_list));
				memset(new->client_ID, 0, sizeof(new->client_ID));
				new->next = NULL;
				strcpy(new->client_ID, incoming_client->client_ID);
				curr_node->next = new;
								
				char new_data[MAXSIZE];
				memset(new_data, 0, sizeof(new_data));
				snprintf(new_data, sizeof(new_data), "%s", session_name);
				int new_size = strlen(new_data);
				char new_data_size[4];
				memset(new_data_size, 0, sizeof(new_data_size));
				sprintf(new_data_size, "%d", new_size);
				char w_buffer[MAXSEND];
				memset(w_buffer, 0, sizeof(w_buffer));
				snprintf(w_buffer, sizeof(w_buffer), "JN_ACK %s SERVER %s\n", new_data_size, new_data);
				while(write(incoming_socket, w_buffer, strlen(w_buffer)) <= 0);
				return;
			}
			else{
				current_session = current_session->next;
			}
		}
		char new_data[MAXSIZE];
		memset(new_data, 0, sizeof(new_data));
		snprintf(new_data, sizeof(new_data), "Unable to find %s, Session not exist", session_name);
		int new_size = strlen(new_data);
		char new_data_size[4];
		memset(new_data_size, 0, sizeof(new_data_size));
		sprintf(new_data_size, "%d", new_size);
		char w_buffer[MAXSEND];
		memset(w_buffer, 0, sizeof(w_buffer));
		snprintf(w_buffer, sizeof(w_buffer), "JN_NAK %s SERVER %s\n", new_data_size, new_data);
		while(write(incoming_socket, w_buffer, strlen(w_buffer)) <= 0);
		return;
	}
	char new_data[MAXSIZE];
	memset(new_data, 0, sizeof(new_data));
	snprintf(new_data, sizeof(new_data), "Unable to find %s, Session not exist", session_name);
	int new_size = strlen(new_data);
	char new_data_size[4];
	memset(new_data_size, 0, sizeof(new_data_size));
	sprintf(new_data_size, "%d", new_size);
	char w_buffer[MAXSEND];
	memset(w_buffer, 0, sizeof(w_buffer));
	snprintf(w_buffer, sizeof(w_buffer), "JN_NAK %s SERVER %s\n", new_data_size, new_data);
	while(write(incoming_socket, w_buffer, strlen(w_buffer)) <= 0);
	return;

}


void query_handler(int incoming_socket)
{
	struct client *current = client_list;
	char result[MAXSIZE];
	memset(result, 0, sizeof(result));
	strcpy(result, "Online users:");
	char result2[MAXSIZE];
	memset(result2, 0, sizeof(result2));
	strcpy(result2, "Available sessions:");

	while(current != NULL){
		if(current->connected){
			strcat(result, current->client_ID);
			strcat(result, "|");
		}
		current = current->next;
	}
	
	struct session *current_session = session_list;
	while(current_session != NULL){
		strcat(result2, current_session->session_ID);
		strcat(result2, "|");
		current_session =  current_session->next;
	}
	
	strcat(result, result2);
	
	int new_size = strlen(result);
	char new_data_size[4];
	memset(new_data_size, 0, sizeof(new_data_size));
	sprintf(new_data_size, "%d", new_size);
	char w_buffer[MAXSEND];
	memset(w_buffer, 0, sizeof(w_buffer));
	snprintf(w_buffer, sizeof(w_buffer), "QU_ACK %s SERVER %s\n", new_data_size, result);
	while(write(incoming_socket, w_buffer, strlen(w_buffer)) <= 0);
	return;
}
	


void create_session_handler(struct client *incoming_client, int incoming_socket, char *new_session_name)
{
	if(session_list != NULL){
		// check for existing name
		struct session *current_session = session_list;
		while(current_session != NULL){
			if(!strcmp(current_session->session_ID, new_session_name)){
				// deplicate name, handling
				char new_data[MAXSIZE];
				memset(new_data, 0, sizeof(new_data));
				strcpy(new_data, "Duplicate session name error!");
				int new_size = strlen(new_data);
				char new_data_size[4];
				memset(new_data_size, 0, sizeof(new_data_size));
				sprintf(new_data_size, "%d", new_size);
				char w_buffer[MAXSEND];
				memset(w_buffer, 0, sizeof(w_buffer));
				snprintf(w_buffer, sizeof(w_buffer), "NS_NAK %s SERVER %s\n", new_data_size, new_data);
				while(write(incoming_socket, w_buffer, strlen(w_buffer)) <= 0);
				return;
			}
			else{
				current_session = current_session->next;
			}
		}
		
	}
	
	// name good
	struct session *new_session = (struct session *)malloc(sizeof(struct session));
	memset(new_session->session_ID, 0, sizeof(new_session->session_ID));
	strcpy(new_session->session_ID, new_session_name);
	new_session->next = NULL;
	new_session->client_list = (struct session_client_list *)malloc(sizeof(struct session_client_list));
	memset(new_session->client_list->client_ID, 0, sizeof(new_session->client_list->client_ID));
	strcpy(new_session->client_list->client_ID, incoming_client->client_ID);
	new_session->client_list->next = NULL;
	
	//
	incoming_client->insession = true;
	strcpy(incoming_client->session_ID, new_session->session_ID);
	
	//
	if (session_list==NULL){
		session_list = new_session;
	}
	else{
		struct session *current_session = session_list;
		while(current_session->next != NULL){
			current_session = current_session->next;
		}
		current_session->next = new_session;
	}
	
	// new session done, handling
	
	char w_buffer[MAXSEND];
	memset(w_buffer, 0, sizeof(w_buffer));
	snprintf(w_buffer, sizeof(w_buffer), "NS_ACK 0 SERVER\n");
	while(write(incoming_socket, w_buffer, strlen(w_buffer)) <= 0);
	return;
}
		

void new_connection_handler(struct sockaddr_in client, int listen_socket)
{
	int client_len = sizeof(client);
    int new_sock;
	if((new_sock = accept(listen_socket, (struct sockaddr *)&client, &client_len)) < 0){
		fprintf(stderr, "Error accepting client\n");
		return;
	}
	char r_buffer[MAXSEND];
	bzero((char *) &r_buffer, sizeof(r_buffer));
	int byte_read = read(new_sock, r_buffer, MAXSEND);
	if (byte_read < 0){
		fprintf(stderr, "Error read socket\n");
		close(new_sock);
		return;
	}
	else{
		struct message new_message;
		char type[MAXNAME];
		bzero((char *)&type, sizeof(type));
		char source[MAXNAME];
		bzero((char *)&source, sizeof(source));
		char data[MAXSIZE];
		bzero((char *)&data, sizeof(data));
		int len = strlen(r_buffer);
		if (r_buffer[len - 1] == '\n') 
      		r_buffer[len - 1] = '\0';
      		
		printf("%s\n", r_buffer);
		int size = buffer_read(r_buffer, type, source, data);
		if (size <= 0){
			close(new_sock);
			return;
		}
		if(!strcmp(type, "LOGIN")){
			new_message.type = LOGIN;
			new_message.size = size;
			strcpy(new_message.source, source);
			strcpy(new_message.data, data);
			
			struct client *current = client_list;
			while(current != NULL){
				if (!strcmp(current->client_ID, source)){
					if(!strcmp(current->password, data)){
						//printf("%s, %s\n", current->client_ID, current->password);
						if(!current->connected){
							current->connected = true;
							current->client_socket = new_sock;
							char w_buffer[MAXSEND];
							memset(w_buffer, 0, sizeof(w_buffer));
							strcpy(w_buffer, "LO_ACK 0 SERVER\n");
							while(write(current->client_socket, w_buffer, strlen(w_buffer)) <= 0);
							printf("Login from %s\n", current->client_ID);
							return;
						}
						else{
							char new_data[MAXSIZE];
							memset(new_data, 0, sizeof(new_data));
							strcpy(new_data, "This user has already connected to this server!");
							int new_size = strlen(new_data);
							char new_data_size[4];
							memset(new_data_size, 0, sizeof(new_data_size));
							sprintf(new_data_size, "%d", new_size);
							char w_buffer[MAXSEND];
							memset(w_buffer, 0, sizeof(w_buffer));
							snprintf(w_buffer, sizeof(w_buffer), "LO_NAK %s SERVER %s\n", new_data_size, new_data);
							while(write(new_sock, w_buffer, strlen(w_buffer)) <= 0);
							return;
						}
					}
					else{
						char new_data[MAXSIZE];
						memset(new_data, 0, sizeof(new_data));
						strcpy(new_data, "Invalid Password!");
						int new_size = strlen(new_data);
						char new_data_size[4];
						memset(new_data_size, 0, sizeof(new_data_size));
						sprintf(new_data_size, "%d", new_size);
						char w_buffer[MAXSEND];
						memset(w_buffer, 0, sizeof(w_buffer));
						snprintf(w_buffer, sizeof(w_buffer), "LO_NAK %s SERVER %s\n", new_data_size, new_data);
						while(write(new_sock, w_buffer, strlen(w_buffer)) <= 0);
						return;
					}
				}
				current = current->next;
			}
			
			// No user name can be found
			char new_data[MAXSIZE];
			memset(new_data, 0, sizeof(new_data));
			strcpy(new_data, "Invalid Username!");
			int new_size = strlen(new_data);
			char new_data_size[4];
			memset(new_data_size, 0, sizeof(new_data_size));
			sprintf(new_data_size, "%d", new_size);
			char w_buffer[MAXSEND];
			memset(w_buffer, 0, sizeof(w_buffer));
			snprintf(w_buffer, sizeof(w_buffer), "LO_NAK %s SERVER %s\n", new_data_size, new_data);
			while(write(new_sock, w_buffer, strlen(w_buffer)) <= 0);
			return;
		}				
		else{
			fprintf(stderr, "Error cmd\n");
			close(new_sock);
			return;
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
