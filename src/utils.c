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
//redis-cli XADD stream_key 1526919030474-0 temperature 36 humidity 95
//"1526919030474-0"
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
    table[index]->value = value; //redis-cli SET "myInt" 12 redis-cli GET "myInt"
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
     int index = hash(key);
    if (table[index] == NULL)
        return NULL;
    return table[index];
}




