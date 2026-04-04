#ifndef HANDLER_H
#define HANDLER_H

#include "parser.h"
#include "utils.h"


int handle_set_flags(RespRequest *req, int *expireAt, TIME_FLAGS *flag);
void handle_ping(int client_fd);
void handle_echo(RespRequest *req, int client_fd);
void handle_set(RespRequest *req, int client_fd);
void handle_get(RespRequest *req, int client_fd);
void handle_type(RespRequest *req, int client_fd);
void handle_unknown(RespRequest *req, int client_fd);
int handle(RespRequest *req, int client_fd);

#endif