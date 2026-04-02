#ifndef HANDLER_H
#define HANDLER_H

#include <parser.h>

typedef void (*cmd_func)(RespRequest *, int);

typedef struct {
    REDIS_CMDS cmd;
    cmd_func handler;
} CommandEntry;


void handle_ping(int client_fd);
void handle_echo(RespRequest *req, int client_fd);
void handle_set(RespRequest *req, int client_fd);
void handle_get(RespRequest *req, int client_fd);
void handle_unkown(RespRequest *req, int client_fd);
int handle(RespRequest *req, int client_fd);

#endif