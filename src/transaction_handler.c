#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "transaction_handler.h"

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
    int current_val = atoi((char *)entry->value) + 1;
    char *new_val_str = malloc(12); 
    if (new_val_str == NULL) {
        printf("No more memory brotato\n");
        return;
    }

    snprintf(new_val_str, 12, "%d", current_val);
    store_set(req->args[0], (void*)new_val_str, NO_TIME_FLAG, -1, TYPE_STRING);
    
    char response[32];
    int len = snprintf(response, sizeof(response), ":%d\r\n", current_val);
	send(client_fd, response, strlen(response), 0);
    
    //free(new_val_str);
    return;
}