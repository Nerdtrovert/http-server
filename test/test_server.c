#include "server.h"
#include <stdio.h>

int main(void)
{
    Server *server = server_create(8080);
    if (server == NULL)     {
        return 1;
    }
    printf("Server is listening on port 8080\n");
    getchar();
    server_close(server);
    return 0;
}