#ifndef HANDLER_H
#define HANDLER_H

#include <netinet/in.h>
#include <netinet/ip.h>
#include "parser.h"
#include "utils.h"
#include "utility_handler.h"
#include "transaction_handler.h"

#define MAX_CLIENTS 100
#define WATCHERS_SIZE 1024
#define MAX_QUEUED_CMDS 100

typedef struct Client{
    int fd;
    int is_blocked;
    int is_queued;
    int is_blpop;
    int is_subscribed;
    int is_replica;
    int is_master;
    int is_waiting;
    long long repl_offset;
    long long timeout_at;
    long long blocked_at;
    char *waiting_for_key;
    char *min_id;
    char *channel_subed[64];
    int channel_count;
    RespRequest *requests[MAX_QUEUED_CMDS];
    int queuedCommands;
    
    //Flags section:
    int is_auth;
    int has_nopass;
    char password[65];

    char *watch_keys[64];
    size_t watch_keys_size;
    int is_dirty;
} Client;



typedef struct ChannelEntry {
    int client_fd;       
    struct ChannelEntry *next;
} ChannelEntry;

typedef struct {
    ChannelEntry *head;
    ChannelEntry *tail;  
    size_t length;
} ChannelsList;

typedef struct Channels {
    char *key;
    void *value;
} Channels;

typedef struct RedisConfig {
    int port;
    char *role; 
    char *master_replid; 
    long long master_repl_offset;
    long long slave_repl_offset;
    long long captured_master_offset;
    char *master_host;
    int master_port;
    int master_fd;
    int wantedReplicas;
    int wait_client_fd;

    //RDB settings
    char rdb_directory[1024];
    char rdb_name[256];
    char appendOnly[4];
    char appenddirname[1024];
    char appendfilename[256];
    char appendfsync[256];

} RedisConfig;

typedef struct Clients_Watch {
    char *key;
    Client **clientList;
    size_t clientsSize;
} Clients_Watch;

extern RedisConfig server_config;
extern Client *clients[MAX_CLIENTS];
extern Channels *channels[MAX_CLIENTS];
extern Clients_Watch *watchers[WATCHERS_SIZE];



int handle_set_flags(RespRequest *req, int *expireAt, TIME_FLAGS *flag);
void handle_ping(int client_fd);
void handle_echo(RespRequest *req, int client_fd);
void handle_set_stream(RespRequest *req, int client_fd);
int get_length(char *array[]);
void handle_set(RespRequest *req, RedisType type, int client_fd);
void handle_get(RespRequest *req, int client_fd);
void handle_type(RespRequest *req, int client_fd);
void handle_unknown(RespRequest *req, int client_fd);
int handle(RespRequest *req, Client *client);
void printRequest(RespRequest *req);

#endif