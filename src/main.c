#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "server.h"
#include "http.h"
#include "client.h"

#define REQUEST_BUFFER_SIZE 4096
#define FILE_BUFFER_SIZE 4096

static Client clients[FD_SETSIZE];

static void close_client(int fd, fd_set *read_set, fd_set *write_set){
    if (clients[fd].file_fd >= 0){
        close(clients[fd].file_fd);
        clients[fd].file_fd = -1;
    }
    close(fd);
    FD_CLR(fd, read_set);
    FD_CLR(fd, write_set);
    clients[fd] = (Client){0};
    clients[fd].file_fd = -1;
}
int main(void){
    Server *server = server_create(8080);
    if(server==NULL){
        fprintf(stderr, "Failed to create server\n");   
        return 1;
    }
    printf("Server listening on port 8080\n");
    fd_set master_read_set, master_write_set, read_set, write_set;
    FD_ZERO(&master_read_set);
    FD_ZERO(&master_write_set);
    int listen_fd=server_get_listen_fd(server);
    int max_fd=listen_fd;
    FD_SET(listen_fd, &master_read_set);
    while(1){
        read_set = master_read_set;
        write_set = master_write_set;
        if (select(max_fd + 1, &read_set, &write_set, NULL, NULL) < 0){
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }
        for (int fd=0;fd<=max_fd;fd++){
            if (FD_ISSET(fd, &read_set)){
                if (fd == listen_fd) {
                    int client_fd = server_accept(server);
                    if (client_fd < 0)
                        continue;

                    int flags = fcntl(client_fd, F_GETFL, 0);
                    if (flags == -1){
                        perror("fcntl(F_GETFL)");
                        close(client_fd);
                        continue;
                    }

                    if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1){
                        perror("fcntl(F_SETFL)");
                        close(client_fd);
                        continue;
                    }
                    clients[client_fd].received = 0;
                    clients[client_fd].file_fd = -1;
                    FD_SET(client_fd, &master_read_set);
                    if (client_fd > max_fd)
                        max_fd = client_fd;
                }
                else{
                    Client *c = &clients[fd];
                    ssize_t n = recv(fd, c->buffer + c->received, sizeof(c->buffer) - c->received, 0);
                    if (n > 0){
                        c->received += (size_t)n;
                        if (find_header_end(c->buffer, c->received) >= 0) {
                            http_process_request(c, c->buffer, c->received);
                            FD_CLR(fd, &master_read_set);
                            FD_SET(fd, &master_write_set);
                        }
                    }
                    else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)){
                        close_client(fd, &master_read_set, &master_write_set);
                    }
                }
            }
            if (FD_ISSET(fd, &write_set)){
                Client *c = &clients[fd];
                // 1. Send HTTP headers or error body
                if (c->response_sent < c->response_length){
                    ssize_t n = send(fd, c->response + c->response_sent, c->response_length - c->response_sent, 0);
                    if (n > 0)    c->response_sent += (size_t)n;
                    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK){
                        close_client(fd, &master_read_set, &master_write_set);
                    }
                    continue;
                }
                // 2. Stream static file (GET requests)
                if (c->file_fd >= 0 && c->file_sent < c->file_size){
                    char file_buf[FILE_BUFFER_SIZE];
                    ssize_t r = read(c->file_fd, file_buf, sizeof(file_buf));
                    if (r > 0){
                        ssize_t s = send(fd, file_buf, (size_t)r, 0);
                        if (s > 0){
                            c->file_sent += (size_t)s;
                            // If socket didn't take all read bytes, rewind file offset
                            if (s < r){
                                lseek(c->file_fd, s - r, SEEK_CUR);
                            }
                        }
                        else if (s < 0){
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                                lseek(c->file_fd, -r, SEEK_CUR);
                            else
                                close_client(fd, &master_read_set, &master_write_set);
                            
                        }
                    }
                    else{
                        // EOF or read error
                        close_client(fd, &master_read_set, &master_write_set);
                    }
                    continue;
                }

                // 3. Response completely finished
                close_client(fd, &master_read_set, &master_write_set);
            }
        }
    }
    server_close(server);
    return 0;
}