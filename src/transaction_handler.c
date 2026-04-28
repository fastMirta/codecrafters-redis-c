#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "transaction_handler.h"


Clients_Watch *watchers[WATCHERS_SIZE] = {NULL};

/**Increments value of given key only if its a number
 * if key doesnt exist creates one with given param and init as 1
 */
void handle_incr(RespRequest *req, int client_fd){
    Entry *entry = store_getEntry(req->args[0]);
    if(entry == NULL){
        printf("Entry is null my friendos\n");
        char *new_val_string = malloc(12); 
        if (new_val_string == NULL) return;
        
        strcpy(new_val_string, "1");
        store_set(req->args[0], new_val_string, NO_TIME_FLAG, -1, TYPE_STRING);
        send(client_fd, ":1\r\n", 4, 0);
        return;
    }
    printf("IM NOT NULL: brotato\n");
    printf("req value string: %s\n", req->args[0]);
    printf("req value int: %d\n", atoi(req->args[0]));
    printf("req value int INCR: %d\n", atoi(req->args[0]) + 1);
    int current_val = atoi((char *)entry->value);
    if(current_val == 0 && strcmp((char*)entry->value, "0") != 0){
        char *response = "-ERR value is not an integer or out of range\r\n";
        send(client_fd, response, strlen(response), 0);
        return;
    }
    char *new_val_str = malloc(12); 
    if (new_val_str == NULL) {
        printf("No more memory brotato\n");
        return;
    }

    current_val++;
    snprintf(new_val_str, 12, "%d", current_val);
    store_set(req->args[0], (void*)new_val_str, NO_TIME_FLAG, -1, TYPE_STRING);
    
    char response[32];
    int len = snprintf(response, sizeof(response), ":%d\r\n", current_val);
	send(client_fd, response, strlen(response), 0);
    
    //free(new_val_str);
    return;
}


void handle_multi(RespRequest *req, Client *client){
    if(client->is_queued){
        send(client->fd, "-ERR MULTI calls can not be nested\r\n", 36, 0);
        return;
    }
    client->queuedCommands = 0;
    client->is_queued = 1;
    printf("is queued?: %d\n", client->is_queued);
    send(client->fd, "+OK\r\n", 5, 0);
}


void handle_exec(Client *client) {
    if (client->is_queued == 0) {
        send(client->fd, "-ERR EXEC without MULTI\r\n", 25, 0);
        return;
    }
    printf("is dirty: %d\n", client->is_dirty);
    if(client->is_dirty){
        printf("YES dirty\n");

        client->is_dirty = 0;
        client->is_queued = 0;
        client->queuedCommands = 0;
        for(int i = 0; i < client->queuedCommands; i++) {
            for(int j = 0; j < client->requests[i]->argc; j++) {
                free(client->requests[i]->args[j]);
            }
            free(client->requests[i]);
        }

        send(client->fd, "*-1\r\n", 5, 0);
        return;
    }

    char header[32];
    int headerLen = snprintf(header, sizeof(header), "*%d\r\n", client->queuedCommands);
    send(client->fd, header, headerLen, 0);

    int cmdCount = client->queuedCommands;
    client->queuedCommands = 0;

    for (int i = 0; i < cmdCount; i++) {
        printRequest(client->requests[i]);
        client->is_queued = 0;
        handle(client->requests[i], client);
        client->is_queued = 0;
        for (int j = 0; j < client->requests[i]->argc; j++)
            free(client->requests[i]->args[j]);
        free(client->requests[i]);
        client->requests[i] = NULL;
    }
    client->is_queued = 0;
}

void handle_discard(RespRequest *req, Client *client){
    if(client->is_queued == 0){
        send(client->fd, "-ERR DISCARD without MULTI\r\n", 28, 0);
        return;
    }

    for(int i = 0; i < client->queuedCommands; i++){
        free(client->requests[i]);
        client->requests[i] = NULL;
    }
    client->queuedCommands = 0;
    client->is_queued = 0;

    send(client->fd, "+OK\r\n", 5, 0);
    
}


void handle_watch(RespRequest *req, Client *client){
    if(req->argc < 1) return;

    if(client->is_queued){
        send(client->fd, "-ERR WATCH inside MULTI is not allowed\r\n", 40, 0);
        return;
    }

    //req->args[i] = key
    for(int i = 0; i < req->argc; i++){

        client->watch_keys[client->watch_keys_size] = strdup(req->args[i]);
        client->watch_keys_size++;

        //Handles global watchers 
        int index = hash(req->args[i]);
        if(watchers[index] == NULL){
            printf("entering malloc block\n");
            watchers[index] = malloc(sizeof(Clients_Watch));
            printf("after first malloc: %p\n", (void*)watchers[index]);
            watchers[index]->key = malloc(256); 
            printf("after key malloc: %p\n", (void*)watchers[index]->key);
            watchers[index]->clientList = malloc(sizeof(Client*) * MAX_CLIENTS); 
            printf("after clientList malloc: %p\n", (void*)watchers[index]->clientList);
            watchers[index]->clientsSize = 0;
        }
        printf("watchers[index] == NULL %d\n", watchers[index] == NULL);

        //strcpy(watchers[index]->key, req->args[i]);printf("index: %d\n", index);
        printf("watchers[index]: %p\n", (void*)watchers[index]);
        printf("watchers[index]->clientList: %p\n", (void*)watchers[index]->clientList);
        printf("watchers[index]->clientsSize: %zu\n", watchers[index]->clientsSize);
        printf("client: %p\n", (void*)client);
        watchers[index]->clientList[watchers[index]->clientsSize] = client;
        watchers[index]->clientsSize++;
        
    }

    send(client->fd, "+OK\r\n", 5, 0);
}

void handle_unwatch(RespRequest *req, Client *client){
    printf("UNWATCH cmd\n");
    for(int i = 0; i < client->watch_keys_size; i++){

        int index = hash(client->watch_keys[i]);
        if(watchers[index] != NULL){
            for(int j = 0; j < watchers[index]->clientsSize; j++){
                if(client == watchers[index]->clientList[j]){
                    watchers[index]->clientList[j] = NULL;
                    free(client->watch_keys[i]);
                }
            }
        }
    }

    
    client->watch_keys_size = 0;
    send(client->fd, "+OK\r\n", 5, 0);
    printf("Finished unwatch\n");
}