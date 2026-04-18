#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>
#include "utility_handler.h"
#include "parser.h"
#include "utils.h"
#include "rdb_handler.h"
#include "channels_handler.h"



extern Client *clients[MAX_CLIENTS];

static inline int from_master(Client *client) {
    printf("client fd: %d\n", client->fd);
    printf("server fd: %d\n", server_config.master_fd);
    return client->fd == server_config.master_fd;
}

/**Copy src request to the dest request
 * @return ptr to dest request
 */
RespRequest* copy_request(RespRequest *src) {
    RespRequest *dest = malloc(sizeof(RespRequest));
    if (!dest) return NULL;

    dest->command = src->command;
    dest->argc = src->argc;
    for (int i = 0; i < src->argc; i++) {
        dest->args[i] = strdup(src->args[i]); 
    }
    return dest;
}

int handle_set_flags(RespRequest *req, int *expireAt, TIME_FLAGS *flag){
    printf("\n");
    printf("ENTERED FLAGS HANDLER\n");  

    *expireAt = -1;
    *flag = NO_TIME_FLAG;
    
    if(req->argc <= 3){
        printf("Doesnt have flags");
        return 0;
    }
    for(int i = 2; i < req->argc - 1; i++){
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
    printf("Client fd in ping: %d\n", client_fd);
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
        free(messages);
        if (response) {
            send(client->fd, response, strlen(response), 0);
            free(response);
        }
        client->is_blocked = 0;
        free(client->waiting_for_key);
        client->waiting_for_key = NULL;
        free(client->min_id);
        client->min_id = NULL;
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

    if(strstr(streamEntry->id, "-*") != NULL){
        sscanf(streamEntry->id, "%lld-*", &finalMs);
        if(finalMs == stream->last_ms){
            finalSeq = stream->last_seq + 1;
        } else {
            finalSeq = (finalMs == 0) ? 1 : 0;
        }
    }
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

void handle_set_stream(RespRequest *req, int client_fd, int isQueued){
    char *errorResp = NULL;
    
    Stream *stream = NULL;
    Entry *entry = store_getEntry(req->args[0]);

    if(entry == NULL){
        stream = calloc(1, sizeof(Stream));
        if(stream == NULL){
            errorResp = "-ERR Out of memory\r\n";
            if (client_fd != server_config.master_fd)
                send(client_fd, errorResp, strlen(errorResp), 0);
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
            if (client_fd != server_config.master_fd)
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

    if (client_fd != server_config.master_fd) {
        char response[128];
        int len = snprintf(response, sizeof(response), "$%zu\r\n%s\r\n", strlen(streamEntry->id), streamEntry->id);
        send(client_fd, response, len, 0);
    }

    printf("Boyya\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
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


void handle_set(RespRequest *req, RedisType type, int client_fd, int isQueued){
    if(req->argc < 2){
        printf("length smaller than 2");
        return;
    }
    if(type == TYPE_STREAM){
        handle_set_stream(req, client_fd, isQueued);
    }
    else{
        TIME_FLAGS time = NO_TIME_FLAG;
        int seconds = 0;
        if(handle_set_flags(req, &seconds, &time) == 0){
            store_set(req->args[0], req->args[1], time, seconds, type);
            if (client_fd != server_config.master_fd)
                send(client_fd, "+OK\r\n", 5, 0);
        }
        else{
            if (client_fd != server_config.master_fd)
                send(client_fd, "+FAIL\r\n", 7, 0);
        }
    }
}

void handle_get(RespRequest *req, int client_fd) {
    char response[1024];
    printf("req->args[0] in handle get: %s\n", req->args[0]);
    char *value = store_get(req->args[0]); 
    if (value == NULL) {
        send(client_fd, "$-1\r\n", 5, 0);  
        return;
    }
    snprintf(response, sizeof(response), "$%d\r\n%s\r\n", (int)strlen(value), value);
    send(client_fd, response, strlen(response), 0);
}

void handle_type(RespRequest *req, int client_fd, int isQueued){
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
    //TODO: create logic for unknown cmd
}

void printRequest(RespRequest *req){
    printf("PRINTING REQUESTTT\n");
    printf("command: %d\n", req->command);
    printf("argc: %d\n", req->argc);
    for(int i = 0; i < req->argc; i++){
        printf("Args: %s\n", req->args[i]);
    }
}

char* to_upper(char *stringToUpper){
    char *newString = malloc(strlen(stringToUpper) + 1);
    for(int i = 0; i < strlen(stringToUpper); i++){
        newString[i] = toupper(stringToUpper[i]);
    }
    return newString;
}

int is_multiple_key(RespRequest *req, int *streamsLength, int *streamIndex){
    int index = 0;
    while(strcmp(to_upper(req->args[index]), "STREAMS") != 0){
        index++;
    }
    printf("num: %d\n", req->argc - index - 1);
    if((req->argc - index - 1) % 2 == 0 && (req->argc - index - 1) >= 4){
        *streamIndex = index;
        *streamsLength = (req->argc - index - 1)/2;
        return 0;
    }
    return 1;
}

int hasBlock(RespRequest *request, int *blockIndex){
    char *wordInUpper = NULL;
    for(int i = 0; i < request->argc; i++){
        wordInUpper = to_upper(request->args[i]);
        printf("normal word: %s\n", request->args[i]);
        printf("upper word: %s\n", wordInUpper);
        if(strstr(wordInUpper, "BLOCK") && i < request->argc - 1){
            *blockIndex = i;
            free(wordInUpper);
            printf("returned 0\n");
            return 0;
        }
        free(wordInUpper);
    }
    printf("false aka 1\n");
    return 1;
}

int hasDollarSign(RespRequest *request, int *dollarIndex){
    for(int i = 0; i < request->argc; i++){
        printf("normal word: %s\n", request->args[i]);
        if(strstr(request->args[i], "$")){
            *dollarIndex = i;
            printf("returned 0 in dollarrr\n");
            return 0;
        }
    }
    printf("false aka 1 in dolllarr\n");
    return 1;
}

char* handle_block_dollar(RespRequest *req, char **temp_messages, int *count){
    printf("Called dollar\n");
    int dollarSignIndex = 0;
    printf("hasDollarSign(req, &dollarSignIndex) == 0: %d\n", hasDollarSign(req, &dollarSignIndex) == 0);
    if(hasDollarSign(req, &dollarSignIndex) == 0){
        StreamEntry *streamEntry = lastStreamEntry(req->args[3]);
        printf("is lastStreamEntry null: %d\n", streamEntry == NULL);
        if(streamEntry == NULL){
            *temp_messages = streamEntry_XREAD_toString(req->args[4], "+", req->args[3], count);
            printf("temp messy bassy yessy: %s\n", *temp_messages);
            return strdup(req->args[4]);
        }
        else{
            *temp_messages = streamEntry_XREAD_toString(streamEntry->id, "+", req->args[3], count);
            printf("temp mess: THE ENTRY IS REAL %s\n", *temp_messages);
            return strdup(streamEntry->id);
        }
    }
    return strdup(req->args[4]);
}

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

int handle_single_xread(RespRequest *req, Client *client, int *count, char **response){
    Entry *entry = NULL;
    char *temp_messages = NULL;
    int blockIndex = 0;
    
    printf("INSIDE single xread\n");
    if(hasBlock(req, &blockIndex) == 0){
        printf("index 5: %s\n", req->args[3]);
        printf("index 6: %s\n", req->args[4]);
        printf("get_current_time_ms() + ms_timeout: %lld\n",get_current_time_ms() + atoi(req->args[blockIndex + 1]));

        int ms = atoi(req->args[blockIndex + 1]);
        client->timeout_at = (ms == 0) ? 0 : get_current_time_ms() + ms;        
        client->min_id = handle_block_dollar(req, &temp_messages, count);

        printf("Temp MESSAGES: %s\n", temp_messages);
        if (temp_messages != NULL && strcmp(temp_messages, "") != 0 && *count > 0) {
            *response = wrapXreadResponse(req->args[3], temp_messages, *count);
            printf("ZERO TO HERO");
            free(temp_messages);
            return 0;
        }
        else{
            printf("min_id in single and mingle: %s\n", client->min_id);
            client->is_blocked = 1;
            client->waiting_for_key = strdup(req->args[3]);
            return 1;
        }

        if(temp_messages) free(temp_messages);

        printf("entry is not null\n");
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

        if (*count == 0 || temp_messages == NULL) {
            send(client->fd, "*0\r\n", 4, 0);
            if(temp_messages) free(temp_messages);
            return 1;
        }
    }
    
    *response = wrapXreadResponse(req->args[1], temp_messages, *count);
    free(temp_messages);   
    return 0;
}

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

int handle_xread(RespRequest *req, Client *client, char **response, int *count){
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
    send(client->fd, response, strlen(response), 0);
    free(response);
}

int handle(RespRequest *req, Client *client) {
    printf("HANDLE CALLED: cmd=%d argc=%d\n", req->command, req->argc);
    fflush(stdout);
    if (req == NULL) {
        printf("ERROR IN HANDLE: Request is NULL\n");
        send(client->fd, "-ERR Internal error\r\n", 21, 0);
        return 1;
    }
    printf("Entered Handle on this request: \n");
    printRequest(req);

    printf("\n--- Entered Handler: Command ID %d ---\n", req->command);

    // Transaction cmds
    if(req->command == MULTI){
        handle_multi(req, client);
        return 0;
    }
    if(req->command == EXEC){
        handle_exec(client);
        printf("Done executing\n");
        return 0;
    }
    if(req->command == DISCARD){
        handle_discard(req, client);
        printf("Done discarding\n");
        return 0;
    }

    if(client->is_queued){
        client->requests[client->queuedCommands] = copy_request(req);
        client->queuedCommands++;
        printf("Command is QUEUED for fd %d\n", client->fd);
        send(client->fd, "+QUEUED\r\n", 9, 0);
        return 0;
    }

    if(client->is_subscribed && (req->command != SUBSCRIBE || req->command != PING)){
        //ERR Can't execute 'echo': only (P|S)SUBSCRIBE / (P|S)UNSUBSCRIBE / PING / QUIT / RESET are allowed in this context
        char errorBuffer[128];
        snprintf(errorBuffer, sizeof(errorBuffer), 
        "-ERR Can't execute '%s' only (P|S)SUBSCRIBE / (P|S)UNSUBSCRIBE / PING / QUIT / RESET are allowed in this context\r\n",
         cmd_to_string(req->command));
        printf("error: %s\n", errorBuffer);
        send(client->fd, errorBuffer, strlen(errorBuffer), 0);
        return 0;
    }

    printf("Is queued after multi: %d", client->is_queued);

    // Utility cmds
    if (req->command == ECHO) {
        handle_echo(req, client->fd);
        return 0;
    }
    if (req->command == PING) {
        if (!from_master(client) && !client->is_subscribed) {
            handle_ping(client->fd);
        }
        else if(client->is_subscribed){
            char *msg = "*2\r\n$4\r\nPONG\r\n$0\r\n";
            send(client->fd, msg, 19, 0);
        }
        return 0; 
    }
    if (req->command == INFO) {
        handle_info(req, client->fd);
        return 0; 
    }
    if (req->command == REPLCONF) {
        printf("req->args[0]: %s\n", req->args[0]);
        if(req->argc < 1){
            send(client->fd, "+OK\r\n", 5, 0);
            return 0; 
        }
        toUpper(req->args[0]);
        if (strcmp(req->args[0], "GETACK") == 0 && from_master(client)) {
            replconf_handle_getack(client->fd);
        } else if (strcmp(req->args[0], "ACK") == 0) {
            replconf_handle_ack(req, client);
        } else {
            send(client->fd, "+OK\r\n", 5, 0);
        }
        return 0;
    }
    if (req->command == PSYNC) {
        char replId[1024];
        snprintf(replId, sizeof(replId), "+FULLRESYNC %s %lld\r\n", 
            server_config.master_replid, 
            server_config.master_repl_offset);
        send(client->fd, replId, strlen(replId), 0);

        unsigned char empty_rdb[] = {
            0x52, 0x45, 0x44, 0x49, 0x53, 0x30, 0x30, 0x31, 0x31,  
            0xff,                                                      
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00      
        };

        char header[32];
        int header_len = snprintf(header, sizeof(header), "$%zu\r\n", sizeof(empty_rdb));
        send(client->fd, header, header_len, 0);
        send(client->fd, empty_rdb, sizeof(empty_rdb), 0);

        client->is_replica = 1;
        return 0; 
    }
    if(req->command == WAIT){
        handle_wait(req, client->fd);
        return 0;
    }
    if(req->command == KEYS){
        handle_key(req, client->fd);
        return 0;
    }
    if (req->command == AUTH || req->command == SELECT || req->command == COMMAND) {
        send(client->fd, "+OK\r\n", 5, 0);
        return 0; 
    }

    //rdb cmds
    if(req->command == CONFIG_GET){
        printf("Config get handler!!\n");
        handle_config(req, client->fd);
        return 0;
    }

    // Core cmds
    if (req->command == SET) {
        printf("Executing SET\n");
        handle_set(req, TYPE_STRING, client->fd, client->is_queued);
        return 0;
    }
    if (req->command == GET) {
        printf("Executing GET\n");
        handle_get(req, client->fd);
        return 0; 
    }
    if (req->command == DEL) {
        return 0; 
    }
    if (req->command == EXISTS) {
        return 0; 
    }
    if (req->command == EXPIRE) {
        return 0; 
    }
    if (req->command == TTL) {
        return 0;
    }
    if (req->command == TYPE) {
        printf("Executing TYPE\n");
        handle_type(req, client->fd, client->is_queued);
        return 0;
    }

    // String & Number specific cmds
    if (req->command == INCR){ 
        handle_incr(req, client->fd, client->is_queued);
        return 0;
    }
    if (req->command == DECR)   { return 0; }
    if (req->command == APPEND) { return 0; }
    if (req->command == STRLEN) { return 0; }
    if (req->command == MGET)   { return 0; }

    // List cmds
    if (req->command == LPUSH) { 
        handle_set(req, TYPE_LIST, client->fd, client->is_queued);
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
        handle_set(req, TYPE_STREAM, client->fd, client->is_queued);
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
    if (req->command == XGROUP) { return 0; }

    //Sub/Pub cmds SUBSCRIBE
    if (req->command == SUBSCRIBE){
        printf("ENTERED Subscribe\n");
        handle_subscribe(req, client);
        return 0;
    }
        
    // Default: UNKNOWN
    printf("Data: %s", req->args[0]);
    printf("Unknown command received\n");
    send(client->fd, "-ERR unknown command\r\n", 22, 0);
    return 1;
}