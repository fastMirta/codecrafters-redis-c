#ifndef HANDLER_H
#define HANDLER_H

#include <netinet/in.h>
#include <netinet/ip.h>
#include "parser.h"
#include "utils.h"
#include "transaction_handler.h"

#define MAX_CLIENTS 100
#define MAX_QUEUED_CMDS 100

typedef struct Client {
    int fd;
    int is_blocked;
    int is_queued;
    long long timeout_at;
    char *waiting_for_key;
    char *min_id;
    RespRequest *requests[MAX_QUEUED_CMDS];
    int queuedCommands;
} Client;

typedef struct {
    int port;
    char *role; 
    char *master_replid; 
    int master_repl_offset;
    char *master_host;
    int master_port;

} RedisConfig;

extern RedisConfig server_config;
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
void printRequest(RespRequest *req);

#endif