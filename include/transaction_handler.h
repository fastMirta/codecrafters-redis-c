#ifndef TRANSACTION_HANDLER_H
#define TRANSACTION_HANDLER_H

#include "handler.h"
#include "utils.h"

void handle_incr(RespRequest *req, int client_fd);

#endif