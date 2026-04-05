#include <stdio.h>
#include <string.h>
#include "parser.h"
#include <ctype.h>
#include <stdlib.h>
//0 = success| 1 = failure

/**Changes all the characters of a given string to upper case */
void toUpper(char *str){
	for (int i = 0; str[i]; i++)
		str[i] = toupper((unsigned char)str[i]);
}

//TODO: maybe change type to int and return 0 for work 1/-1 for didnt
int parse(char *client_input, RespRequest *request){
    printf("ENTERED Parser");
    if(client_input == NULL){
        printf("Error");
        return 1;
    }
     /**Redis request: 
        *3\r\n
        $3\r\n
        SET\r\n
        $3\r\n
        key\r\n
        $5\r\n
        value\r\n
        
        * = arrays
        $ = long word
        + = short word
        - = error
        : = number
        The cmd name is always in the third line    
     */
    char *firstLine = getLine(client_input);
    char *prefix = calloc(1, sizeof(char));
    char *firstLinePtr = firstLine;
    //*(firstLine + 1);

    //Break parser if the first line doesnt start with a prefix or doesnt start with arrays prefix
    if(getPrefix(firstLine, prefix) != 0 || *prefix != '*'){return 1;}

    int loopLen = atoi(firstLinePtr + 1);
    request->argc = loopLen - 1;

    //jumping to the next line
    client_input = client_input + strlen(firstLine) + 2;
    for(int i = 0; i < loopLen; i++){
        printf("\n");
        printf("Index: %d", i);
        firstLine = getLine(client_input);
        if(getPrefix(firstLine, prefix) != 0){return 1;}

        client_input = client_input + strlen(firstLine) + 2;
        char *secondLine = getLine(client_input);
        printf("Second line: %s", secondLine);
        printf("\n");
        int nextLineLen = atoi(firstLine + 1);
        if(strlen(secondLine) != nextLineLen){return 1;}
        
        //Finds the cmd
        if(i == 0){
            
            char *secondLinewith2 = malloc(strlen(secondLine));
            strncpy(secondLinewith2, secondLine, strlen(secondLine));
            printf("second linessss: %s", secondLinewith2);
            request->command = findRedisCmd(secondLine);
        }
        else{
            request->args[i-1] = secondLine;
        }
        client_input = client_input + strlen(secondLine) + 2;
    }
    printf("\n");
    printf("argc: %d\n", request->argc);
    //printf("last args: %s\n", request->args[strlen(request->args)]);
    
    if(*client_input != '\0'){
        printf("Another error\n");
        return 1;
    }
    
    /**
     * --SET--
     * *5\r\n
        $3\r\n
        SET\r\n
        $3\r\n
        key\r\n
        $5\r\n
        value\r\n
        $2\r\n
        EX\r\n
        $2\r\n
        10\r\n
     * 
     * *2\r\n
     * $4\r\n
     * ECHO\r\n
     * $11\r\n
     * Hello World\r\n
     * 
     * 
     */
    printf("No error\n");
    printf("%d", request->command);

    free(firstLinePtr);
    return 0;
    
}
/**
 * Create (ECHO)Handler and add this to it:
 * if(i == 0){
            in = strlen(firstLine);
      - 2;
            currentMethod = findRedisCmd(firstLine);
            if(currentMethod == ECHO && loopLen != 2){return;}
        }
 */

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