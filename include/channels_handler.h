#ifndef CHANNELS_HANDLER_H
#define CHANNELS_HANDLER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "handler.h"

void handle_subscribe(RespRequest *req, Client *client);
void handle_publish(RespRequest *req, int client_fd);
void handle_unsubscribe(RespRequest *req, Client* client);

#endif