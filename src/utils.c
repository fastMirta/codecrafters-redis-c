#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include <sys/time.h>

Entry *table[TABLE_SIZE] = {NULL};

/**Changes all the characters of a given string to upper case */
void toUpper(char *str){
    printf("B4 str is to upper: %s\n", str);
	for (int i = 0; str[i]; i++)
		str[i] = toupper((unsigned char)str[i]);
    printf("after str is to upper: %s\n", str);
}


int hash(char *key) {
    int hash = 0;
    printf("key: %s\n", key);
    while (*key)
        hash = (hash * 31 + *key++) % TABLE_SIZE;
    return hash;
}

int validate_set(char *key, void *value, RedisType type){
    int index = hash(key);
    //  switch (type) {
    //     case TYPE_STRING: return validate_set_string();
    //     case TYPE_LIST:   return validate_set_();  
    //     case TYPE_HASH:   return validate_set_();  
    //     case TYPE_ZSET:   return validate_set_();  
    //     case TYPE_SET:    return validate_set_();
    //     case TYPE_STREAM: return validate_set_stream(index);  
    //     default:          return validate_set_();  
    // }
}

int validate_set_stream(int index){
    return(table[index]->type != TYPE_STREAM);
}

void store_set_stream(char *key, Stream *stream){
    int index = hash(key);
    if (table[index] == NULL)
        table[index] = malloc(sizeof(Stream));
    else{
        free(table[index]->key);
        free(table[index]->value);
    }
    table[index]->key = strdup(key);
    table[index]->value = stream;
    table[index]->expires_at = 0;
    table[index]->type = TYPE_STREAM;
    printf("\n");
    printf("SUCCESS IN SET STREAM \n");

}



void store_set(char *key, void *value, TIME_FLAGS flag, int seconds, RedisType type) {
    int index = hash(key);
    
    Entry *existing = table[index];
    while (existing != NULL) {
        if (strcmp(existing->key, key) == 0) break;
        existing = existing->next;
    }
    
    if (existing == NULL) {
        Entry *newEntry = malloc(sizeof(Entry));
        newEntry->key = strdup(key);
        newEntry->next = table[index]; 
        table[index] = newEntry;
        existing = newEntry;
    } else {
        free(existing->key);
        existing->key = strdup(key);
        if (existing->type == TYPE_STRING && existing->value)
            free(existing->value);
    }

    if (type == TYPE_STRING)
        existing->value = strdup((char*)value);
    else
        existing->value = value;

    existing->expires_at = 0;
    existing->type = type;

    if (seconds != -1) {
        long long now = get_current_time_ms();
        if (flag == EX)
            existing->expires_at = now + ((long long)seconds * 1000);
        else if (flag == PX)
            existing->expires_at = now + (long long)seconds;
    }

    printf("Saved value: %s with key: %s\n", table[index]->key, table[index]->value);
}


char *store_get(char *key) {
    printf("Key in store: %s\n", key);
    int index = hash(key);
    printf("LOOKING at index: %d\n", index);
    Entry *entry = table[index];
    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            if (entry->expires_at != 0) {
                long long now = get_current_time_ms();
                if (now >= entry->expires_at) {
                    // TODO: proper removal from chain
                    return NULL;
                }
            }
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}


Entry *store_getEntry(char *key){
    printf("key in getEntry: %s\n", key);
    int index = hash(key);
    printf("Index: %d\n", index);
    if (table[index] == NULL){
        printf("Value doesnt exist in getEntry\n");
        return NULL;
    }
        
    return table[index];
}

StreamEntry* lastStreamEntry(char *key){
    Entry *entry = store_getEntry(key);
    if(entry == NULL){
        return NULL;
    }
    
    if(entry->type != TYPE_STREAM){
        return NULL;
    }

    Stream *stream = (Stream*)entry->value;
    if(stream == NULL){
        return NULL;
    }
    
    return stream->tail;
}

/**Checks if need 
 * isXrange = 0 in xrange, isXrange = 1 in xread
 */

int add_to_string(long long startMs, long long startSeq, long long endMs,
     long long endSeq, long long currentMs, long long currentSeq, int isXrange){

    if(isXrange == 1){
        return !((currentMs > startMs ||(currentMs == startMs && currentSeq > startSeq))
     && ((currentMs < endMs) || (currentMs == endMs && currentSeq <= endSeq)));   
    }
        
    return !((currentMs >= startMs && currentSeq >= startSeq) && (currentMs <= endMs && currentSeq <= endSeq));       
}

void printValue(StreamEntry *entry){
    printf("id: %s\n", entry->id);
    printf("field count in print value: %d\n", entry->field_count);
    for(int i = 0; i < entry->field_count; i++){
        printf("Field num: %d field name: %s\n", i, entry->fields[i]);
    }
}

//checks if checkId is bigger than srcId
int isBigger(char *srcId, char *checkId){
    long long srcMs, srcSeq, checkMS, checkSeq;
    sscanf(srcId, "%lld-%lld", &srcMs, &srcSeq);
    sscanf(checkId, "%lld-%lld", &checkMS, &checkSeq);
    printf("Check ms: %lld\n", checkMS);
    printf("Check seq: %lld\n", checkSeq);
    printf("Src ms: %lld\n", srcMs);
    printf("Src seq: %lld\n", srcSeq);
    return(checkMS > srcMs || (checkMS == srcMs && checkSeq > srcSeq));
}



char* streamEntry_toString(char *idStart, char* idEnd, char *key, int *count){
    Entry *entry = store_getEntry(key);
    if(entry == NULL){
        printf("Entry wasnt FOUND\n");
        return NULL;
    }
    if(entry->type != TYPE_STREAM){
        printf("Type is NOT stream: %d\n", entry->type);
        return NULL;
    }

    Stream *stream = (Stream*)entry->value;
    StreamEntry *newPtr = stream->head;

    long long firstMS, firstSeq, endMs, endSeq, currentMs, currentSeq;

    if(strcmp(idStart, "-") == 0 && strcmp(idEnd, "+") == 0){
        sscanf(newPtr->id, "%lld-%lld", &firstMS, &firstSeq);
        sscanf(stream->tail->id, "%lld-%lld", &endMs, &endSeq);
    }
    else if(strcmp(idStart, "-") == 0){
        sscanf(newPtr->id, "%lld-%lld", &firstMS, &firstSeq);
        sscanf(idEnd, "%lld-%lld", &endMs, &endSeq);
    }
    else if(strcmp(idEnd, "+") == 0){
        sscanf(idStart, "%lld-%lld", &firstMS, &firstSeq);
        sscanf(stream->tail->id, "%lld-%lld", &endMs, &endSeq);
    }
    else{
        sscanf(idStart, "%lld-%lld", &firstMS, &firstSeq);
        sscanf(idEnd, "%lld-%lld", &endMs, &endSeq);
    }

    int bufSize = 64 + (int)stream->length * 256;
    char *entriesToPrint = malloc(bufSize);
    if(entriesToPrint == NULL) return NULL;
    int offset = 0;

    printf("stream length: %zu\n", stream->length);
    for(int i = 0; i < (int)stream->length; i++){
        printValue(newPtr);
        sscanf(newPtr->id, "%lld-%lld", &currentMs, &currentSeq);

        if(add_to_string(firstMS, firstSeq, endMs, endSeq, currentMs, currentSeq, 0) == 0){
            offset += snprintf(entriesToPrint + offset, bufSize - offset,
                "*2\r\n$%zu\r\n%s\r\n*%d\r\n",
                strlen(newPtr->id), newPtr->id, newPtr->field_count);
            (*count)++;
            for(int j = 0; j < newPtr->field_count; j++){
                offset += snprintf(entriesToPrint + offset, bufSize - offset,
                    "$%zu\r\n%s\r\n",
                    strlen(newPtr->fields[j]), newPtr->fields[j]);
            }
        }
        newPtr = newPtr->next;
    }

    printf("finished toString\n");
    printf("entriesToPrint: %s\n", entriesToPrint);

    char *result = malloc(bufSize + 32);
    if(result == NULL){
        free(entriesToPrint);
        return NULL;
    }
    snprintf(result, bufSize + 32, "*%d\r\n%s", *count, entriesToPrint);
    free(entriesToPrint);

    return result;
}


char* streamEntry_XREAD_toString(char *idStart, char* idEnd, char *key, int *count){
    char* entriesToPrint = malloc(1024);
    int offset = 0;

    Entry *entry = store_getEntry(key);
    if(entry == NULL){
        printf("Entry wasnt FOUND\n"); 
        return NULL;
    }
    if(entry->type != TYPE_STREAM){
        printf("Type is NOT stream: %d\n", entry->type);
        printf("Key: %s\n", entry->key);
        return NULL;
    }
    
    
    Stream *stream = (Stream*)entry->value;
    StreamEntry *newPtr = stream->head;

    long long firstMS, firstSeq, endMs, endSeq, currentMs, currentSeq;
    if(strcmp(idStart, "-") == 0 && strcmp(idEnd, "+") != 0){
        sscanf(newPtr->id, "%lld-%lld", &firstMS, &firstSeq);
        sscanf(idEnd, "%lld-%lld", &endMs, &endSeq);
        printf("Entered - only\n");
        printf("start ms: %lld, start seq: %lld, end ms: %lld, end seq: %lld\n", firstMS, firstSeq, endMs, endSeq);
    }
    else if(strcmp(idStart, "-") != 0 && strcmp(idEnd, "+") == 0){
        sscanf(idStart, "%lld-%lld", &firstMS, &firstSeq);
        printf("Tail is null? %d\n", stream->tail == NULL);
        sscanf(stream->tail->id, "%lld-%lld", &endMs, &endSeq);
        printf("Entered + only\n");
        printf("start ms: %lld, start seq: %lld, end ms: %lld, end seq: %lld\n", firstMS, firstSeq, endMs, endSeq);
    }
    else if(strcmp(idStart, "-") != 0 && strcmp(idEnd, "+") != 0) {
        sscanf(idStart, "%lld-%lld", &firstMS, &firstSeq);
        sscanf(idEnd, "%lld-%lld", &endMs, &endSeq);
        printf("Entered none only\n");
        printf("start ms: %lld, start seq: %lld, end ms: %lld, end seq: %lld\n", firstMS, firstSeq, endMs, endSeq);
    }
    printValue(stream->head);

    
    printf("stream length: %zu\n", stream->length);
    for(int i = 0; i < stream->length; i++){
        printValue(newPtr);
        sscanf(newPtr->id, "%lld-%lld", &currentMs, &currentSeq);
        
        if(add_to_string(firstMS, firstSeq, endMs, endSeq, currentMs, currentSeq, 1) == 0){
            offset += snprintf(entriesToPrint + offset,
                 1024 - offset, "*2\r\n$%zu\r\n%s\r\n*%d\r\n", strlen(newPtr->id), newPtr->id, newPtr->field_count);
            (*count)++;
            for(int j = 0; j < newPtr->field_count; j++){
                offset += snprintf(entriesToPrint + offset, 1024 - offset,
                 "$%zu\r\n%s\r\n", strlen(newPtr->fields[j]), newPtr->fields[j]);
            }
        }
        newPtr = newPtr->next;
    }
    printf("finished toString\n");
    printf("entriesToPrint: %s\n", entriesToPrint);
    printf("test entries: %d\n", strcmp(entriesToPrint, "") == 0);
    
    
    return entriesToPrint;
}


char* streamEntry_XREAD_Mul_toString(int len, char *keyArray[], char *idArrays[]) {
    char **blobs  = malloc(len * sizeof(char*));
    int  *counts  = calloc(len, sizeof(int));

    printf("Entered bs\n");
    for (int i = 0; i < len; i++) {
        printf("id: %s\n", idArrays[i]);
        blobs[i] = streamEntry_XREAD_toString(idArrays[i], "+", keyArray[i], &counts[i]);
        if (blobs[i] == NULL) {
            for (int j = 0; j < i; j++) free(blobs[j]);
            free(blobs); free(counts);
            return NULL;
        }
    }
    
    int totalSize = 32; 
    for (int i = 0; i < len; i++)
        totalSize += 32 + strlen(keyArray[i]) + strlen(blobs[i]);

    char *result = malloc(totalSize);
    int offset = 0;

    offset += snprintf(result + offset, totalSize - offset, "*%d\r\n", len);

    for (int i = 0; i < len; i++) {
        offset += snprintf(result + offset, totalSize - offset,
            "*2\r\n$%zu\r\n%s\r\n*%d\r\n%s",
            strlen(keyArray[i]), keyArray[i],
            counts[i],
            blobs[i]);
        free(blobs[i]); 
    }

    free(blobs);
    free(counts);
    return result; 
}



/**Checks if given cmd is part of write cmd
 * return 0 for true or 1 for false
 */
int isWriteCmd(REDIS_CMDS cmd){
    if (cmd == SET)  {return 0;}
    if (cmd == DEL)  {return 0;}
    if (cmd == APPEND) {return 0;}
    if (cmd == EXPIRE) {return 0;}
    if (cmd == INCR) {return 0;}
    if (cmd == DECR) {return 0;}
    if (cmd == XADD) {return 0;}
    if (cmd == SADD) {return 0;}
    if (cmd == SREM) {return 0;}
    if (cmd == HSET) {return 0;}
    if (cmd == HDEL) {return 0;}
    if (cmd == LPUSH) {return 0;}
    if (cmd == RPUSH) {return 0;}
    if (cmd == LPOP) {return 0;}
    if (cmd == RPOP) {return 0;}

    return 1;
}


char* cmd_to_string(REDIS_CMDS cmdName){
    switch(cmdName) {
        case ECHO:     return "ECHO";
        case PING:     return "PING";
        case INFO:     return "INFO";
        case REPLCONF: return "REPLCONF";
        case WAIT:     return "WAIT";
        case PSYNC:    return "PSYNC";
        case AUTH:     return "AUTH";
        case SELECT:   return "SELECT";
        case COMMAND:  return "COMMAND";
        case SET:      return "SET";
        case GET:      return "GET";
        case DEL:      return "DEL";
        case EXISTS:   return "EXISTS";
        case EXPIRE:   return "EXPIRE";
        case TTL:      return "TTL";
        case TYPE:     return "TYPE";
        case INCR:     return "INCR";
        case DECR:     return "DECR";
        case APPEND:   return "APPEND";
        case STRLEN:   return "STRLEN";
        case MGET:     return "MGET";
        case MULTI:    return "MULTI";
        case EXEC:     return "EXEC";
        case DISCARD:  return "DISCARD";
        case WATCH:    return "WATCH";
        case LPUSH:    return "LPUSH";
        case RPUSH:    return "RPUSH";
        case LPOP:     return "LPOP";
        case RPOP:     return "RPOP";
        case LLEN:     return "LLEN";
        case LRANGE:   return "LRANGE";
        case HSET:     return "HSET";
        case HGET:     return "HGET";
        case HGETALL:  return "HGETALL";
        case HDEL:     return "HDEL";
        case SADD:     return "SADD";
        case SREM:     return "SREM";
        case SMEMBERS: return "SMEMBERS";
        case SISMEMBER:return "SISMEMBER";
        case XADD:     return "XADD";
        case XREAD:    return "XREAD";
        case XRANGE:   return "XRANGE";
        case XGROUP:   return "XGROUP";
        case SUBSCRIBE: return "SUBSCRIBE";
        default:       return "UNKNOWN";
    }
}



