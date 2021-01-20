/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: papajan6
 *
 * Created on November 3, 2018, 4:32 PM
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <regex.h>

#define BUF 1100
#define DATA 1000

/********************************************
 **************** tagPacket *****************
 ********************************************/

typedef struct tagPacket {
    
    unsigned int fragNum;
    unsigned int size;
    unsigned int totalFrag;
    char *filename;
    char filedata[DATA]; 
    
} Packet;

/********************************************
 ************ convertString ****************
 ********************************************/

void convertString(const Packet *packet, char *result) {
    
    int i = 0;
    
    /*
     * initalize string buffer 
     * then load data into the strings
     */
    
    memset(result, 0, BUF);

    sprintf(result, "%d", packet -> totalFrag);
    i = strlen(result);
    memcpy(result + i, ":", sizeof(char));
    ++i;
    
    sprintf(result + i, "%d", packet -> fragNum);
    i = strlen(result);
    memcpy(result + i, ":", sizeof(char));
    ++i;

    sprintf(result + i, "%d", packet -> size);
    i = strlen(result);
    memcpy(result + i, ":", sizeof(char));
    ++i;

    sprintf(result + i, "%s", packet -> filename);
    i += strlen(packet -> filename);
    memcpy(result + i, ":", sizeof(char));
    ++i;

    memcpy(result + i, packet -> filedata, sizeof(char) * DATA);


}

/********************************************
 ************* convertPacket ***************
 ********************************************/
void convertPacket(const char* word, Packet *packet) {

    // use regex to find colon
    regex_t colon;
    regmatch_t find[1];
    int i = 0;
    char buf[BUF];
    
    regcomp(&colon, "[:]", REG_EXTENDED);
    regexec(&colon, word + i, 1, find, REG_NOTBOL);
    
    memset(buf, 0, BUF * sizeof(char));
    memcpy(buf, word + i, find[0].rm_so);
    packet -> totalFrag = atoi(buf);
    i += (find[0].rm_so + 1);

    // Match fragNum
    regexec(&colon, word + i, 1, find, REG_NOTBOL);
    memset(buf, 0,  BUF * sizeof(char));
    memcpy(buf, word + i, find[0].rm_so);
    packet -> fragNum = atoi(buf);
    i += (find[0].rm_so + 1);

    // Match size
    regexec(&colon, word + i, 1, find, REG_NOTBOL);
    memset(buf, 0, BUF * sizeof(char));
    memcpy(buf, word + i, find[0].rm_so);
    packet -> size = atoi(buf);
    i += (find[0].rm_so + 1);

    // Match filename
    regexec(&colon, word + i, 1, find, REG_NOTBOL);
    memcpy(packet -> filename, word + i, find[0].rm_so);
    packet -> filename[find[0].rm_so] = 0;
    i += (find[0].rm_so + 1);
    
    // Match filedata
    memcpy(packet -> filedata, word + i, packet -> size);

}

/********************************************
 ************ printPacket *******************
 ********************************************/
void printPacket(Packet* packet) {
    
    char data[DATA + 1] = {0};
    printf("%s", data);
    printf("totalFrag = %d\n", packet->totalFrag);
    printf("fragNum = %d\n", packet->fragNum);
    printf("size = %d\n", packet->size);
    printf("filename = %s\n",packet->filename);
    memcpy(data, packet->filedata, DATA);
    
}


/********************************************
 ******************* main *******************
 ********************************************/
int main(int argc, char const *argv[])
{
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int sockfd;
    char* port;
    char buf[BUF];
    struct sockaddr_in theirAddr; 
    socklen_t addrLen = sizeof(theirAddr);
    char filename[BUF];
    Packet packet;
    packet.filename = (char *) malloc(BUF);
    FILE* incomingFile = NULL;
    bool* recievedFrags = NULL;
    
    if (argc != 2) { 
        printf("usage: server -<UDP listen port>\n");
        exit(0);
    }
    
    port = argv[1];

    // open socket (DGRAM)
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; 

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    
    // recvfrom the client and store info in theirAddr so as to send back later
    if (recvfrom(sockfd, buf, BUF, 0, (struct sockaddr *) &theirAddr, &addrLen) == -1) {
        printf("recvfrom error\n");
        exit(1);
    }

    // send message back to client based on message recevied
    if (strcmp(buf, "ftp") == 0) {
        if ((sendto(sockfd, "yes", strlen("yes"), 0, (struct sockaddr *) &theirAddr, addrLen)) == -1) {
            printf("sendto error1\n");
            exit(1);
        }
    } else {
        if ((sendto(sockfd, "no", strlen("no"), 0, (struct sockaddr *) &theirAddr, addrLen)) == -1) {
            printf("sendto error\n");
            exit(1);
        }
    }

    packet.filename = (char *) malloc(BUF);
    
    while(1) {
        
        //waiting for datagram
        
        if (recvfrom(sockfd, buf, BUF, 0, (struct sockaddr *) &theirAddr, &addrLen) == -1) {
            printf("recvfrom error\n");
            exit(1);
        }
        
        convertPacket(buf, &packet);
        
        if (!incomingFile) {
            
            strcpy(filename, packet.filename);	
            
            while (access(filename, F_OK) == 0) {
                
                char *findPeriod = strrchr(filename, '.'); //returns pointer to last occurence of period
                
                if(findPeriod == NULL){
                    
                    strcat(filename, " copy");
                
                }
                else if(findPeriod != NULL) {
                    
                    char suffix[BUF + 1] = {0};
                    strncpy(suffix, findPeriod, BUF - 1);
                    *findPeriod = '\0';
                    strcat(filename, " copy");
                    strcat(filename, suffix);

                }
            } 
            
            incomingFile = fopen(filename, "w");
        }
        
        if (!recievedFrags) {
            
            recievedFrags = (bool *) malloc(packet.totalFrag * sizeof(recievedFrags));
            
            for (int i = 0; i < packet.totalFrag; i++) {
                recievedFrags[i] = false;
            }
        }
        
        if (!recievedFrags[packet.fragNum - 1]) {
            
            int numbyte = fwrite(packet.filedata, sizeof(char), packet.size, incomingFile);
            if (numbyte != packet.size) {
                printf("fwrite error\n");
                exit(1);
            } 
            
            recievedFrags[packet.fragNum - 1] = true;
        }
        
        strcpy(packet.filedata, "ACK");

        convertString(&packet, buf);
        if ((sendto(sockfd, buf, BUF, 0, (struct sockaddr *) &theirAddr, sizeof(theirAddr))) == -1) {
            printf("sendto error\n");
            exit(1);
        }
        
        if (packet.fragNum == packet.totalFrag) {
            bool done = true;
            
            for(int j = 0; j < packet.totalFrag; j++){
                done &= recievedFrags[j];
            }
            
            if(done) break;
        }
    }
    
    // wait for FIN message
    struct timeval timeout;
    timeout.tv_usec = 999999;
    timeout.tv_sec = 1;
    
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "setsockopt failed\n");
    }
    
    for (;;) {
        if (recvfrom(sockfd, buf, BUF, 0, (struct sockaddr *) &theirAddr, &addrLen) == -1) {
            printf("TIMEOUT while waiting for FIN message\n");
            break;
        }
        
        convertPacket(buf, &packet);	
        
        if (strcmp(packet.filedata, "FIN") == 0) { 
            printf("File %s transfer completed\n", filename);
            break;
        } else {
            
            strcpy(packet.filedata, "ACK");
            convertString(&packet, buf);
            
            if ((sendto(sockfd, buf, BUF, 0, (struct sockaddr *) &theirAddr, sizeof(theirAddr))) == -1) {
                printf("sendto error\n");
                exit(1);
            }
        } 
    }

    close(sockfd);
    fclose(incomingFile);
    free(recievedFrags);
    free(packet.filename);
    return 0;
}