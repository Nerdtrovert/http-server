#ifndef FILE_H
#define FILE_H

int build_file_path(const char *request_path, char *file_path, size_t file_path_size);

const char *get_mime_type(const char *file_path);

#endif