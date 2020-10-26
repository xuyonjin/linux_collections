/*
 * Copyright © 2008-2014 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the BSD License.
 */
#define __USE_MISC
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <modbus/modbus.h>

#define NB_CONNECTION    64




static void close_sigint(int dummy)
{

    exit(dummy);
}



int is_valid_ip(char *ip_address) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ip_address, &(sa.sin_addr));
    return result != 0;
}
void *main1(void *argv);

int main(int argc, char **argv) 
{
    
    int i,j,ret[10];
    pthread_t id[10];
        /* For reading registers and bits, the addesses go from 0 to 0xFFFF */
    i = 0;
    j = 1;
    ret[0] =  pthread_create(&id[0],NULL,main1,(void*)&i);    
    ret[1] =  pthread_create(&id[1],NULL,main1,(void*)&j);        
    while(1)sleep(10);
    return 0;
}

void * main1(void *argv) {
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    int master_socket;
    int rc;
    fd_set refset;
    fd_set rdset;
    /* Maximum file descriptor number */
    int fdmax;
modbus_t *ctx = NULL;
int server_socket = -1;
modbus_mapping_t *mb_mapping;
    /* Getting the options through getopt */
    int c;
    char *ip_addr = NULL;
    char *port_s = NULL;
    int port;

    mb_mapping = modbus_mapping_new(500, 0,
                                    500, 0);

    c = *((int *)argv);
    printf("c = %d\n",c);
    if(c == 0){
        ctx = modbus_new_tcp("192.168.23.131", 1502);
        printf("192.168.23.131\n");
    }
    if(c == 1){
        ctx = modbus_new_tcp("192.168.23.143", 1502);
        printf("192.168.23.143\n");
    }    



    server_socket = modbus_tcp_listen(ctx, NB_CONNECTION);

    signal(SIGINT, close_sigint);

    /* Clear the reference set of socket */
    FD_ZERO(&refset);
    /* Add the server socket */
    FD_SET(server_socket, &refset);

    /* Keep track of the max file descriptor */
    fdmax = server_socket;

    for (;;) {
        rdset = refset;
        if (select(fdmax+1, &rdset, NULL, NULL, NULL) == -1) {
            perror("Server select() failure.");
            close_sigint(1);
        }

        /* Run through the existing connections looking for data to be
         * read */
        for (master_socket = 0; master_socket <= fdmax; master_socket++) {

            if (!FD_ISSET(master_socket, &rdset)) {
                continue;
            }

            if (master_socket == server_socket) {
                /* A client is asking a new connection */
                socklen_t addrlen;
                struct sockaddr_in clientaddr;
                int newfd;

                /* Handle new connections */
                addrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, sizeof(clientaddr));
                newfd = accept(server_socket, (struct sockaddr *)&clientaddr, &addrlen);
                if (newfd == -1) {
                    perror("Server accept() error");
                } else {
                    FD_SET(newfd, &refset);

                    if (newfd > fdmax) {
                        /* Keep track of the maximum */
                        fdmax = newfd;
                    }
                    printf("New connection from %s:%d on socket %d\n",
                           inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
                }
            } else {
                modbus_set_socket(ctx, master_socket);
                rc = modbus_receive(ctx, query);
                if (rc > 0) {
                    modbus_reply(ctx, query, rc, mb_mapping);
                } else if (rc == -1) {
                    /* This example server in ended on connection closing or
                     * any errors. */
                    printf("Connection closed on socket %d\n", master_socket);
                    close(master_socket);

                    /* Remove from reference set */
                    FD_CLR(master_socket, &refset);

                    if (master_socket == fdmax) {
                        fdmax--;
                    }
                }
            }
        }
    }
}


