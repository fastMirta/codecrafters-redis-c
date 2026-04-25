#include "list_hander.h"

void handle_rpush(RespRequest *req, int client_fd){
    if(req->argc < 2){
        send(client_fd, "-ERR wrong number of arguments for 'rpush' command\r\n", 52, 0);
        return;
    }

    List *list = NULL;
    Entry *entry = store_getEntry(req->args[0]);
    if(entry == NULL || entry->value == NULL){
        list = calloc(1, sizeof(List));
        list->size = 0;
        list->current = 0;
        list->capacity = req->argc - 1;
        list->values = malloc(list->capacity * sizeof(char *));


        store_set_list(req->args[0], list);
        //ist->capacity
        if(list == NULL){
            send(client_fd, "-ERR Out of memory\r\n", 20, 0);
            return;
        }
    }
    else{
        list = (List*)(entry->value);
        list->values = realloc(list->values, (list->current + req->argc - 1) * sizeof(char *));
    }

    for(int i = 1; i < req->argc; i++){
        list->values[list->current] = strdup(req->args[i]);
        list->current++;
    }
    
    list->size += list->current;
    char addMsg[1024];
    snprintf(addMsg, sizeof(addMsg), ":%zd\r\n", list->current);

    printf("addMsg: %s\n", addMsg);
    send(client_fd, addMsg, strlen(addMsg), 0);
    
}