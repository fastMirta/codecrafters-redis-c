#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "parser.h"
#include "utils.h"



// parser.c
int parse(char **client_input, RespRequest *request){
    if(*client_input == NULL) return 1;

    char *prefix = calloc(1, sizeof(char));
    char *firstLine = getLine(*client_input);

    if(getPrefix(firstLine, prefix) != 0 || *prefix != '*'){
        free(firstLine);
        free(prefix);
        return 1;
    }

    int loopLen = atoi(firstLine + 1);
    request->argc = loopLen - 1;
    *client_input = *client_input + strlen(firstLine) + 2;
    free(firstLine);

    for(int i = 0; i < loopLen; i++){
        char *lenLine = getLine(*client_input);
        if(getPrefix(lenLine, prefix) != 0){
            free(lenLine);
            free(prefix);
            return 1;
        }
        *client_input = *client_input + strlen(lenLine) + 2;

        char *valueLine = getLine(*client_input);
        int expectedLen = atoi(lenLine + 1);
        free(lenLine);

        if((int)strlen(valueLine) != expectedLen){
            free(valueLine);
            free(prefix);
            return 1;
        }

        if(i == 0){
            request->command = findRedisCmd(valueLine);
            free(valueLine);
        } else {
            request->args[i-1] = strdup(valueLine);
            free(valueLine);
        }
        *client_input = *client_input + expectedLen + 2;
    }

    free(prefix);
    return 0;
}

//Copies the line until reaches the end of the word or the end of the line (symbolized by \r\n)
char* getLine(char *client_input){
    char *line = malloc(256);  
    int i = 0;
    while (client_input[i] != '\r' && client_input[i] != '\0') {
        line[i] = client_input[i];   
        i++;
    }
    line[i] = '\0';  
    return line;
}

//Sets the prefix of the line to the value of variable ptr prefix if worked returns 0 other wise 1
int getPrefix(char line[], char *prefix){
    switch (line[0]){
        case '*':
            *prefix = '*';
            return 0;
        case '$':
            *prefix = '$';
            return 0;
        case '+':
            *prefix = '+';
            return 0;
        case '-':
            *prefix = '-';
            return 0;
        case ':':
            *prefix = ':';
            return 0;
        default:
            return 1;
    }
}

/**
 * @param cmdName name of the cmd (third line of the data given by the client)
 */
REDIS_CMDS findRedisCmd(char *cmdName) {
    if (cmdName == NULL) return UNKNOWN;
    
    toUpper(cmdName);

    // Utility cmds
    if (strcmp(cmdName, "ECHO") == 0) return ECHO;
    if (strcmp(cmdName, "PING") == 0) return PING;
    if (strcmp(cmdName, "INFO") == 0) return INFO;
    if (strcmp(cmdName, "AUTH") == 0) return AUTH;
    if (strcmp(cmdName, "SELECT") == 0) return SELECT;
    if (strcmp(cmdName, "COMMAND") == 0) return COMMAND;

    // Core cmds (Generic)
    if (strcmp(cmdName, "SET") == 0) return SET;
    if (strcmp(cmdName, "GET") == 0) return GET;
    if (strcmp(cmdName, "DEL") == 0) return DEL;
    if (strcmp(cmdName, "EXISTS") == 0) return EXISTS;
    if (strcmp(cmdName, "EXPIRE") == 0) return EXPIRE;
    if (strcmp(cmdName, "TTL") == 0) return TTL;
    if (strcmp(cmdName, "TYPE") == 0) return TYPE;

    // String & Number specific cmds
    if (strcmp(cmdName, "INCR") == 0) return INCR;
    if (strcmp(cmdName, "DECR") == 0) return DECR;
    if (strcmp(cmdName, "APPEND") == 0) return APPEND;
    if (strcmp(cmdName, "STRLEN") == 0) return STRLEN;
    if (strcmp(cmdName, "MGET") == 0) return MGET;

    // Transaction cmds
    if(strcmp(cmdName, "MULTI") == 0)   return MULTI;
    if(strcmp(cmdName, "EXEC") == 0)    return EXEC;
    if(strcmp(cmdName, "DISCARD") == 0) return DISCARD; 

    // List cmds (Added real list commands)
    if (strcmp(cmdName, "LPUSH") == 0) return LPUSH;
    if (strcmp(cmdName, "RPUSH") == 0) return RPUSH;
    if (strcmp(cmdName, "LPOP") == 0) return LPOP;
    if (strcmp(cmdName, "RPOP") == 0) return RPOP;
    if (strcmp(cmdName, "LLEN") == 0) return LLEN;
    if (strcmp(cmdName, "LRANGE") == 0) return LRANGE;

    // Hash cmds (Fixed: H-commands are Hashes, not Lists)
    if (strcmp(cmdName, "HSET") == 0) return HSET;
    if (strcmp(cmdName, "HGET") == 0) return HGET;
    if (strcmp(cmdName, "HGETALL") == 0) return HGETALL;
    if (strcmp(cmdName, "HDEL") == 0) return HDEL;

    // Sets cmds
    if (strcmp(cmdName, "SADD") == 0) return SADD;
    if (strcmp(cmdName, "SREM") == 0) return SREM;
    if (strcmp(cmdName, "SMEMBERS") == 0) return SMEMBERS;
    if (strcmp(cmdName, "SISMEMBER") == 0) return SISMEMBER;

    // Stream cmds
    if (strcmp(cmdName, "XADD") == 0) return XADD;
    if (strcmp(cmdName, "XREAD") == 0) return XREAD;
    if (strcmp(cmdName, "XRANGE") == 0) return XRANGE;
    if (strcmp(cmdName, "XGROUP") == 0) return XGROUP;

    return UNKNOWN;
}