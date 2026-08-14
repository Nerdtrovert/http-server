#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>

#ifndef REQUEST_BUFFER_SIZE
#define REQUEST_BUFFER_SIZE 4096
#endif

typedef struct Client {

    char buffer[REQUEST_BUFFER_SIZE];
    size_t received;

    int file_fd;
    size_t file_size;
    size_t file_sent;

    char *response;
    size_t response_length;
    size_t response_sent;

    int response_ready;
} Client;

#endif // CLIENT_H