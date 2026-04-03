#ifndef UTILS_H
#define UTILS_H
#include <time.h>
#include <string.h>
#define TABLE_SIZE 1024

typedef enum {
    EX, PX, EXAT, PXAT, NO_TIME_FLAG,
} TIME_FLAGS;

typedef struct Entry {
    char *key;
    char *value;
    struct Entry *next;
    time_t expires_at;
} Entry;

extern Entry *table[TABLE_SIZE];

int hash(char *key);
void store_set_EX(char *key, int exTime);
void store_set_PX(char *key, int pxTime);
void store_set(char *key, char *value, TIME_FLAGS flag, int seconds);
char* store_get(char *key);


#endif