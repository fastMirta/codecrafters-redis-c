#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "transaction_handler.h"


/**Increments value of given key only if its a number
 * if key doesnt exist creates one with given param and init as 1
 */
void handle_incr(RespRequest *req, int client_fd, int isQueued){
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


void handle_exec(Client *client){
    if(client->is_queued == 0){
        send(client->fd, "-ERR EXEC without MULTI\r\n", 25, 0);
        return;
    }

    char header[32];
    int headerLen = snprintf(header, sizeof(header), "*%d\r\n", client->queuedCommands);
    send(client->fd, header, headerLen, 0);
    client->is_queued = 0;

    if(client->queuedCommands == 0){
        return;
    }

    
    printf("queued cmds: %d\n", client->queuedCommands);
    for(int i = 0; i < client->queuedCommands; i++){
        printf("Entered loop in exec\n");
        printRequest(client->requests[i]);
        handle(client->requests[i], client);

        for(int j=0; j < client->requests[i]->argc; j++) free(client->requests[i]->args[j]);

        free(client->requests[i]);
        client->requests[i] = NULL;
    }
    
    client->queuedCommands = 0;
}

