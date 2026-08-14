#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "http.h"
#include "file.h"
#include "client.h"
#define REQUEST_BUFFER_SIZE 4096
#define FILE_BUFFER_SIZE 4096

static const char RESP_400[] = "HTTP/1.1 400 Bad Request\r\n"
                               "Content-Type: text/html\r\n"
                               "Content-Length: 67\r\n"
                               "Connection: close\r\n\r\n"
                               "<!DOCTYPE html><html><body><h1>400 Bad Request</h1></body></html>";

static const char RESP_403[] = "HTTP/1.1 403 Forbidden\r\n"
                               "Content-Type: text/html\r\n"
                               "Content-Length: 65\r\n"
                               "Connection: close\r\n\r\n"
                               "<!DOCTYPE html><html><body><h1>403 Forbidden</h1></body></html>";

static const char RESP_404[] = "HTTP/1.1 404 Not Found\r\n"
                               "Content-Type: text/html\r\n"
                               "Content-Length: 65\r\n"
                               "Connection: close\r\n\r\n"
                               "<!DOCTYPE html><html><body><h1>404 Not Found</h1></body></html>";

static const char RESP_405[] = "HTTP/1.1 405 Method Not Allowed\r\n"
                               "Content-Type: text/html\r\n"
                               "Content-Length: 74\r\n"
                               "Connection: close\r\n\r\n"
                               "<!DOCTYPE html><html><body><h1>405 Method Not Allowed</h1></body></html>";

static const char RESP_500[] = "HTTP/1.1 500 Internal Server Error\r\n"
                               "Content-Type: text/html\r\n"
                               "Content-Length: 81\r\n"
                               "Connection: close\r\n\r\n"
                               "<!DOCTYPE html><html><body><h1>500 Internal Server Error</h1></body></html>";

typedef struct{
    char method[16];
    char path[256];
    char version[16];
} HTTPRequest;
ssize_t find_header_end(const char *buffer, size_t length){
    if (!buffer || length <4 )return -1;
    for (size_t i = 0; i <= length - 4; i++){
        if (buffer[i] == '\r' &&
            buffer[i + 1] == '\n' &&
            buffer[i + 2] == '\r' &&
            buffer[i + 3] == '\n'){
            return (ssize_t)i;
        }
    }
    return -1;
}

int send_all(int fd, const char *data, size_t length){
    size_t total_sent = 0;
    while (total_sent < length)    {
        ssize_t sent = send(fd, data + total_sent, length - total_sent, 0);
        if (sent <= 0)        {
            return 0;
        }
        total_sent += (size_t)sent;
    }
    return 1;
}

int parse_request_line(const char *buffer, size_t length, HTTPRequest *request){
    if (!buffer || length == 0 || !request)
        return 0;
    size_t i = 0;
    size_t m_len = 0;
    while (i < length && buffer[i] != ' ')    {
        if (m_len >= sizeof(request->method) - 1)
            return 0;
        request->method[m_len++] = buffer[i++];
    }
    request->method[m_len] = '\0';
    if (m_len == 0 || i >= length || buffer[i] != ' ')
        return 0;
    i++;
    size_t p_len = 0;
    while (i < length && buffer[i] != ' '){
        if (p_len >= sizeof(request->path) - 1)
            return 0;
        request->path[p_len++] = buffer[i++];
    }
    request->path[p_len] = '\0';
    if (p_len == 0 || i >= length || buffer[i] != ' ')
        return 0;
    i++;
    size_t v_len = 0;
    while (i < length && buffer[i] != '\r'){
        if (v_len >= sizeof(request->version) - 1)
            return 0;
        request->version[v_len++] = buffer[i++];
    }
    request->version[v_len] = '\0';
    if (v_len == 0 || i >= length || buffer[i] != '\r')
        return 0;
    if (strcmp(request->version, "HTTP/1.1") != 0)
        return 0;
    i++;
    if (i >= length || buffer[i] != '\n')
        return 0;
    return 1;
}
int send_response_header(int client_fd, int status_code, const char *status_text,
                         const char *content_type, size_t content_length){
    char header_buf[512];
    int res = snprintf(header_buf, sizeof(header_buf),
                       "HTTP/1.1 %d %s\r\n"
                       "Content-Length: %zu\r\n"
                       "Content-Type: %s\r\n"
                       "Connection: close\r\n\r\n",
                       status_code, status_text, content_length, content_type);

    if (res < 0 || (size_t)res >= sizeof(header_buf)){
        return 0; // Truncation error
    }
    return send_all(client_fd, header_buf, (size_t)res);
}
int send_response_body(int client_fd, int status_code, const char *status_text, 
    const char *content_type, const char *data, size_t length){
        char header_buf[512];
        int res = snprintf(header_buf, sizeof(header_buf),
                           "HTTP/1.1 %d %s\r\n"
                           "Content-Length: %zu\r\n"
                           "Content-Type: %s\r\n"
                           "Connection: close\r\n\r\n",
                           status_code, status_text, length, content_type);
        if (res < 0 || (size_t)res >= sizeof(header_buf)) {
            return 0;
        }
        if (!send_all(client_fd, header_buf, (size_t)res)){
            return 0;
        }
        if(length > 0 && data == NULL){
            return 0;
        }
        if (length > 0){
            if (!send_all(client_fd, data, length))
                return 0;
        }
        return 1;
}
int http_process_request(Client *client, const char *buffer, size_t length){
    if (!client) return 0;
    // 1. Reset response tracking fields
    client->file_fd = -1; client->file_size = 0;
    client->file_sent = 0; client->response = NULL;
    client->response_length = 0;
    client->response_sent = 0;
    client->response_ready = 0;

    HTTPRequest request;
    if (!parse_request_line(buffer, length, &request)){
        client->response = (char *)RESP_400;
        client->response_length = sizeof(RESP_400) - 1;
        client->response_ready = 1;
        return 1;
    }

    if (strcmp(request.method, "GET") != 0 && strcmp(request.method, "HEAD") != 0){
        client->response = (char *)RESP_405;
        client->response_length = sizeof(RESP_405) - 1;
        client->response_ready = 1;
        return 1;
    }

    char file_path[PATH_MAX];
    if (!build_file_path(request.path, file_path, sizeof(file_path))){
        client->response = (char *)RESP_400;
        client->response_length = sizeof(RESP_400) - 1;
        client->response_ready = 1;
        return 1;
    }

    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0){
        if (errno == ENOENT){
            client->response = (char *)RESP_404;
            client->response_length = sizeof(RESP_404) - 1;
        }
        else if (errno == EACCES || errno == EISDIR){
            client->response = (char *)RESP_403;
            client->response_length = sizeof(RESP_403) - 1;
        }
        else{
            client->response = (char *)RESP_500;
            client->response_length = sizeof(RESP_500) - 1;
        }
        client->response_ready = 1;
        return 1;
    }
    struct stat file_info;
    if (fstat(file_fd, &file_info) == -1 || !S_ISREG(file_info.st_mode)){
        close(file_fd);
        client->response = (char *)RESP_403;
        client->response_length = sizeof(RESP_403) - 1;
        client->response_ready = 1;
        return 1;
    }
    // Reuse client->buffer to format 200 OK headers (since request reading is finished)
    const char *content_type = get_mime_type(file_path);
    int header_len = snprintf(client->buffer, sizeof(client->buffer),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Length: %zu\r\n"
                              "Content-Type: %s\r\n"
                              "Connection: close\r\n\r\n",
                              (size_t)file_info.st_size, content_type);

    if (header_len < 0 || (size_t)header_len >= sizeof(client->buffer)){
        close(file_fd);
        client->response = (char *)RESP_500;
        client->response_length = sizeof(RESP_500) - 1;
        client->response_ready = 1;
        return 1;
    }
    client->response = client->buffer;
    client->response_length = (size_t)header_len;
    if (strcmp(request.method, "HEAD") == 0){
        close(file_fd);
    }
    else{
        client->file_fd = file_fd;
        client->file_size = (size_t)file_info.st_size;
    }
    client->response_ready = 1;
    return 1;
}