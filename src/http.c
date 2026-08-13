#include<unistd.h>
#include<stdio.h>
#include<sys/socket.h>
#include<string.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#define REQUEST_BUFFER_SIZE 4096
#define FILE_BUFFER_SIZE 4096

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
int send_response(int client_fd, int status_code, const char *status_text, const char *content_type, const char *body){
    char header_buf[512];
    size_t body_len = body ? strlen(body) : 0;
    int res = snprintf(header_buf, sizeof(header_buf),
                       "HTTP/1.1 %d %s\r\n"
                       "Content-Length: %zu\r\n"
                       "Content-Type: %s\r\n"
                       "Connection: close\r\n\r\n",
                       status_code, status_text, body_len, content_type);
    if (res < 0 || (size_t)res >= sizeof(header_buf))    {
        return 0;
    }
    if (!send_all(client_fd, header_buf, (size_t)res)){
        return 0;
    }
    if (body_len > 0 && body != NULL){
        if (!send_all(client_fd, body, body_len)){
            return 0;
        }
    }
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

int http_handle(int client_fd){
    HTTPRequest request;
    char buffer[REQUEST_BUFFER_SIZE];
    size_t total_received = 0;
    while (total_received < sizeof(buffer)){
        ssize_t bytes_received = recv(client_fd, buffer + total_received, sizeof(buffer) - total_received, 0);
        if (bytes_received > 0){
            total_received += (size_t)bytes_received;
            if (find_header_end(buffer, total_received) >= 0)
                break;
        }
        else if (bytes_received == 0){
            fprintf(stderr, "Connection closed by client\n");
            return -1;
        }
        else{
            perror("recv");
            return -1;
        }
    }
    if (total_received == sizeof(buffer) && find_header_end(buffer, total_received) < 0){
        fprintf(stderr, "Request header too large\n");
        send_response(client_fd, 400, "Bad Request", "text/html",
                      "<!DOCTYPE html><html><head><title>400 Bad Request</title></head><body><h1>400 Request Header Too Large</h1></body></html>");
        return -1;
    }
    if (!parse_request_line(buffer, total_received, &request)){
        fprintf(stderr, "Invalid HTTP request line\n");
        send_response(client_fd, 400, "Bad Request", "text/html",
                      "<!DOCTYPE html><html><head><title>400 Bad Request</title></head><body><h1>400 Invalid Request Line</h1></body></html>");
        return -1;
    }
    if (strcmp(request.method, "GET") != 0 && strcmp(request.method, "HEAD") != 0){
        if (!send_response(client_fd, 405, "Method Not Allowed", "text/html",
                           "<!DOCTYPE html><html><head><title>405 Method Not Allowed</title></head><body><h1>405 Method Not Allowed</h1></body></html>")){
            return -1;
        }
        return 0;
    }
    char file_path[PATH_MAX];
    if (!build_file_path(request.path, file_path, sizeof(file_path))){
        fprintf(stderr, "Invalid path or directory traversal detected\n");
        send_response(client_fd, 400, "Bad Request", "text/html",
                      "<!DOCTYPE html><html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request</h1></body></html>");
        return -1;
    }
    printf("Filesystem path: %s\n", file_path);
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0){
        if (errno == ENOENT){
            send_response(client_fd, 404, "Not Found", "text/html",
                          "<!DOCTYPE html><html><head><title>404 Not Found</title></head><body><h1>404 Resource Not Found</h1></body></html>");
            return 0;    
        }
        else if (errno == EACCES){
            send_response(client_fd, 403, "Forbidden", "text/html",
                          "<!DOCTYPE html><html><head><title>403 Forbidden</title></head><body><h1>403 Forbidden</h1></body></html>");
                          return 0;
        }
        else if(errno == EISDIR){
            send_response(client_fd, 403, "Forbidden", "text/html",
                          "<!DOCTYPE html><html><head><title>403 Forbidden</title></head><body><h1>403 Forbidden</h1></body></html>");
                            return 0;
        }
        else{
            send_response(client_fd, 500, "Internal Server Error", "text/html",
                          "<!DOCTYPE html><html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1></body></html>");
                          return -1;
        }
    }
    printf("File opened successfully: fd=%d\n", file_fd);
    struct stat file_info;
    if(fstat(file_fd, &file_info)==-1){
        perror("fstat");
        send_response(client_fd, 500, "Internal Server Error", "text/html",
                      "<!DOCTYPE html><html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1></body></html>");
        close(file_fd);
        return -1;
    }
    if (!S_ISREG(file_info.st_mode)){
        close(file_fd);
        send_response(
            client_fd,
            403,
            "Forbidden",
            "text/html",
            "<!DOCTYPE html><html><body><h1>403 Forbidden</h1></body></html>");
        return 0;
    }
    printf("File size: %ld bytes\n", (long)file_info.st_size);
    const char *content_type = get_mime_type(file_path);
    if (!send_response_header(client_fd,   200,  "OK", content_type,
            (size_t)file_info.st_size)){
        close(file_fd);
        return -1;
    }
    if (strcmp(request.method, "HEAD") == 0){
        close(file_fd);
        return 0;
    }
    char file_buffer[FILE_BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, file_buffer, sizeof(file_buffer))) > 0){
        if (!send_all(client_fd, file_buffer, (size_t)bytes_read)){
            close(file_fd);
            return -1;
        }
    }
    if (bytes_read < 0){
        perror("read");
        close(file_fd);
        return -1;
    }
 
    close(file_fd);

    return 0;
}
