#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "handler.h"

void handle_config(RespRequest *req, int client_fd){
    if(req->argc > 2){
        printf("DEBUG: arguments are less than 2");
        return;
    }

    printf("printing config\n");
    printRequest(req);


    toUpper(req->args[0]);
    if(req->command == CONFIG_GET){
        toUpper(req->args[0]);
        printf("Print after got upper\n");
        char directoryBuffer[1024];
        printf("after setting buffer\n");
        if(strcmp(req->args[0], "DIR") == 0){
            
            //*2\r\n$3\r\ndir\r\n$16\r\n/tmp/redis-files\r\n
            snprintf(directoryBuffer, sizeof(directoryBuffer), "*2\r\n$3\r\ndir\r\n$%ld\r\n%s\r\n",
             strlen(server_config.rdb_directory), server_config.rdb_directory);
            send(client_fd, directoryBuffer, strlen(directoryBuffer), 0);
            return;
        }

        if(strcmp(req->args[0], "DBFILENAME") == 0){
            printf("DBFILE NAME 4 EVEA\n");
             snprintf(directoryBuffer, sizeof(directoryBuffer), "*2\r\n$3\r\ndir\r\n$%ld\r\n%s\r\n",
             strlen(server_config.rdb_name), server_config.rdb_name);
            send(client_fd, directoryBuffer, strlen(directoryBuffer), 0);
            return;
        }

    }
}