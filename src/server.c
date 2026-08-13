#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>
#include "server.h"

struct Server{
    int listen_fd;
};

Server *server_create(int port){
    Server *server = malloc(sizeof *server);
    if (server == NULL){
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server->listen_fd<0){
        perror("socket");
        free(server);
        return NULL;
    }
    struct sockaddr_in address={0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server->listen_fd, (struct sockaddr *)&address, sizeof(address)) == -1){
        perror("bind");
        close(server->listen_fd);
        free(server);
        return NULL;
    }
    if (listen(server->listen_fd, 10) == -1){
        perror("listen");
        close(server->listen_fd);
        free(server);
        return NULL;
    }
    return server;
}

int server_accept(Server *server){
    struct sockaddr_in client_addr = {0};
    socklen_t client_len = sizeof(client_addr);
    while(1){
        int client_fd = accept(
            server->listen_fd,
            (struct sockaddr *)&client_addr, &client_len);
        if(client_fd>=0) return client_fd;
        if (errno == EINTR) continue; // interrupted by signal, retry
        perror("accept");
        return -1;
    }
    
}

void server_close(Server *server){
    if(server!=NULL){
        if(server->listen_fd>=0) close(server->listen_fd);
        free(server);
    }
}

void server_close_listener(Server *server){
    if(server!=NULL && server->listen_fd>=0){
        close(server->listen_fd);
        server->listen_fd = -1;
    }
}