#ifndef HANDLER_H
#define HANDLER_H

#include "parser.h"
#include "utils.h"
#include "transaction_handler.h"

#define MAX_CLIENTS 100

typedef struct Client {
    int fd;
    int is_blocked;
    int is_queued;
    long long timeout_at;
    char *waiting_for_key;
    char *min_id;
} Client;

extern Client *clients[MAX_CLIENTS];

int handle_set_flags(RespRequest *req, int *expireAt, TIME_FLAGS *flag);
void handle_ping(int client_fd);
void handle_echo(RespRequest *req, int client_fd);
void handle_set_stream(RespRequest *req, int client_fd, int isQueued);
int get_length(char *array[]);
void handle_set(RespRequest *req, RedisType type, int client_fd, int isQueued);
void handle_get(RespRequest *req, int client_fd);
void handle_type(RespRequest *req, int client_fd, int isQueued);
void handle_unknown(RespRequest *req, int client_fd);
int handle(RespRequest *req, Client *client);

#endif