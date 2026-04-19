#ifndef SET_HANDLER_H
#define SET_HANDLER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "handler.h"
#include "utils.h"

void handle_zadd(RespRequest *req, int client_fd);
void handle_zrank(RespRequest *req, int client_fd);
void handle_zrange(RespRequest *req, int client_fd);

#endif