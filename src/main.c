#include <stdio.h>
#include <unistd.h>
#include "server.h"
#include "http.h"

int main(void){
    const int port = 8080;
    Server *server = server_create(port);
    if (server == NULL){
        return 1;
    }
    printf("Server listening on port %d\n", port);
    int client_fd = server_accept(server);

    if (client_fd < 0){
        server_close(server);
        return 1;
    }
    printf("Client connected: fd=%d\n", client_fd);
    http_handle(client_fd);
    close(client_fd);
    server_close(server);
    return 0;
}