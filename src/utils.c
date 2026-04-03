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



void store_set(char *key, char *value, TIME_FLAGS flag, int seconds, RedisType type) {
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




