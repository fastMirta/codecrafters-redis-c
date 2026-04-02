#include <stdlib.h>
#include <utils.h>


int hash(char *key) {
    int hash = 0;
    while (*key)
        hash = (hash * 31 + *key++) % TABLE_SIZE;
    return hash;
}

void store_set(char *key, char *value) {
    int index = hash(key);
    table[index]->key = key;
    table[index]->value = value;
}

char* store_get(char *key) {
    int index = hash(key);
    return table[index]->value;
}
