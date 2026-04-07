#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include <sys/time.h>

Entry *table[TABLE_SIZE] = {NULL};

/**Changes all the characters of a given string to upper case */
void toUpper(char *str){
	for (int i = 0; str[i]; i++)
		str[i] = toupper((unsigned char)str[i]);
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
    if (table[index] == NULL){
        printf("Value doesnt exist in getEntry\n");
        return NULL;
    }
        
    return table[index];
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
    char* entriesToPrint = malloc(1024);;
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
        
        if(add_to_string(firstMS, firstSeq, endMs, endSeq, currentMs, currentSeq, 0) == 0){
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
    char *result = malloc(1024 + 32);
    int finalLen = snprintf(result, 1024 + 32, "*%d\r\n%s", *count, entriesToPrint);
    free(entriesToPrint);
    free(newPtr);
    
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



