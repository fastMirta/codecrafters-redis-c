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

    while (curr != NULL) {
        if (curr->score < score) {
            prev = curr;
            curr = curr->next;
        } else if (curr->score == score && strcmp(curr->member, member) < 0) {
            prev = curr;
            curr = curr->next;
        } else {
            break;
        }
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
    char *member = req->args[2];

    int existed = zset_remove(sortedSet, member);

    zset_add(sortedSet, newScore, member);

    if (existed) {
        send(client_fd, ":0\r\n", 4, 0);
    } else {
        send(client_fd, ":1\r\n", 4, 0);
    }
}

void handle_zrank(RespRequest *req, int client_fd){
    if(req->argc < 2){return;}
    printf("\nNot smaller\n");
    
    Entry *entry = store_getEntry(req->args[0]);
    if (entry == NULL || entry->value == NULL) {
        send(client_fd, "$-1\r\n", 5, 0);
        return;
    }
    
    ZSet *sortedSet = (ZSet*) entry->value;
    ZSetEntry *ptr = sortedSet->head;

    for (int i = 0; i < sortedSet->length; i++) {
        if (ptr == NULL) break;
        
        if (strcmp(req->args[1], ptr->member) == 0) {
            char rankMsg[64];
            int len = snprintf(rankMsg, sizeof(rankMsg), ":%d\r\n", i);
            send(client_fd, rankMsg, len, 0);
            return;
        }
        ptr = ptr->next;
    }

    send(client_fd, "$-1\r\n", 5, 0);
}

void handle_zrange(RespRequest *req, int client_fd){
    printf("\n Enter to z range!!!!");
    printf("argc: %d\n", req->argc);
    if(req->argc < 3){return;}

    Entry *entry = store_getEntry(req->args[0]);
    if (entry == NULL || entry->value == NULL) {
        send(client_fd, "*0\r\n", 4, 0);
        return;
    }
    
    ZSet *sortedSet = (ZSet*) entry->value;
    ZSetEntry *ptr = sortedSet->head;

    int startIndex = atoi(req->args[1]);
    int endIndex = atoi(req->args[2]);
    int setLen = sortedSet->length;

    if (startIndex < 0) startIndex += setLen;
    if (endIndex < 0) endIndex += setLen;
    
    if (startIndex < 0) startIndex = 0;
    if (endIndex >= setLen) endIndex = setLen - 1;
    if (startIndex > endIndex || startIndex >= setLen) {
        send(client_fd, "*0\r\n", 4, 0);
        return;
    }

    char members[4096];

    for(int i = 0; i < startIndex; i++){
        ptr = ptr->next;
    }

    int offset = snprintf(members, sizeof(members), "*%d\r\n", endIndex - startIndex + 1);
    for(int i = startIndex; i <= endIndex; i++){
        int written = snprintf(members + offset, sizeof(members) - offset, "$%zd\r\n%s\r\n", strlen(ptr->member), ptr->member);
        ptr = ptr->next;
        if (written > 0 && offset + written < sizeof(members)) {
            offset += written;
        } else {
            break;
        }
    }
    send(client_fd, members, offset, 0);
}

/*Sends to the client the length of the sorted set, if not exist sends 0*/
void handle_zcard(RespRequest *req, int client_fd){
    if(req->argc < 1){return;}

    Entry *entry = store_getEntry(req->args[0]);
    if (entry == NULL || entry->value == NULL) {
        send(client_fd, ":0\r\n", 4, 0);
        return;
    }

    ZSet *sortedSet = (ZSet*) entry->value;
    char sortLength[128];
    snprintf(sortLength, sizeof(sortLength), ":%zd\r\n", sortedSet->length);
    send(client_fd, sortLength, strlen(sortLength), 0);
}

void handle_zscore(RespRequest *req, int client_fd){
    if(req->argc < 2){return;}

    Entry *entry = store_getEntry(req->args[0]);
    if (entry == NULL || entry->value == NULL) {
        send(client_fd, "$-1\r\n", 5, 0);
        return;
    }

    ZSet *sortedSet = (ZSet*) entry->value;
    ZSetEntry *ptr = sortedSet->head;
    for(int i = 0; i < sortedSet->length; i++){
        if(strcmp(req->args[1], ptr->member) == 0){
            char memberScore[128];
            char scoreLength[64];
            snprintf(scoreLength, sizeof(scoreLength), "%.17g", ptr->score);
            snprintf(memberScore, sizeof(memberScore), "$%zd\r\n%s\r\n", strlen(scoreLength), scoreLength);
            send(client_fd, memberScore, strlen(memberScore), 0);
            return;
        }
        ptr = ptr->next;
    }
    send(client_fd, "$-1\r\n", 5, 0);
}


//ZREVRANK:
/*void handle_zrank(RespRequest *req, int client_fd){
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
}*/