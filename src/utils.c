#include <stdlib.h>
#include "utils.h"

Entry *table[TABLE_SIZE] = {NULL};

int hash(char *key) {
    int hash = 0;
    while (*key)
        hash = (hash * 31 + *key++) % TABLE_SIZE;
    return hash;
}

void store_set_EX(char *key, int exTime){
    int index = hash(key);
    table[index]->key = key;
    table[index]->expires_at = 0; 
    
    table[index]->expires_at = time(NULL) + exTime;
}

void store_set_PX(char *key, int pxTime){
    int index = hash(key);
    table[index]->key = key;
    table[index]->expires_at = 0; 
    
    table[index]->expires_at = time(NULL) + (pxTime/1000);
}

void store_set(char *key, char *value, TIME_FLAGS flag, int seconds) {
    int index = hash(key);
    if (table[index] == NULL)
        table[index] = malloc(sizeof(Entry));
    table[index]->key = key;
    table[index]->value = value;

    if(seconds == -1)
        table[index]->expires_at = 0;
    else if(flag == EX)
        store_set_EX(key, seconds);
    else if(flag == PX)
        store_set_PX(key, seconds);
    printf("\n");
    printf("seconds: %d\n", table[index]->expires_at);
}

char *store_get(char *key) {
    int index = hash(key);
    if (table[index] == NULL)
        return NULL;
    if (table[index]->expires_at != 0 && time(NULL) > table[index]->expires_at) {
        free(table[index]);
        table[index] = NULL;
        return NULL;
    }
    return table[index]->value;
}





