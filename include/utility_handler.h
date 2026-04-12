#ifndef UTILITY_HANDLER
#define UTILITY_HANDLER

#include "handler.h"

void handle_info(RespRequest *req ,int client_fd);
void handle_replconf(char *response, long responseSize);
void handle_psync(char *replicationId, int offset);

#endif