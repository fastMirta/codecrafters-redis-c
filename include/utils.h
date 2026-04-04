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
    TYPE_STREAM,
    //NOTHING,
} RedisType;

typedef struct Entry {
    char *key;
    void *value;
    struct Entry *next;
    long long expires_at;
    RedisType type;
} Entry;


typedef struct StreamEntry {
    char *id;            
    char **fields;       
    int field_count;     
    struct StreamEntry *next;
} StreamEntry;

typedef struct Stream {
    StreamEntry *head;
    StreamEntry *tail;  
    long long last_ms;
    long long last_seq;
    size_t length;
} Stream;

extern Entry *table[TABLE_SIZE];

int hash(char *key);
void store_set_stream(char *key, Stream *stream);
void store_set(char *key, void *value, TIME_FLAGS flag, int seconds, RedisType type);
char* store_get(char *key);
Entry *store_getEntry(char *key);


#endif