#ifndef UTILS_H
#define UTILS_H

#define TABLE_SIZE 1024


typedef struct Entry {
    char *key;
    char *value;
    struct Entry *next;  
} Entry;

extern Entry *table[TABLE_SIZE];

int hash(char *key);
void store_set(char *key, char *value);
char* store_get(char *key);

#endif