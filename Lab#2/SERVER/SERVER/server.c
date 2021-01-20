/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: papajan6
 *
 * Created on October 18, 2018, 1:29 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include "packet.h"

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

    Packet packet;
    packet.filename = (char *) malloc(BUF);
    FILE* pFile = NULL;
    bool* fragRecv = NULL;
    
    for(;;) {
        
        //waiting for datagram
        
        if (recvfrom(sockfd, buf, BUF, 0, (struct sockaddr *) &theirAddr, &addrLen) == -1) {
            printf("recvfrom error\n");
            exit(1);
        }
        
        stringToPacket(buf, &packet);
        
        if (!pFile) {
            
            strcpy(filename, packet.filename);	
            
            while (access(filename, F_OK) == 0) {
                
                char *pSuffix = strrchr(filename, '.'); //returns pointer to last occurence of period
                char suffix[BUF + 1] = {0};
                strncpy(suffix, pSuffix, BUF - 1);
                *pSuffix = '\0';
                strcat(filename, " copy");
                strcat(filename, suffix);
            } 
            
            pFile = fopen(filename, "w");
        }
        
        if (!fragRecv) {
            
            fragRecv = (bool *) malloc(packet.totalFrag * sizeof(fragRecv));
            
            for (int i = 0; i < packet.totalFrag; i++) {
                fragRecv[i] = false;
            }
        }
        
        if (!fragRecv[packet.fragNum]) {
            
            int numbyte = fwrite(packet.filedata, sizeof(char), packet.size, pFile);
            if (numbyte != packet.size) {
                printf("fwrite error\n");
                exit(1);
            } 
            
            fragRecv[packet.fragNum] = true;
        }
        
        strcpy(packet.filedata, "ACK");

        packetToString(&packet, buf);
        if ((sendto(sockfd, buf, BUF, 0, (struct sockaddr *) &theirAddr, sizeof(theirAddr))) == -1) {
            printf("sendto error\n");
            exit(1);
        }
        
        if (packet.fragNum == packet.totalFrag) {
            printf("File %s transfer completed\n", filename);
            break;
        }
    }

    close(sockfd);
    fclose(pFile);
    free(fragRecv);
    free(packet.filename);
    return 0;
}