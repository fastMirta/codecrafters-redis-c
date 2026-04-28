#include "list_hander.h"

void printList(List *list){
    printf("Size: %zd\n", list->size);
    for(int i = 0; i < list->size; i++){
        printf("value[%d] = %s\n", i, list->values[i]);
    }
}

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


        Client *longestTimeout = NULL;
    double currentBiggestTimeOut = 0;
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i] && clients[i]->is_blpop && clients[i]->is_blocked
                && clients[i]->waiting_for_key != NULL
                && strcmp(clients[i]->waiting_for_key, req->args[0]) == 0){


            double clientWaitingTime = fabs(get_current_time_ms() - clients[i]->blocked_at);

            if(clientWaitingTime > currentBiggestTimeOut){
                currentBiggestTimeOut = clientWaitingTime;
                longestTimeout = clients[i];
            }
        }
        
    }
    if(longestTimeout != NULL){
        char response[1024];
        snprintf(response, sizeof(response),
            "*2\r\n$%zu\r\n%s\r\n$%zu\r\n%s\r\n",
            strlen(req->args[0]), req->args[0],
            strlen(list->values[0]), list->values[0]);
        send(longestTimeout->fd, response, strlen(response), 0);

        // Shift list left
        for (int j = 0; j < (int)list->size - 1; j++)
            list->values[j] = list->values[j + 1];
        list->size--;

        longestTimeout->is_blocked = 0;
        longestTimeout->is_blpop = 0;
        free(longestTimeout->waiting_for_key);
        longestTimeout->waiting_for_key = NULL;
    }
    
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

    //TODO: change it into brpop
    // for (int i = 0; i < MAX_CLIENTS; i++) {
    //     if (clients[i] && clients[i]->is_blpop && clients[i]->is_blocked
    //             && clients[i]->waiting_for_key != NULL
    //             && strcmp(clients[i]->waiting_for_key, req->args[0]) == 0) {

    //         // Deliver the first element
    //         char response[1024];
    //         snprintf(response, sizeof(response),
    //             "*2\r\n$%zu\r\n%s\r\n$%zu\r\n%s\r\n",
    //             strlen(req->args[0]), req->args[0],
    //             strlen(list->values[0]), list->values[0]);
    //         send(clients[i]->fd, response, strlen(response), 0);

    //         // Shift list right by 1
    //         for(int j = list->size - 1; j >= 0; j--){
    //             list->values[j + 1] = list->values[j];
    //         }
    //         list->size--;

    //         clients[i]->is_blocked = 0;
    //         clients[i]->is_blpop = 0;
    //         free(clients[i]->waiting_for_key);
    //         clients[i]->waiting_for_key = NULL;
    //         break;
    //     }
    // }

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
        printList(list);
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
    printList(list);
    send(client_fd, listValue, strlen(listValue), 0);
}


/**Sends the client the length of the given list */
void handle_llen(RespRequest *req, int client_fd){
    if(req->argc < 1) return;

    List *list = NULL;
    Entry *entry = store_getEntry(req->args[0]);

    if(entry == NULL || entry->value == NULL){
        printf("DEBUG: doesnt exist or value is null\n");
        send(client_fd, ":0\r\n", 4, 0);
        return;
    }
    else{
        list = (List*)entry->value;
    }

    char listLength[1024];
    snprintf(listLength, sizeof(listLength), ":%zd\r\n", list->size);

    send(client_fd, listLength, strlen(listLength), 0);

}


void handle_lpop(RespRequest *req, int client_fd){
    if(req->argc < 1) return;
    List *list = NULL;
    Entry *entry = store_getEntry(req->args[0]);
    if(entry == NULL || entry->value == NULL){
        send(client_fd, "$-1\r\n", 5, 0);
        return;
    }
    list = (List*)entry->value;
    if(list->size == 0){
        send(client_fd, "*0\r\n", 4, 0);
        return;
    }

    char deletedValues[2048];
    if(req->argc == 2){
        int count = atoi(req->args[1]);
        if(count < 0){
            send(client_fd, "-ERR value is out of range, must be positive\r\n", 46, 0);
            return;
        }
        if(count == 0){
            send(client_fd, "*0\r\n", 4, 0);
            return;
        }
        if(count > (int)list->size) count = list->size;

        int offset = snprintf(deletedValues, sizeof(deletedValues), "*%d\r\n", count);
        for(int i = 0; i < count; i++){
            int written = snprintf(deletedValues + offset, sizeof(deletedValues) - offset,
                "$%zd\r\n%s\r\n", strlen(list->values[i]), list->values[i]);
            if(written > 0) offset += written;
        }
        send(client_fd, deletedValues, offset, 0);

        // shift left
        for(int i = 0; i < (int)list->size - count; i++){
            list->values[i] = list->values[i + count];
        }
        list->size -= count;
        return;
    }

    // single pop
    snprintf(deletedValues, sizeof(deletedValues), "$%zd\r\n%s\r\n",
        strlen(list->values[0]), list->values[0]);
    send(client_fd, deletedValues, strlen(deletedValues), 0);
    for(int i = 0; i < (int)list->size - 1; i++){
        list->values[i] = list->values[i + 1];
    }
    list->size--;
}

//TODO: realloc the list->values to free unused space
void handle_rpop(RespRequest *req, int client_fd){
    if(req->argc < 1) return;

    List *list = NULL;
    Entry *entry = store_getEntry(req->args[0]);
    if(entry == NULL || entry->value == NULL){
        send(client_fd, "$-1\r\n", 5, 0);
        return;
    }

    list = (List*)entry->value;
    if(list->size == 0){
        send(client_fd, "*0\r\n", 4, 0);
        return;
    }

    char deletedValues[2048];
    if(req->argc == 2){
        int count = atoi(req->args[1]);
        if(count < 0){
            send(client_fd, "-ERR value is out of range, must be positive\r\n", 46, 0);
            return;
        }
        if(count == 0){
            send(client_fd, "*0\r\n", 4, 0);
            return;
        }
        if(count > (int)list->size) count = list->size;

        int offset = snprintf(deletedValues, sizeof(deletedValues), "*%d\r\n", count);

        // read from end
        for(int i = list->size - count; i < (int)list->size; i++){
            int written = snprintf(deletedValues + offset, sizeof(deletedValues) - offset,
                "$%zd\r\n%s\r\n", strlen(list->values[i]), list->values[i]);
            if(written > 0) offset += written;
        }
        send(client_fd, deletedValues, offset, 0);
        list->size -= count;
        return;
    }

    // single pop from end
    char single[1024];
    snprintf(single, sizeof(single), "$%zd\r\n%s\r\n",
        strlen(list->values[list->size - 1]), list->values[list->size - 1]);
    send(client_fd, single, strlen(single), 0);
    list->size--;
}


void handle_blpop(RespRequest *req, Client *client){
       if(req->argc < 2) return;
    
    Entry *entry = store_getEntry(req->args[0]);
    
    // Key exists and has elements = pop immediately
    if(entry != NULL && entry->value != NULL){
        List *list = (List*)(entry->value);
        if(list->size > 0){
            char response[1024];
            snprintf(response, sizeof(response),
                "*2\r\n$%zu\r\n%s\r\n$%zu\r\n%s\r\n",
                strlen(req->args[0]), req->args[0],
                strlen(list->values[0]), list->values[0]);
            send(client->fd, response, strlen(response), 0);
            for(int i = 0; i < (int)list->size - 1; i++)
                list->values[i] = list->values[i + 1];
            list->size--;
            return;
        }
    }
    
    // Key doesn't exist or list is empty = block
    double sec = atof(req->args[req->argc - 1]);
    client->timeout_at = (sec == 0.0) ? 0 : get_current_time_ms() + (sec * 1000);
    client->is_blpop = 1;
    client->is_blocked = 1;
    client->blocked_at = get_current_time_ms();
    client->waiting_for_key = strdup(req->args[0]);
    
}


void handle_endblpop(Client *client){
    long long now = get_current_time_ms();

    if(client && client->is_blpop
                    && client->timeout_at != 0
                    && now >= client->timeout_at){
                
        
        if(hasValue(TYPE_LIST, client->waiting_for_key) == 0){
            
            Entry *entry = store_getEntry(client->waiting_for_key);
            List *list = (List*)entry->value;

            char buffer[1024];
            snprintf(buffer, sizeof(buffer), "*2\r\n$%zd\r\n%s\r\n$%zd\r\n%s\r\n",
            strlen(client->waiting_for_key), client->waiting_for_key, strlen(list->values[0]), list->values[0]);

            send(client->fd, buffer, strlen(buffer), 0);
        }
        else{
            printf("nil in end blpop\n");
            send(client->fd, "*-1\r\n", 5, 0);
        }
        
        client->is_blocked = 0;
        client->is_blpop = 0;
    }
}