#ifndef UTILS_H
#define UTILS_H
#include <time.h>
#include <string.h>


#define TABLE_SIZE 1024

typedef enum {
    EX, PX, EXAT, PXAT, NO_TIME_FLAG,
} TIME_FLAGS;

typedef enum {
    TYPE_STRING,
    TYPE_LIST,
    TYPE_HASH,
    TYPE_ZSET,
    TYPE_SET,
    //NOTHING,
} RedisType;

typedef struct Entry {
    char *key;
    char *value;
    struct Entry *next;
    long long expires_at;
    RedisType type;
} Entry;



extern Entry *table[TABLE_SIZE];

int hash(char *key);
void store_set(char *key, char *value, TIME_FLAGS flag, int seconds, RedisType type);
char* store_get(char *key);
Entry *store_getEntry(char *key);


#endif