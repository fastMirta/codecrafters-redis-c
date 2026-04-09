#ifndef TRANSACTION_HANDLER_H
#define TRANSACTION_HANDLER_H

#include "handler.h"
#include "utils.h"

struct Client; 
typedef struct Client Client;

void handle_incr(RespRequest *req, int client_fd, int isQueued);
void handle_multi(RespRequest *req, Client *client);
void handle_exec(Client *client);
void handle_discard(RespRequest *req, Client *client);
#endif