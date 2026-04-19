#include "set_handler.h"



int zset_remove(ZSet *sortedSet, const char *member) {
    ZSetEntry *curr = sortedSet->head;
    ZSetEntry *prev = NULL;

    while (curr != NULL) {
        if (strcmp(curr->member, member) == 0) {
            if (prev == NULL) {
                sortedSet->head = curr->next;
            } else {
                prev->next = curr->next;
            }

            free(curr->member);
            free(curr);
            sortedSet->length--;
            return 1; 
        }
        prev = curr;
        curr = curr->next;
    }
    return 0;
}

void zset_add(ZSet *sortedSet, double score, const char *member) {
    ZSetEntry *newNode = malloc(sizeof(ZSetEntry));
    if (!newNode) return; 

    newNode->member = strdup(member);
    newNode->score = score;
    newNode->next = NULL;

    ZSetEntry *curr = sortedSet->head;
    ZSetEntry *prev = NULL;

    while (curr != NULL && curr->score < score) {
        prev = curr;
        curr = curr->next;
    }

    if (prev == NULL) {
        newNode->next = sortedSet->head;
        sortedSet->head = newNode;
    } else {
        newNode->next = curr;
        prev->next = newNode;
    }
    sortedSet->length++;
}

void handle_zadd(RespRequest *req, int client_fd){
    if(req->argc < 3){return;}
    printf("Not smaller\n");
    char *errorResp = NULL;
    
    ZSet *sortedSet = NULL;
    Entry *entry = store_getEntry(req->args[0]);
    
    if(entry == NULL){
        sortedSet = calloc(1, sizeof(ZSet));
        if(sortedSet == NULL){
            errorResp = "-ERR Out of memory\r\n";
            if (client_fd != server_config.master_fd)
                send(client_fd, errorResp, strlen(errorResp), 0);
            return;
        }
        store_set_zset(req->args[0], sortedSet);
    } else {
        sortedSet = (ZSet*) entry->value;
    }

    double newScore = atof(req->args[1]);
    if(sortedSet->length == 0){
        ZSetEntry *firstEntry = malloc(sizeof(ZSetEntry));
        
        firstEntry->next = NULL;
        firstEntry->score = newScore;
        firstEntry->member = strdup(req->args[2]);
        sortedSet->head = firstEntry;
        sortedSet->length++;

        send(client_fd, ":1\r\n", 4, 0);
        return;
    }

    ZSetEntry *zsortEntry = sortedSet->head;
    for(int i = 0; i < sortedSet->length; i++){
        if(zsortEntry->score > newScore && strcmp(req->args[2], zsortEntry->member) != 0){
            ZSetEntry *temp = zsortEntry->next;
            ZSetEntry *newValue = malloc(sizeof(ZSetEntry));
            if(newValue == NULL){
                printf("No place in memory\n");
                send(client_fd, "-ERR memory empty", 17, 0);
            }

            newValue->next = temp;
            newValue->score = newScore;
            newValue->member = strdup(req->args[2]);
            zsortEntry->next = newValue;
            sortedSet->length++;

            send(client_fd, ":1\r\n", 4, 0);
            return;
        }
        else if(zsortEntry->score < newScore && strcmp(req->args[2], zsortEntry->member) == 0){
            zsortEntry->score = newScore;
            send(client_fd, ":0\r\n", 4, 0);
            return;
        }
        else if(zsortEntry->score > newScore && strcmp(req->args[2], zsortEntry->member) == 0){
            ZSetEntry *toDelete = zsortEntry;
            zset_remove(sortedSet, req->args[2]); 
            zset_add(sortedSet, newScore, req->args[2]);
            send(client_fd, ":0\r\n", 4, 0);
            return;
        }
        if(zsortEntry->next == NULL){
            break;
        }
        zsortEntry = zsortEntry->next;
    }
    ZSetEntry *newEntry = malloc(sizeof(ZSetEntry));
    newEntry->next = NULL;
    newEntry->score = newScore;
    newEntry->member = strdup(req->args[2]);
    zsortEntry->next = newEntry;
    sortedSet->length++;

    send(client_fd, ":1\r\n", 4, 0);
    return;
}

void handle_zrank(RespRequest *req, int client_fd){
    if(req->argc < 2){return;}
    printf("\nNot smaller\n");
    char *errorResp = NULL;
    
    ZSet *sortedSet = NULL;
    Entry *entry = store_getEntry(req->args[0]);
    
    if(entry == NULL){
        sortedSet = calloc(1, sizeof(ZSet));
        if(sortedSet == NULL){
            errorResp = "-ERR Out of memory\r\n";
            if (client_fd != server_config.master_fd)
                send(client_fd, errorResp, strlen(errorResp), 0);
            return;
        }
    } else {
        sortedSet = (ZSet*) entry->value;
    }
    
    int isExist = 0;
    ZSetEntry *ptr = sortedSet->head;
    ZSetEntry *memberToRank = NULL;
    for(int i = 0; i < sortedSet->length; i++){
        if(strcmp(req->args[1], ptr->member) == 0){
            isExist = 1;
            memberToRank = ptr;
        }
        ptr = ptr->next;
    }

    // ptr = sortedSet->head;
    // for(int i = 0; i < sortedSet->length; i++){
    //     printf("member: %s\n", ptr->member);
    //     ptr = ptr->next
            
    // }

    if(!isExist){
        printf("Didnt found ya \n");
        send(client_fd, "$-1\r\n", 5, 0);
        return;
    }

    printf("found ya\n");
    if(sortedSet->head == NULL){
        printf("found ya but no values");
        send(client_fd, "$-1\r\n", 5, 0);
        return;
    }

    if(sortedSet->length == 1){
        send(client_fd, ":0\r\n", 4, 0);
        return;
    }

    //ZRANK zset_key member_with_score_1
    ptr = sortedSet->head;
    for(int i = 0; i < sortedSet->length; i++){
        if(ptr->score >= memberToRank->score && strcmp(ptr->member, memberToRank->member) != 0){
            char rankMsg[1024];
            snprintf(rankMsg, sizeof(rankMsg), ":%zd\r\n", sortedSet->length - i - 1);
            send(client_fd, rankMsg, strlen(rankMsg), 0);
            return;
        }
        
        ptr = ptr->next;
    }
    char lastRank[64];
    snprintf(lastRank, sizeof(lastRank), ":%zd\r\n", sortedSet->length - 1);

    send(client_fd, lastRank, strlen(lastRank), 0);
}
