#include "auth_handler.h"

void handle_acl(RespRequest *req, Client *client){
    toUpper(req->args[0]);
    if(strcmp(req->args[0], "WHOAMI") == 0){
        send(client->fd, "$7\r\ndefault\r\n", 13, 0);
        return;
    }
    if(strcmp(req->args[0], "GETUSER") == 0 && (strcmp(req->args[1], "default") == 0 || strcmp(req->args[1], "DEFAULT") == 0)){
        if(client->has_nopass){
            send(client->fd, "*2\r\n$5\r\nflags\r\n*1\r\n$6\r\nnopass\r\n", 31, 0);
        }
        else{
            send(client->fd, "*2\r\n$5\r\nflags\r\n*0\r\n", 19, 0);
        }
        
        return;
    }
}