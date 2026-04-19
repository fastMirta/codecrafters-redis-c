#ifndef UTILS_H
#define UTILS_H

#include <time.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

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

typedef struct ZSetEntry {
    char *member;          
    double score;          
    struct ZSetEntry *next; 
} ZSetEntry;

typedef struct {
    ZSetEntry *head;        
    size_t length;          
} ZSet;

extern Entry *table[TABLE_SIZE];

void toUpper(char *str);
int hash(char *key);
void store_set_stream(char *key, Stream *stream);
void store_set_zset(char *key, ZSet *zSet);
void store_set(char *key, void *value, TIME_FLAGS flag, int seconds, RedisType type);
char* store_get(char *key);
Entry *store_getEntry(char *key);
long long get_current_time_ms();
int add_to_string(long long startMs, long long startSeq, long long endMs,
     long long endSeq, long long currentMs, long long currentSeq, int isXrange);
char* streamEntry_toString(char *idStart, char* idEnd, char *key,  int *count);
char* streamEntry_XREAD_toString(char *idStart, char* idEnd, char *key, int *count);
int isBigger(char *srcId, char *checkId);
char* streamEntry_XREAD_Mul_toString(int len, char *keyArray[], char *idArrays[]);
void printValue(StreamEntry *entry);
StreamEntry* lastStreamEntry(char *key);
int isWriteCmd(REDIS_CMDS cmd);
char* cmd_to_string(REDIS_CMDS cmdName);

#endif