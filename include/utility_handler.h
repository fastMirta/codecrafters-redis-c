#ifndef UTILITY_HANDLER
#define UTILITY_HANDLER

#include "handler.h"
struct Client; 
typedef struct Client Client;


void handle_info(RespRequest *req ,int client_fd);
void handle_replconf(char *response, long responseSize);
void handle_psync(char *replicationId, int offset);
void handle_replconf_ack(RespRequest *request, int client_fd);
void replconf_handle_getack(int client_fd);
void replconf_handle_ack(RespRequest *request, Client *client);
void handle_wait(RespRequest *req, int client_fd);

#endif