#include "auth_handler.h"

void handle_acl(RespRequest *req, int client_fd){
    toUpper(req->args[0]);
    if(strcmp(req->args[0], "WHOAMI") == 0){
        send(client_fd, "$7\r\ndefault\r\n", 13, 0);
        return;
    }
    if(strcmp(req->args[0], "GETUSER") == 0 && (strcmp(req->args[1], "default") == 0 || strcmp(req->args[1], "DEFAULT") == 0)){
        send(client_fd, "*2\r\n$5\r\nflags\r\n*0\r\n", 19, 0);
        return;
    }
}