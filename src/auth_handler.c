#include "auth_handler.h"

void handle_acl(RespRequest *req, int client_fd){
    toUpper(req->args[0]);
    if(strcmp(req->args[0], "WHOAMI") == 0){
        send(client_fd, "$7\r\ndefault\r\n", 13, 0);
        return;
    }
}