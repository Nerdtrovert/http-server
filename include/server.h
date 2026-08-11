#ifndef SERVER_H
#define SERVER_H

typedef struct Server Server;

Server *server_create(int port);

int server_accept(Server *server);

void server_close(Server *server);

#endif