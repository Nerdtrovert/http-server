#ifndef HTTP_H
#define HTTP_H

#include <sys/types.h>
#include "client.h"

int http_process_request(Client *client, const char *buffer, size_t length);
ssize_t find_header_end(const char *buffer, size_t length);

#endif