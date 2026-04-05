#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <time.h>
#include "utils.h"
#include <errno.h>


int handle_set_flags(RespRequest *req, int *expireAt, TIME_FLAGS *flag){
    printf("\n");
    printf("ENTERED FLAGS HANDLER\n");
    
    if(req->argc <= 1){
        printf("Doesnt have flags");
        return 1;
    }
    for(int i = 1; i < req->argc - 1; i++){
        printf("%s\n", req->args[i]);
        if(strcmp(req->args[i], "EX") == 0){
            *expireAt = atoi(req->args[i + 1]);
            *flag = EX;
            break;
        }
        else if(strcmp(req->args[i], "PX") == 0){
            *expireAt = atoi(req->args[i + 1]);
            *flag = PX;
            break;
        }
        
    }
    if(*flag == NO_TIME_FLAG){
        *expireAt = -1;
        return 0;
    }
    return 0;

    
}



void handle_ping(int client_fd){
    const char *ping_response = "+PONG\r\n";
    printf("Client fd: %d", client_fd);
    if(send(client_fd, ping_response, strlen(ping_response), 0) != -1){
        printf("SEND SUCCESS");
    }
    else{
        printf("\n");
        printf("SEND failed inside handle ping \n");
    }
}

void handle_echo(RespRequest *req, int client_fd){
    char response[1024];
    snprintf(response, sizeof(response), "$%zu\r\n%s\r\n", strlen(req->args[0]), req->args[0]);
	send(client_fd, response, strlen(response), 0);

}


int validate_stream_id(RespRequest *req, char **errorMsg){
    if(strcmp(req->args[1], "*") == 0){
        return 0;
    }

    if(strstr(req->args[1], "-") == NULL){
        *errorMsg = "-ERR Invalid stream ID specified\r\n";
        return 1;
    }

    char ms[64], seq[64];
    sscanf(req->args[1], "%63[^-]-%63s", ms, seq);

    if(strcmp(ms, "*") == 0){
        *errorMsg = "-ERR Invalid stream ID specified\r\n";
        return 1;
    }

    if(strcmp(seq, "*") == 0 && atoi(ms) >= 0){
        long long parsed_ms;
        if(sscanf(ms, "%lld", &parsed_ms) != 1){
            *errorMsg = "-ERR Invalid stream ID specified\r\n";
            return 1;
        }
        return 0;
    }

    long long request_id_ms, request_id_seq;
    if(sscanf(req->args[1], "%lld-%lld", &request_id_ms, &request_id_seq) != 2){
        *errorMsg = "-ERR Invalid stream ID specified\r\n";
        return 1;
    }

    if(request_id_ms == 0 && request_id_seq == 0){
        *errorMsg = "-ERR The ID specified in XADD must be greater than 0-0\r\n";
        return 1;
    }

    return 0;
}

void generateId(Stream *stream, StreamEntry *streamEntry) {
    long long now = get_current_time_ms();
    long long finalMs, finalSeq;
    char idToString[48];

    // partial auto-generate: "ms-*"
    if(strstr(streamEntry->id, "-*") != NULL){
        sscanf(streamEntry->id, "%lld-*", &finalMs);
        if(finalMs == stream->last_ms){
            finalSeq = stream->last_seq + 1;
        } else {
            finalSeq = (finalMs == 0) ? 1 : 0;
        }
    }
    // full auto-generate: "*"
    else {
        if(stream->head == NULL){
            finalMs = now;
            finalSeq = (finalMs == 0) ? 1 : 0;
        } else if(now > stream->last_ms){
            finalMs = now;
            finalSeq = 0;
        } else {
            finalMs = stream->last_ms;
            finalSeq = stream->last_seq + 1;
        }
    }

    snprintf(idToString, sizeof(idToString), "%lld-%lld", finalMs, finalSeq);
    free(streamEntry->id);
    streamEntry->id = strdup(idToString);
}

void handle_set_stream(RespRequest *req, int client_fd){
    char *errorResp = NULL;
    if(validate_stream_id(req, &errorResp) == 1){
        send(client_fd, errorResp, strlen(errorResp), 0);
        return;
    }

    Stream *stream = NULL;
    Entry *entry = store_getEntry(req->args[0]);

    if(entry == NULL){
        stream = calloc(1, sizeof(Stream));
        if(stream == NULL){
            char *err = "-ERR Out of memory\r\n";
            send(client_fd, err, strlen(err), 0);
            return;
        }
    } else {
        stream = (Stream*) entry->value;
    }

    StreamEntry *streamEntry = malloc(sizeof(StreamEntry));
    if(streamEntry == NULL){
        char *err = "-ERR Out of memory\r\n";
        send(client_fd, err, strlen(err), 0);
        return;
    }

    streamEntry->id = strdup(req->args[1]);
    streamEntry->next = NULL;

    if(strchr(req->args[1], '*') != NULL){
        generateId(stream, streamEntry);
    }

    long long request_id_ms, request_id_seq;
    sscanf(streamEntry->id, "%lld-%lld", &request_id_ms, &request_id_seq);

    streamEntry->field_count = req->argc - 2;
    streamEntry->fields = malloc(sizeof(char **) * (req->argc - 2));
    for(int i = 2; i < req->argc; i++){
        printf("req->args[%d] = %s\n", i-2, req->args[i]);
        streamEntry->fields[i-2] = malloc(strlen(req->args[i]) + 1);
        strcpy(streamEntry->fields[i - 2], req->args[i]);
    }

    if(stream->head == NULL){
        stream->head = streamEntry;
        stream->tail = streamEntry;
        stream->last_ms = request_id_ms;
        stream->last_seq = request_id_seq;
        stream->length++;
        store_set_stream(req->args[0], stream);
    } else {
        if(request_id_ms < stream->last_ms || (request_id_ms == stream->last_ms && request_id_seq <= stream->last_seq)){
            char *err = "-ERR The ID specified in XADD is equal or smaller than the target stream top item\r\n";
            send(client_fd, err, strlen(err), 0);
            return;
        }
        stream->tail->next = streamEntry;
        stream->tail = streamEntry;
        stream->last_ms = request_id_ms;
        stream->last_seq = request_id_seq;
        stream->length++;
    }

    char response[128];
    int len = snprintf(response, sizeof(response), "$%zu\r\n%s\r\n", strlen(streamEntry->id), streamEntry->id);
    send(client_fd, response, len, 0);
}

int get_length(char *array[]){
    int count = 0;
    while (array[count] != NULL) {
        count++;
    }
    printf("COUNT: %d\n", count);
    return count;
}


void handle_set(RespRequest *req, int client_fd, RedisType type){
    if(req->argc < 2){
        printf("length smaller than 2");
        return;
    }
    if(type == TYPE_STREAM){
        handle_set_stream(req, client_fd);
    }
    else{
        TIME_FLAGS time = NO_TIME_FLAG;
        int seconds = 0;
        if(handle_set_flags(req, &seconds, &time) == 0){
            store_set(req->args[0], req->args[1], time, seconds, type);
            send(client_fd, "+OK\r\n", 5, 0);
        }
        else{
            send(client_fd, "+FAIL\r\n", 7, 0);
        }
    }
    

}

void handle_get(RespRequest *req, int client_fd) {
    char response[1024];
    char *value = store_get(req->args[0]); 
    if (value == NULL) {
        send(client_fd, "$-1\r\n", 5, 0);  
        return;
    }
    snprintf(response, sizeof(response), "$%d\r\n%s\r\n", (int)strlen(value), value);
    send(client_fd, response, strlen(response), 0);
}

void handle_type(RespRequest *req, int client_fd){
    char response[1024];
    Entry *entry = store_getEntry(req->args[0]);
    if(entry == NULL){
        send(client_fd, "+none\r\n", 7, 0);
        return;
    }
    switch (entry->type) {
        case TYPE_STRING: send(client_fd, "+string\r\n", 9, 0); break;
        case TYPE_LIST:   send(client_fd, "+list\r\n", 7, 0);   break;
        case TYPE_HASH:   send(client_fd, "+hash\r\n", 7, 0);   break;
        case TYPE_ZSET:   send(client_fd, "+zset\r\n", 7, 0);   break;
        case TYPE_SET:    send(client_fd, "+set\r\n", 6, 0);    break;
        case TYPE_STREAM: send(client_fd, "+stream\r\n", 9, 0); break;
        default:          send(client_fd, "+none\r\n", 7, 0);   break;
    }
}

void handle_unknown(RespRequest *req, int client_fd){

}

void printRequest(RespRequest *req){
    printf("PRINTING REQUESTTT\n");
    printf("argc: %d\n", req->argc);
    for(int i = 0; i < req->argc; i++){
        printf("Args: %s\n", req->args[i]);
    }
}

void handle_xrange(RespRequest *req, int client_fd, REDIS_CMDS cmd){
    printRequest(req);
    int count = 0;
    printf("Key given: %s\n", req->args[0]);
    char *response = NULL;
    if(cmd == XRANGE){
        response = streamEntry_toString(req->args[1], req->args[2], req->args[0], &count);
    }
    else{
        Entry *entry = store_getEntry(req->args[1]);
        if(entry == NULL){
            printf("Entry wasnt FOUND\n");
            send(client_fd, "*0\r\n", 4, 0);
            return;
        }
        printf("entry is not null\n");
        Stream *stream = ((Stream*)entry->value);
        /**    Stream *stream = (Stream*)entry->value;
               StreamEntry *newPtr = stream->head; */
        StreamEntry *nextPtr = stream->head;
        while(isBigger(req->args[2], nextPtr->id) != 1){//redis-cli XREAD streams stream_key 0-0
            nextPtr = nextPtr->next;
        }
        if(nextPtr == NULL){
            send(client_fd, "*0\r\n", 4, 0);
        }
        printf("Next IDDDDD: %s\n", nextPtr->id);
        response = streamEntry_XREAD_toString(nextPtr->id, "+", req->args[1], &count);
    }
    
    printf("response: %s\n", response);
    printf("response length: %lu\n", strlen(response));
    if(response == NULL){
        printf("inside null\n");
        char *emptyArray = "*0\r\n";
        send(client_fd, "-ERR Internal error\r\n", 21, 0);
        send(client_fd, emptyArray, strlen(emptyArray), 0);
        return;
    }
    printf("outside null\n");
    int test = send(client_fd, response, strlen(response), 0);
    free(response);
}

int handle(RespRequest *req, int client_fd) {
    if (req == NULL) {
        printf("ERROR IN HANDLE: Request is NULL\n");
        send(client_fd, "-ERR Internal error\r\n", 21, 0);
        return 1;
    }

    printf("\n--- Entered Handler: Command ID %d ---\n", req->command);

    // Utility cmds
    if (req->command == ECHO) {
        handle_echo(req, client_fd);
        return 0;
    }
    if (req->command == PING) {
        handle_ping(client_fd);
        return 0; 
    }
    if (req->command == AUTH || req->command == SELECT || req->command == COMMAND) {
        // Redis-cli שולח לעיתים פקודות אלו בהתחלה, נחזיר OK כדי לא לתקוע אותו
        send(client_fd, "+OK\r\n", 5, 0);
        return 0; 
    }

    // Core cmds
    if (req->command == SET) {
        printf("Executing SET\n");
        handle_set(req, client_fd, TYPE_STRING);
        return 0;
    }
    if (req->command == GET) {
        printf("Executing GET\n");
        handle_get(req, client_fd);
        return 0; 
    }
    if (req->command == DEL) {
        // handle_del(req, client_fd);
        return 0; 
    }
    if (req->command == EXISTS) {
        // handle_exists(req, client_fd);
        return 0; 
    }
    if (req->command == EXPIRE) {
        // handle_expire(req, client_fd);
        return 0; 
    }
    if (req->command == TTL) {
        // handle_ttl(req, client_fd);
        return 0;
    }
    if (req->command == TYPE) {
        printf("Executing TYPE\n");
        handle_type(req, client_fd);
        return 0;
    }

    // String & Number specific cmds
    if (req->command == INCR)   { return 0; }
    if (req->command == DECR)   { return 0; }
    if (req->command == APPEND) { return 0; }
    if (req->command == STRLEN) { return 0; }
    if (req->command == MGET)   { return 0; }

    // List cmds
    if (req->command == LPUSH) { 
        handle_set(req, client_fd, TYPE_LIST);
        return 0; 
    }
    if (req->command == RPUSH) { return 0; }
    if (req->command == LPOP)  { return 0; }
    if (req->command == RPOP)  { return 0; }

    // Hash cmds
    if (req->command == HSET)    { return 0; }
    if (req->command == HGET)    { return 0; }
    if (req->command == HGETALL) { return 0; }
    if (req->command == HDEL)    { return 0; }

    // Sets cmds
    if (req->command == SADD)      { return 0; }
    if (req->command == SREM)      { return 0; }
    if (req->command == SMEMBERS)  { return 0; }
    if (req->command == SISMEMBER) { return 0; }

    // Stream cmds
    if (req->command == XADD){ 
        handle_set(req, client_fd, TYPE_STREAM);
        return 0;
    }
    if (req->command == XREAD){ 
        printf("Entered XREAD\n");
        handle_xrange(req, client_fd, XREAD);
        return 0; 
    }
    if (req->command == XRANGE){
        printf("ENTERED XRANGE\n");
        handle_xrange(req, client_fd, XRANGE);
        return 0;
    }
    if (req->command == XGROUP)    { return 0; }
        
    
    // Default: UNKNOWN
    printf("Unknown command received\n");
    send(client_fd, "-ERR unknown command\r\n", 22, 0);
    return 1;
}








