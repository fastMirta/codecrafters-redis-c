#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <time.h>
#include <errno.h>
#include "handler.h"
#include "parser.h"
#include "utils.h"


extern Client *clients[MAX_CLIENTS];

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

char* wrapXreadResponse(const char* key, const char* messages, int msgCount) {
    if (msgCount == 0) return strdup("*0\r\n");

    char *finalResponse = malloc(strlen(messages) + strlen(key) + 64); 
    if (!finalResponse) return NULL;

    sprintf(finalResponse, "*1\r\n*2\r\n$%zu\r\n%s\r\n*%d\r\n%s", 
            strlen(key), key, msgCount, messages);
    
    return finalResponse;
}


void try_deliver_stream_data(Client *client) {
    if (!client->is_blocked || !client->waiting_for_key || !client->min_id) return;

    int count = 0;
    char *messages = streamEntry_XREAD_toString(client->min_id, "+", client->waiting_for_key, &count);

    if (messages != NULL && count > 0) {
        char *response = wrapXreadResponse(client->waiting_for_key, messages, count);
        if (response) {
            send(client->fd, response, strlen(response), 0);
            
            client->is_blocked = 0;
            free(client->waiting_for_key);
            client->waiting_for_key = NULL;
            free(client->min_id);
            client->min_id = NULL;

            free(response);
        }
        free(messages);
        printf("Woke up blocked client on FD %d with %d messages\n", client->fd, count);
    } else {
        if (messages) free(messages);
    }
}


int validate_stream_id(RespRequest *req, StreamEntry *tail, char **errorMsg){
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
    
    if(!isBigger(tail->id, req->args[1])){
        *errorMsg = "-ERR The ID specified in XADD is equal or smaller than the target stream top item\r\n";
        return 1;
    }
    printf("is BIGGER %d\n", isBigger(tail->id, req->args[1]));
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
    
    Stream *stream = NULL;
    Entry *entry = store_getEntry(req->args[0]);

    if(entry == NULL){
        stream = calloc(1, sizeof(Stream));
        if(stream == NULL){
            errorResp = "-ERR Out of memory\r\n";
            send(client_fd, errorResp, strlen(errorResp), 0);
            free(errorResp);
            return;
        }
    } else {
        stream = (Stream*) entry->value;
    }

    StreamEntry *streamEntry = malloc(sizeof(StreamEntry));
    streamEntry->id = strdup(req->args[1]);
    streamEntry->next = NULL;

    if(stream->tail != NULL){
        printValue(stream->tail);
        if(validate_stream_id(req, stream->tail, &errorResp) == 1){
            printf("error respond: %s\n", errorResp);
            send(client_fd, errorResp, strlen(errorResp), 0);
            return;
        }
    }

    if(strchr(req->args[1], '*') != NULL){
        generateId(stream, streamEntry);
    }




    long long request_id_ms, request_id_seq;
    sscanf(streamEntry->id, "%lld-%lld", &request_id_ms, &request_id_seq);

    streamEntry->field_count = req->argc - 2;
    streamEntry->fields = malloc(sizeof(char **) * (req->argc - 2));
    for(int i = 2; i < req->argc; i++){
        streamEntry->fields[i-2] = strdup(req->args[i]);
    }

    if(stream->head == NULL){
        stream->head = streamEntry;
        stream->tail = streamEntry;
        stream->last_ms = request_id_ms;
        stream->last_seq = request_id_seq;
        stream->length++;
        store_set_stream(req->args[0], stream);
    } else {
        stream->tail->next = streamEntry;
        stream->tail = streamEntry;
        stream->last_ms = request_id_ms;
        stream->last_seq = request_id_seq;
        stream->length++;
    }


    char response[128];
    int len = snprintf(response, sizeof(response), "$%zu\r\n%s\r\n", strlen(streamEntry->id), streamEntry->id);
    send(client_fd, response, len, 0);

    printf("Boyya\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        //printf("client is null? %d\n", clients[i] == NULL);
        if (clients[i] && clients[i]->is_blocked) {
            printf("DEBUG: Found blocked client on FD %d waiting for %s. XADD key is %s\n", 
                clients[i]->fd, clients[i]->waiting_for_key, req->args[0]);
            if (clients[i]->waiting_for_key != NULL && strcmp(clients[i]->waiting_for_key, req->args[0]) == 0) {
                printf("DEBUG: Match found! Calling try_deliver...\n");
                try_deliver_stream_data(clients[i]);
            }
        }

        if(clients[i]){
            printf("is blocked? %d\n", clients[i]->is_blocked);
        }
    }
}

int get_length(char *array[]){
    int count = 0;
    while (array[count] != NULL) {
        count++;
    }
    printf("COUNT: %d\n", count);
    return count;
}


void handle_set(RespRequest *req, RedisType type, int client_fd){
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

/**Returns a the string of the given string with all upper case letter
 * 
 */
char* to_upper(char *stringToUpper){
    char *newString = malloc(strlen(stringToUpper) + 1);
    for(int i = 0; i < strlen(stringToUpper); i++){
        newString[i] = toupper(stringToUpper[i]);
    }
    return newString;
}

int is_multiple_key(RespRequest *req, int *streamsLength, int *streamIndex){
    int index = 0;
    char **contents = NULL;
    while(strcmp(to_upper(req->args[index]), "STREAMS") != 0){
        index++;
    }
    printf("num: %d\n", req->argc - index - 1);
    if((req->argc - index - 1) % 2 == 0 && (req->argc - index - 1) > 2){
        *streamIndex = index;
        *streamsLength = (req->argc - index - 1)/2;
        return 0;
    }
    //return ((req->argc - index + 1) % 2 != 0 && index + 1 > 2); //1 ezogi 0 zogi
    return 1;
}


/**Checks if the specified request contains Block
 * 1 = false
 * 0 = true
 */
int hasBlock(RespRequest *request, int *blockIndex){
    char *wordInUpper = NULL;
    for(int i = 0; i < request->argc; i++){
        wordInUpper = to_upper(request->args[i]);
        printf("normal word: %s\n", request->args[i]);
        printf("upper word: %s\n", wordInUpper);
        if(strstr(wordInUpper, "BLOCK") && i < request->argc - 1){
            *blockIndex = i;
            free(wordInUpper);
            printf("returned 0");
            return 0;
        }
        free(wordInUpper);
    }
    printf("false aka 1\n");
    return 1;
}

/**Builds appropriate response for xread with multiple streams
 * 
 */
void handle_multiple_xread(RespRequest *req, char **response, int streamIndex, int streamsLength){
    streamIndex++;
    char **keyArray = malloc(streamsLength/2 * sizeof(char*)),
        **idArray = malloc(streamsLength/2 * sizeof(char*));
    
    printf("Stream length in multiple: %d\n", streamsLength);
    for(int i = streamIndex; i < streamsLength + streamIndex; i++){
        keyArray[i - streamIndex] = malloc(strlen(req->args[i]) + 1);
        strcpy(keyArray[i - streamIndex], req->args[i]);
    }
    for(int i = streamIndex + streamsLength; i < 2 * streamsLength + streamIndex; i++){
        idArray[i - streamIndex - streamsLength] = malloc(strlen(req->args[i]) + 1);
        strcpy(idArray[i - streamIndex - streamsLength], req->args[i]);
        printf("args[%d] = %s\n", i, req->args[i]);
        printf("idArray[%d] = %s\n", i - streamIndex - streamsLength, idArray[i - streamIndex - streamsLength]);
    }
    printf("ENTERED THE GOAT 1 AND only XREAD WITH mul @param\n");
    *response = streamEntry_XREAD_Mul_toString(streamsLength, keyArray, idArray);
}

/**Handles single xread cmd (with or without block keyword)
 * False = 1
 * True = 0
 */
int handle_single_xread(RespRequest *req, Client *client, int *count, char **response){
    //TODO: refactor this code change it to couple functions
    Entry *entry = NULL;
    char *temp_messages = NULL;
    int blockIndex = 0;
    
    printf("INSIDE single xread\n");
    if(hasBlock(req, &blockIndex) == 0){
        printf("index 5: %s\n", req->args[3]);
        printf("index 6: %s\n", req->args[4]);
        printf("get_current_time_ms() + ms_timeout: %lld\n",get_current_time_ms() + atoi(req->args[blockIndex + 1]));
        int ms = atoi(req->args[blockIndex + 1]);
        if(ms == 0){
            client->timeout_at = 0;
        }
        else{
            client->timeout_at = get_current_time_ms() + ms;
        }

        client->is_blocked = 1;
        client->min_id = strdup(req->args[4]);
        client->waiting_for_key = strdup(req->args[3]);

        entry = store_getEntry(req->args[3]);
        if(entry == NULL){
            printf("Entry wasnt FOUND in block\n");
            //send(client->fd, "*0\r\n", 4, 0);
            printf("min_id in single: %s\n", client->min_id);
            return 1;
        }
        
        printf("entry is not null\n");

        temp_messages = streamEntry_XREAD_toString(req->args[4], "+", req->args[3], count);
        printf("Temp MESSAGES: %s\n", temp_messages);
        if (temp_messages != NULL && strcmp(temp_messages, "") != 0 && *count > 0) {
            *response = wrapXreadResponse(req->args[3], temp_messages, *count);
            printf("ZERO TO HERO");
            free(temp_messages);
            return 0;
        }
        else{
            printf("min_id in single and mingle: %s\n", client->min_id);
            return 1;
        }

        if(temp_messages) free(temp_messages);


        
        printf("Entered block on single xread");
        return 1;
        
    }
    else{
        printf("Everything is broken: %s\n", req->args[1]);
        entry = store_getEntry(req->args[1]);
        if(entry == NULL){
            printf("Entry wasnt FOUND\n");
            send(client->fd, "*0\r\n", 4, 0);
            return 1;
        }
        printf("entry is not null\n");

        temp_messages = streamEntry_XREAD_toString(req->args[2], "+", req->args[1], count);
        

        if (count == 0 || temp_messages == NULL) {
            send(client->fd, "*0\r\n", 4, 0);
            if(temp_messages) free(temp_messages);
            return 1;
        }
    }
    
    
    *response = wrapXreadResponse(req->args[1], temp_messages, *count);
    free(temp_messages);   
    return 0;
}

/**Build client if found a block
 * @return 1 if failed or 0 if succeed
 */
int build_client(Client *client, RespRequest *req, int *blockIndex, int *count, char **response){
    if(hasBlock(req, blockIndex) == 0 && *count == 0){
        int ms_timeout = atoi(req->args[*blockIndex + 1]);
        if(ms_timeout == 0){
            client->timeout_at = 0;
        }
        else{
            client->timeout_at = get_current_time_ms() + ms_timeout;
        }

        client->is_blocked = 1;
        client->min_id = strdup(req->args[2]);
        client->waiting_for_key = strdup(req->args[1]);
        
        printf("min_id: %s\n", client->min_id);
        printf("Entered block on multiple  xread");
        if(response) free(response);
        return 1;
        
    }
    return 0;
}

/**Handles Xread cmd (single and multiple)
 * @return 0 if worked 1 if didnt
 */
int handle_xread(RespRequest *req, Client *client, char **response, int *count){
    //TODO: put the Block search here and give it as a ptr to these funcs (if no block ptr will be null and need to be free)
    int blockIndex = 0;
    int streamsLength = 0;
    int streamIndex = 0;
    int isMul = is_multiple_key(req, &streamsLength, &streamIndex);
    printf("is even? %d\n", isMul);
    printf("%d\n", streamsLength > 0 && streamIndex > 0);
    printf("length: %d, index: %d\n", streamsLength, streamIndex);
    if(isMul == 0 && streamsLength > 0 && streamIndex >= 0){
        handle_multiple_xread(req, response, streamIndex, streamsLength);
        if(*response != NULL){
            return 0;
        }
        return build_client(client, req, &blockIndex, count, response);
    }
    else{
        printf("Finished single xread\n");
        return handle_single_xread(req, client, count, response);
    }
}

void handle_stream_print(RespRequest *req, Client *client, REDIS_CMDS cmd){
    printRequest(req);
    int count = 0;
    int blockIndex = 0;
    printf("Key given: %s\n", req->args[0]);
    char *response = NULL;
    
    if(cmd == XRANGE){
        response = streamEntry_toString(req->args[1], req->args[2], req->args[0], &count);
    }
    else{
        int result = handle_xread(req, client, &response, &count);
        if(result == 1){
            printf("Return cuz xread is false\n");
            return;
        }
        printf("Didnt return\n");
    }
    
    printf("response: %s\n", response);
    if(response == NULL){
        printf("inside null\n");
        char *emptyArray = "*0\r\n";
        send(client->fd, "-ERR Internal error\r\n", 21, 0);
        send(client->fd, emptyArray, strlen(emptyArray), 0);
        return;
    }
    printf("response length: %lu\n", strlen(response));
    printf("outside null\n");
    int test = send(client->fd, response, strlen(response), 0);
    free(response);
}

int handle(RespRequest *req, Client *client) {
    if (req == NULL) {
        printf("ERROR IN HANDLE: Request is NULL\n");
        send(client->fd, "-ERR Internal error\r\n", 21, 0);
        return 1;
    }

    printf("\n--- Entered Handler: Command ID %d ---\n", req->command);

    // Utility cmds
    if (req->command == ECHO) {
        handle_echo(req, client->fd);
        return 0;
    }
    if (req->command == PING) {
        handle_ping(client->fd);
        return 0; 
    }
    if (req->command == AUTH || req->command == SELECT || req->command == COMMAND) {
        send(client->fd, "+OK\r\n", 5, 0);
        return 0; 
    }

    // Core cmds
    if (req->command == SET) {
        printf("Executing SET\n");
        handle_set(req, TYPE_STRING, client->fd);
        return 0;
    }
    if (req->command == GET) {
        printf("Executing GET\n");
        handle_get(req, client->fd);
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
        handle_type(req, client->fd);
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
        handle_set(req, TYPE_LIST, client->fd);
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
        handle_set(req, TYPE_STREAM, client->fd);
        return 0;
    }
    if (req->command == XREAD){ 
        printf("Entered XREAD\n");
        handle_stream_print(req, client, XREAD);
        return 0; 
    }
    if (req->command == XRANGE){
        printf("ENTERED XRANGE\n");
        handle_stream_print(req, client, XRANGE);
        return 0;
    }
    if (req->command == XGROUP)    { return 0; }
        
    
    // Default: UNKNOWN
    printf("Unknown command received\n");
    send(client->fd, "-ERR unknown command\r\n", 22, 0);
    return 1;
}








