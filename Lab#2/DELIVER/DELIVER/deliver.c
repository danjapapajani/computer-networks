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
#include <string.h> 
#include <stdlib.h> 
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include "packet.h"
#define ALIVE 32


// Convert packet to string

void send_file(char *filename, int sockfd, struct sockaddr_in serv_addr) {

    // Open file
    FILE *fp;
    if((fp = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "Can't open input file %s\n", filename);
        exit(1);
    }
    // printf("Successfully opened file %s\n", filename);


    // Calculate total number of packets required
    fseek(fp, 0, SEEK_END);
    int totalFrag = ftell(fp) / 1000 + 1;
    printf("Number of packets: %d\n", totalFrag);
    rewind(fp); 


    // Read file into packets, and save in packets array
    char rec_buf[BUF];                                 // Buffer for receiving packets
    char **packets = malloc(sizeof(char*) * totalFrag);    // Stores packets for retransmitting

    for(int packetNum = 1; packetNum <= totalFrag; ++packetNum) {

        // printf("Preparing packet #%d / %d...\n", packetNum, totalFrag);

        // Create Packet
        Packet packet;
        memset(packet.filedata, 0, sizeof(char) * (DATA));
        fread((void*)packet.filedata, sizeof(char), DATA, fp);

        // Update packet info
        packet.totalFrag = totalFrag;
        packet.fragNum = packetNum;
        packet.filename = filename;
        if(packetNum != totalFrag) {
            // This packet is not the last packet
            packet.size = DATA;
        }
        else {
            // This packet is the last packet
            fseek(fp, 0, SEEK_END);
            packet.size = (ftell(fp) - 1) % 1000 + 1;
        }

        // Save packet to packets array
        packets[packetNum - 1] = malloc(BUF * sizeof(char));
        packetToString(&packet, packets[packetNum - 1]);

    }    


    // Send packets and receive acknowledgements

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "setsockopt failed\n");
    }
    int timesent = 0;   // Number of times a packet is sent
    socklen_t serv_addr_size = sizeof(serv_addr);

    Packet ack_packet;  // Packet receiveds
    ack_packet.filename = (char *)malloc(BUF * sizeof(char));

    for(int packetNum = 1; packetNum <= totalFrag; ++packetNum) {

        int numbytes;       // Return value of sendto and recvfrom
        ++timesent;

        // Send packets
        // printf("Sending packet #%d / %d...\n", packetNum, totalFrag);
        if((numbytes = sendto(sockfd, packets[packetNum - 1], BUF, 0 , (struct sockaddr *) &serv_addr, sizeof(serv_addr))) == -1) {
            fprintf(stderr, "sendto error for packet #%d\n", packetNum);
            exit(1);
        }

        // Receive acknowledgements
        memset(rec_buf, 0, sizeof(char) * BUF);
        if((numbytes = recvfrom(sockfd, rec_buf, BUF, 0, (struct sockaddr *) &serv_addr, &serv_addr_size)) == -1) {
            // Resend if timeout
            fprintf(stderr, "Timeout or recvfrom error for ACK packet #%d, resending attempt #%d...\n", packetNum--, timesent);
            if(timesent < ALIVE){
                continue;
            }
            else {
                fprintf(stderr, "Too many resends. File transfer terminated.\n");
                exit(1);
            }
        }
        
        stringToPacket(rec_buf, &ack_packet);
        
        // Check contents of ACK packets
        if(strcmp(ack_packet.filename, filename) == 0) {
            if(ack_packet.fragNum == packetNum) {
                if(strcmp(ack_packet.filedata, "ACK") == 0) {
                    // printf("ACK packet #%d received\n", packetNum);
                    timesent = 0;
                    continue;
                }
            }
        }

        // Resend packet
        fprintf(stderr, "ACK packet #%d not received, resending attempt #%d...\n", packetNum, timesent);
        --packetNum;

    }
    

    // Free memory
    for(int packetNum = 1; packetNum <= totalFrag; ++packetNum) {
        free(packets[packetNum - 1]);
    }
    free(packets);

    
    free(ack_packet.filename);

}



int main(int argc, char const *argv[])
{
    struct addrinfo hints, *servinfo, *p, *fake;
    int sockfd;
    int rv;
     
    if (argc != 3) {
        fprintf(stdout, "usage: deliver -<server address> -<server port number>\n");
        exit(0);
    }
    
  char* port = argv[2];
  
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(argv[1], port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }
 
    struct sockaddr_in serv_addr;
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
 
    char buf[BUF] = {0};
    char filename[BUF] = {0};

    // get user input
    fgets(buf, BUF, stdin);

    int cursor = 0;
    if(tolower(buf[cursor]) == 'f' && tolower(buf[cursor + 1]) == 't' && tolower(buf[cursor + 2]) == 'p') {
        cursor += 3;
        while (buf[cursor] == ' ') { // skip whitespaces inbetween
            cursor++;
        }
        char *token = strtok(buf + cursor, "\r\t\n ");
        strncpy(filename, token, BUF);
    } else {
        fprintf(stderr, "Starter \"ftp\" undeteceted.\n");
        exit(1);
    }
    // Eligibility check of the file  
    if(access(filename, F_OK) == -1) {
        fprintf(stderr, "File \"%s\" doesn't exist.\n", filename);
        exit(1);
    }

    int numbytes;
    socklen_t serv_addr_size = sizeof(serv_addr);
    // send the message
  
     if ((numbytes = sendto(sockfd, "ftp", strlen("ftp") , 0 , p->ai_addr , p->ai_addrlen)) == -1) {
        fprintf(stderr, "sendto error\n");
        exit(1);
    }
    
    memset(buf, 0, BUF); // clean the buffer
    if((numbytes = recvfrom(sockfd, buf, BUF, 0, (struct sockaddr *) &serv_addr, &serv_addr_size)) == -1) {
        fprintf(stderr, "recvfrom error\n");
        exit(1);
    }
 
/*

    if(strcmp(buf, "yes") == 0) {
        fprintf(stdout, "A file transfer can start\n");
    }
    else {
        fprintf(stderr, "Error: File transfer can't start\n");
    }
*/


    // Begin sending file and check for acknowledgements
    send_file(filename, sockfd, serv_addr);
    
    // Sending Completed
    close(sockfd);
    printf("File Transfer Completed, SocketClosed.\n");
    
    return 0;
}