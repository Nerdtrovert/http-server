#include <stdio.h>
#include <unistd.h>
#include "server.h"
#include "http.h"
#include <signal.h>
#include <sys/wait.h>

static void handle_sigchld(int signal){
    (void)signal;
    while (waitpid(-1, NULL, WNOHANG) > 0){
    }
}
int main(void){
    const int port = 8080;
    Server *server = server_create(port);
    if (server == NULL){
        return 1;
    }
    if (signal(SIGCHLD, handle_sigchld) == SIG_ERR){
        perror("signal");
        server_close(server);
        return 1;
    }
    printf("Server listening on port %d\n", port);
    while(1){
        int client_fd = server_accept(server);
        if (client_fd<0 ){
            continue;
        }
        pid_t pid = fork();
        if(pid<0){
            perror("fork");
            close(client_fd);
            continue;
        }else if(pid==0){
            printf("Client connected: fd=%d\n", client_fd);
            server_close_listener(server);        //child also gets a copy of the listening socket, so close it in the child
            http_handle(client_fd);
            close(client_fd);
            _exit(0);
        }else if (pid>0){
            close(client_fd);                   //parent also has a copy of the client socket, so close it in the parent
            continue;
        }
    }
    return 0;
}