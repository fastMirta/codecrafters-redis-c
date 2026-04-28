#ifndef LIST_HANDLER_H
#define LIST_HANDLER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "handler.h"
#include "utils.h"

void handle_rpush(RespRequest *req, int client_fd);
void handle_lpush(RespRequest *req, int client_fd);
void handle_lrange(RespRequest *req, int client_fd);
void handle_llen(RespRequest *req, int client_fd);
void handle_lpop(RespRequest *req, int client_fd);
void handle_blpop(RespRequest *req, Client *client);
void handle_endblpop(Client *client);

#endif