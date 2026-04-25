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
        
        if(list == NULL){
            send(client_fd, "-ERR Out of memory\r\n", 20, 0);
            return;
        }

        list->size = 0;
        list->capacity = req->argc - 1;
        list->values = malloc(list->capacity * sizeof(char *));

        if(list->values == NULL){
            send(client_fd, "-ERR Out of memory\r\n", 20, 0);
            return;
        }


        store_set_list(req->args[0], list);
    }
    else{
        list = (List*)(entry->value);
        list->values = realloc(list->values, (list->size + req->argc - 1) * sizeof(char *));
    }

    for(int i = 1; i < req->argc; i++){
        list->values[list->size] = strdup(req->args[i]);
        list->size++;
    }
    
    char addMsg[1024];
    snprintf(addMsg, sizeof(addMsg), ":%zd\r\n", list->size);

    printf("addMsg: %s\n", addMsg);
    send(client_fd, addMsg, strlen(addMsg), 0);
    
}

void handle_lpush(RespRequest *req, int client_fd){
    if(req->argc < 2){
        send(client_fd, "-ERR wrong number of arguments for 'lpush' command\r\n", 52, 0);
        return;
    }
    List *list = NULL;
    Entry *entry = store_getEntry(req->args[0]);
    int newCount = req->argc - 1;

    if(entry == NULL || entry->value == NULL){
        list = calloc(1, sizeof(List));
        list->size = 0;
        list->capacity = newCount;
        list->values = malloc(list->capacity * sizeof(char *));
        store_set_list(req->args[0], list);
        if(list == NULL){
            send(client_fd, "-ERR Out of memory\r\n", 20, 0);
            return;
        }
    } else {
        list = (List*)(entry->value);
        list->values = realloc(list->values, (list->size + newCount) * sizeof(char *));
    }

    // shift existing elements right
    for(int i = list->size - 1; i >= 0; i--){
        list->values[i + newCount] = list->values[i];
    }
    // prepend in reverse order
    for(int i = 1; i <= newCount; i++){
        list->values[newCount - i] = strdup(req->args[i]);
    }
    list->size += newCount;

    char addMsg[1024];
    snprintf(addMsg, sizeof(addMsg), ":%zd\r\n", list->size);
    send(client_fd, addMsg, strlen(addMsg), 0);
}


void handle_lrange(RespRequest *req, int client_fd){
    if(req->argc < 3) return;

    List *list = NULL;
    Entry *entry = store_getEntry(req->args[0]);

    if(entry == NULL || entry->value == NULL){
        printf("DEBUG: doesnt exist or value is null\n");
        send(client_fd, "*0\r\n", 4, 0);
        return;
    }
    else{
        list = (List*)entry->value;
    }

    int startIndex = atoi(req->args[1]);
    int endIndex = atoi(req->args[2]);

    printf("startIndex: %d, endIndex: %d\n", startIndex, endIndex);
    printf("startIndex > endIndex %d\n", startIndex > endIndex);

    if(abs(startIndex) > list->size){
        startIndex = 0;
    }
    if(abs(endIndex) > list->size){
        startIndex = 0;
    }

    //Handle neg index cases
    if(startIndex < 0) startIndex = list->size - abs(startIndex);
    if(endIndex < 0) endIndex = list->size - abs(endIndex);

    if(startIndex >= list->size || startIndex > endIndex){
        send(client_fd, "*0\r\n", 4, 0);
        return;
    }

    if(startIndex == endIndex){
        char value[1024];
        snprintf(value, sizeof(value), "*1\r\n$%zd\r\n%s\r\n", strlen(list->values[startIndex]), list->values[startIndex]);

        send(client_fd, value, strlen(value), 0);
        return;
    }

    if(endIndex >= list->size){
        endIndex = list->size - 1;
    }

    char listValue[2048];
    int offset = snprintf(listValue, sizeof(listValue), "*%d\r\n", endIndex - startIndex + 1);
    for(int i = startIndex; i <= endIndex; i++){
        int written = snprintf(listValue + offset, sizeof(listValue) - offset, "$%zd\r\n%s\r\n",
         strlen(list->values[i]), list->values[i]);

        if (written > 0 && offset + written < sizeof(listValue)) {
            offset += written;
        } else {
            break;
        }
    }

    send(client_fd, listValue, strlen(listValue), 0);
}


