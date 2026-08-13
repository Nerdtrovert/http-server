#include "file.h"
#include <stdio.h>
#include <string.h>

int build_file_path(const char *request_path, char *file_path, size_t file_path_size){
    if (!request_path || !file_path || file_path_size == 0)
        return 0;
    if (request_path[0] != '/')
        return 0;
    int segment_len = 0;
    for (size_t i = 0;; i++){
        char c = request_path[i];
        if (c == '/' || c == '\0'){
            if (segment_len == 2 && request_path[i - 2] == '.' && request_path[i - 1] == '.'){
                return 0;
            }
            segment_len = 0;
        }
        else
            segment_len++;
        if (c == '\0')
            break;
    }
    if (strcmp(request_path, "/") == 0){
        if (snprintf(file_path, file_path_size, "%s/index.html", "www") >= (int)file_path_size)
            return 0;
    }
    else{
        if (snprintf(file_path, file_path_size, "%s%s", "www", request_path) >= (int)file_path_size)
            return 0;
    }
    return 1;
}

const char *get_mime_type(const char *file_path){
    if (!file_path){
        return "application/octet-stream";
    }
    const char *dot = strrchr(file_path, '.');
    if (!dot || strchr(dot, '/') != NULL)
    {
        return "application/octet-stream";
    }
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".js") == 0)
        return "text/javascript";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".txt") == 0)
        return "text/plain";

    return "application/octet-stream";
}
