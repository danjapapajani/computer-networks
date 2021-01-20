/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   packet.h
 * Author: papajan6
 *
 * Created on October 18, 2018, 2:23 PM
 */

#include <stdio.h>
#include <string.h> 
#include <stdlib.h>
#include <regex.h>
#define BUF 1100
#define DATA 1000

typedef struct tagPacket {
	unsigned int totalFrag;
	unsigned int fragNum;
	unsigned int size;
	char *filename;
	char filedata[DATA]; 
} Packet;
void packetToString(const Packet *packet, char *result) {
    
    // Initialize string buffer
    memset(result, 0, BUF);

    // Load data into string
    int index = 0;
    sprintf(result, "%d", packet -> totalFrag);
    index = strlen(result);
    memcpy(result + index, ":", sizeof(char));
    ++index;
    
    sprintf(result + index, "%d", packet -> fragNum);
    index = strlen(result);
    memcpy(result + index, ":", sizeof(char));
    ++index;

    sprintf(result + index, "%d", packet -> size);
    index = strlen(result);
    memcpy(result + index, ":", sizeof(char));
    ++index;

    sprintf(result + index, "%s", packet -> filename);
    index += strlen(packet -> filename);
    memcpy(result + index, ":", sizeof(char));
    ++index;

    memcpy(result + index, packet -> filedata, sizeof(char) * DATA);


}

void stringToPacket(const char* str, Packet *packet) {

    // Compile Regex to match ":"
    regex_t regex;
    if(regcomp(&regex, "[:]", REG_EXTENDED)) {
        fprintf(stderr, "Could not compile regex\n");
    }

    // Match regex to find ":" 
    regmatch_t pmatch[1];
    int index = 0;
    char buf[BUF];

    // Match totalFrag
    if(regexec(&regex, str + index, 1, pmatch, REG_NOTBOL)) {
        fprintf(stderr, "Error matching regex\n");
        exit(1);
    }
    memset(buf, 0, BUF * sizeof(char));
    memcpy(buf, str + index, pmatch[0].rm_so);
    packet -> totalFrag = atoi(buf);
    index += (pmatch[0].rm_so + 1);

    // Match fragNum
    if(regexec(&regex, str + index, 1, pmatch, REG_NOTBOL)) {
        fprintf(stderr, "Error matching regex\n");
        exit(1);
    }
    memset(buf, 0,  BUF * sizeof(char));
    memcpy(buf, str + index, pmatch[0].rm_so);
    packet -> fragNum = atoi(buf);
    index += (pmatch[0].rm_so + 1);

    // Match size
    if(regexec(&regex, str + index, 1, pmatch, REG_NOTBOL)) {
        fprintf(stderr, "Error matching regex\n");
        exit(1);
    }
    memset(buf, 0, BUF * sizeof(char));
    memcpy(buf, str + index, pmatch[0].rm_so);
    packet -> size = atoi(buf);
    index += (pmatch[0].rm_so + 1);

    // Match filename
    if(regexec(&regex, str + index, 1, pmatch, REG_NOTBOL)) {
        fprintf(stderr, "Error matching regex\n");
        exit(1);
    }


    memcpy(packet -> filename, str + index, pmatch[0].rm_so);
    packet -> filename[pmatch[0].rm_so] = 0;
    index += (pmatch[0].rm_so + 1);
    
    // Match filedata
    memcpy(packet -> filedata, str + index, packet -> size);

}

void printPacket(Packet* packet) {
    printf("totalFrag = %d,\n fragNum = %d, size = %d, filename = %s\n", packet->totalFrag, packet->fragNum, packet->size, packet->filename);
    char data[DATA + 1] = {0};
    memcpy(data, packet->filedata, DATA);
    printf("%s", data);
}

