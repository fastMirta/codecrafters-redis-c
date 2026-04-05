#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include <sys/time.h>

Entry *table[TABLE_SIZE] = {NULL};

long long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
}

int hash(char *key) {
    int hash = 0;
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
    table[index]->key = key;
    table[index]->value = stream; 
    table[index]->expires_at = 0;
    table[index]->type = TYPE_STREAM;
    printf("\n");
    printf("SUCCESS IN SET STREAM \n");

}

void store_set(char *key, void *value, TIME_FLAGS flag, int seconds, RedisType type) {
    int index = hash(key);
    if (table[index] == NULL)
        table[index] = malloc(sizeof(Entry));
    else{
        free(table[index]->key);
        free(table[index]->value);
    }
    
    table[index]->key = key;
    table[index]->value = value; 

    table[index]->expires_at = 0;
    table[index]->type = type;

    if (seconds != -1) {
        long long now = get_current_time_ms();
        if (flag == EX) {
            table[index]->expires_at = now + ((long long)seconds * 1000);
        } else if (flag == PX) {
            table[index]->expires_at = now + (long long)seconds;
        }
    }

    printf("\n");
    printf("seconds: %lld\n", table[index]->expires_at);
}

char *store_get(char *key) {
    int index = hash(key);
    if (table[index] == NULL)
        return NULL;

    if (table[index]->expires_at != 0) {
        long long now = get_current_time_ms();
        if (now >= table[index]->expires_at) {
            table[index] = NULL; 
            return NULL; 
        }
    }
    return table[index]->value;
}

Entry *store_getEntry(char *key){
    printf("key in getEntry: %s\n", key);
    int index = hash(key);
    printf("Index: %d\n", index);
    if (table[index] == NULL)
        return NULL;
    return table[index];
}

int add_to_string(long long startMs, long long startSeq, long long endMs,
     long long endSeq, long long currentMs, long long currentSeq){
        // printf("Start Ms: %llu\n", startMs);
        // printf("End Ms: %llu\n", endMs);
        // printf("Start Seq: %llu\n", startSeq);
        // printf("End Seq: %llu\n", endSeq);
        // printf("Current Ms: %llu\n", currentMs);
        // printf("Current Seq: %llu\n", currentSeq);
        // printf("%d\n", !(currentMs >= startMs && (currentMs <= endMs && currentSeq <= endSeq)));
    return !((currentMs >= startMs && currentSeq >= startSeq) && (currentMs <= endMs && currentSeq <= endSeq));    
}

void printValue(StreamEntry *entry){
    printf("id: %s\n", entry->id);
    printf("field count in print value: %d\n", entry->field_count);
    for(int i = 0; i < entry->field_count; i++){
        printf("Field num: %d field name: %s\n", i, entry->fields[i]);
    }
}

char* streamEntry_toString(char *idStart, char* idEnd, char *key, int *count){
    char* entriesToPrint = malloc(1024);;
    int offset = 0;

    Entry *entry = store_getEntry(key);
    if(entry == NULL){
        printf("Entry wasnt FOUND\n");
        return NULL;}
    if(entry->type != TYPE_STREAM){
        printf("Type is NOT stream: %d\n", entry->type);
        printf("Key: %s\n", entry->key);
        return NULL;}

    long long firstMS, firstSeq, endMs, endSeq, currentMs, currentSeq;
    Stream *stream = (Stream*)entry->value;
    StreamEntry *newPtr = stream->head;
    printValue(stream->head);
    sscanf(idStart, "%lld-%lld", &firstMS, &firstSeq);
    sscanf(idEnd, "%lld-%lld", &endMs, &endSeq);
    
    printf("stream length: %zu\n", stream->length);
    for(int i = 0; i < stream->length; i++){
        printValue(newPtr);
        sscanf(newPtr->id, "%lld-%lld", &currentMs, &currentSeq);
        if(add_to_string(firstMS, firstSeq, endMs, endSeq, currentMs, currentSeq) == 0){
            //printf("EntriesToPrint 1: %s\n", entriesToPrint);
            offset += snprintf(entriesToPrint + offset, 1024 - offset, "*2\r\n$%zu\r\n%s\r\n*%d\r\n", strlen(newPtr->id), newPtr->id, newPtr->field_count);
            //printf("Offset: %d\n", offset);
            (*count)++;
            for(int j = 0; j < newPtr->field_count; j++){
                //printf("Field count: %d\n", newPtr->field_count);
                offset += snprintf(entriesToPrint + offset, 1024 - offset,
                 "$%zu\r\n%s\r\n", strlen(newPtr->fields[j]), newPtr->fields[j]);
                // printf("EntriesToPrint 2: %s\n", entriesToPrint);
                // printf("Offset BELOW: %d\n", offset);
            }
        }
        newPtr = newPtr->next;
    }
    printf("finished toString\n");
    printf("entriesToPrint: %s\n", entriesToPrint);
    char *result = malloc(1024 + 32);
    int finalLen = snprintf(result, 1024 + 32, "*%d\r\n%s", *count, entriesToPrint);
    free(entriesToPrint);
    return result;
    //redis-cli XADD some_key 1526985054069-0 temperature 36 humidity 95
    //redis-cli XADD some_key 1526985054079-0 temperature 37 humidity 94
    //redis-cli XRANGE some_key 1526985054069 1526985054079
}




