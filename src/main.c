#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include "server.h"
#include "http.h"
#include "client.h"

#define REQUEST_BUFFER_SIZE 4096
#define FILE_BUFFER_SIZE 4096

struct pollfd poll_fds[FD_SETSIZE];
static Client clients[FD_SETSIZE];
static void remove_client(int index, int *poll_count){
    int fd = poll_fds[index].fd;
    if (clients[fd].file_fd >= 0){
        close(clients[fd].file_fd);
    }
    close(fd);
    clients[fd] = (Client){0};
    clients[fd].file_fd = -1;
    poll_fds[index] = poll_fds[*poll_count - 1];
    (*poll_count)--;
}
int main(void){
    Server *server = server_create(8080);
    if (server == NULL)
    {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }
    printf("Server listening on port 8080\n");

    int listen_fd = server_get_listen_fd(server);
    int poll_count = 0;

    poll_fds[0].fd = listen_fd;
    poll_fds[0].events = POLLIN;
    poll_fds[0].revents = 0;
    poll_count = 1;

    while (1){
        int ready = poll(poll_fds, poll_count, -1);
        if (ready < 0){
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }
        for (int i = 0; i < poll_count; i++){
            if (poll_fds[i].revents == 0)
                continue;

            int fd = poll_fds[i].fd;

            if (poll_fds[i].revents & POLLIN){
                if (fd == listen_fd){
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
                    if (poll_count >= FD_SETSIZE){
                        close(client_fd);
                        fprintf(stderr, "Maximum clients reached\n");
                        continue;
                    }
                    poll_fds[poll_count].fd = client_fd;
                    poll_fds[poll_count].events = POLLIN;
                    poll_fds[poll_count].revents = 0;
                    poll_count++;
                    clients[client_fd] = (Client){0};
                    clients[client_fd].file_fd = -1;
                }
                else{
                    Client *c = &clients[fd];
                    ssize_t n = recv(
                        fd,
                        c->buffer + c->received,
                        sizeof(c->buffer) - c->received,
                        0);
                    if (n > 0){
                        c->received += (size_t)n;
                        if (find_header_end(c->buffer, c->received) >= 0){
                            if (http_process_request( c,  c->buffer,  c->received) < 0){
                                close(fd);
                                poll_fds[i] = poll_fds[poll_count - 1];
                                poll_count--;
                                i--;continue;
                            }
                            //Request is complete. Switch this client from READ → WRITE.
                            poll_fds[i].events = POLLOUT;
                            c->received = 0;
                        }
                    }
                    else if (n == 0){
                        // client disconnected
                        close(fd);
                        poll_fds[i] =  poll_fds[poll_count - 1];
                        poll_count--;
                        i--;
                    }else if (errno != EAGAIN && errno != EWOULDBLOCK){
                        perror("recv");
                        close(fd);
                        poll_fds[i] =  poll_fds[poll_count - 1];
                        poll_count--;
                        i--;
                    }
                }
            }
            // ==================== WRITE READY ====================
            if (poll_fds[i].revents & POLLOUT){
                Client *c = &clients[fd];
                // Phase 1: Send headers (or error response body)
                if (c->response_sent < c->response_length){
                    ssize_t n = send( fd, c->response + c->response_sent, c->response_length - c->response_sent,
                        0);
                    if (n > 0) c->response_sent += (size_t)n;
                    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK){
                        perror("send");
                        remove_client(i, &poll_count);
                        i--;
                    }
                    continue; // Stay in POLLOUT, don't stream file until headers finish
                }
                // Phase 2: Headers are done -> Stream file body (if GET request with file_fd)
                if (c->file_fd >= 0 && c->file_sent < c->file_size){
                    char file_buf[FILE_BUFFER_SIZE];
                    ssize_t r = read(c->file_fd, file_buf, sizeof(file_buf));
                    if (r > 0){
                        ssize_t s = send(fd, file_buf, (size_t)r, 0);
                        if (s > 0){
                            c->file_sent += (size_t)s;
                            // If kernel socket buffer accepted fewer bytes than read, rewind file
                            if (s < r){
                                lseek(c->file_fd, s - r, SEEK_CUR);
                            }
                        }
                        else if (s < 0 && errno != EAGAIN && errno != EWOULDBLOCK){
                            perror("send file");
                            remove_client(i, &poll_count);
                            i--;
                        }
                    }
                    else if (r == 0){
                        remove_client(i, &poll_count);
                        i--;
                    }
                    else if (r < 0){
                        perror("read");
                        remove_client(i, &poll_count);
                        i--;
                    }
                    continue;
                }
                // Phase 3: Everything sent (e.g. HEAD request or error response) -> Close
                remove_client(i, &poll_count);
                i--;
            }
        }
    }
    server_close(server);
    return 0;
}