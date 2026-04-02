#include <stdio.h>
#include <string.h>
#include <parser.h>
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
    if(client_input == NULL){return 1;}
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
    //char *secondLine = malloc(1024, sizeof(char));
    for(int i = 0; i < loopLen; i++){
        firstLine = getLine(client_input);
        if(getPrefix(firstLine, prefix) != 0){return 1;}

        client_input = client_input + strlen(firstLine) + 2;
        char *secondLine = getLine(client_input);
        int nextLineLen = atoi(firstLine + 1);
        if(strlen(secondLine) != nextLineLen){return 1;}
        
        //Gets the 
        if(i == 0){
            request->command = findRedisCmd(secondLine, strlen(secondLine) - 2);
        }
        else{
            request->args[i-1] = secondLine;
        }
        client_input = client_input + strlen(secondLine) + 2;
    }
    if(*client_input != '\0'){return 1;}
    
    /**
     * *2\r\n
     * $4\r\n
     * ECHO\r\n
     * $11\r\n
     * Hello World\r\n
     */

    free(firstLinePtr);
    return 0;
    
}
/**
 * Create (ECHO)Handler and add this to it:
 * if(i == 0){
            int lineLength = strlen(firstLine);
            lineLength = lineLength - 2;
            currentMethod = findRedisCmd(firstLine, lineLength);
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
REDIS_CMDS findRedisCmd(char *cmdName, int lineLength){
    toUpper(cmdName);
    //Utility cmds
    if(strncmp(cmdName, "ECHO", lineLength) == 0){return ECHO;}
    if(strncmp(cmdName, "PING", lineLength) == 0){return PING;}
    if(strncmp(cmdName, "AUTH", lineLength) == 0){return AUTH;}
    if(strncmp(cmdName, "SELECT", lineLength) == 0){return SELECT;}
    if(strncmp(cmdName, "COMMAND", lineLength) == 0){return COMMAND;}

    //Core cmds
    if(strncmp(cmdName, "SET", lineLength) == 0){return SET;}
    if(strncmp(cmdName, "GET", lineLength) == 0){return GET;}
    if(strncmp(cmdName, "DEL", lineLength) == 0){return DEL;}
    if(strncmp(cmdName, "EXISTS", lineLength) == 0){return EXISTS;}
    if(strncmp(cmdName, "EXPIRE", lineLength) == 0){return EXPIRE;}
    if(strncmp(cmdName, "TTL", lineLength) == 0){return TTL;}

    //Cmds for strings and numbers
    if(strncmp(cmdName, "INCR", lineLength) == 0){return INCR;}
    if(strncmp(cmdName, "DECR", lineLength) == 0){return DECR;}
    if(strncmp(cmdName, "APPEND", lineLength) == 0){return APPEND;}
    if(strncmp(cmdName, "STRLEN", lineLength) == 0){return STRLEN;}
    if(strncmp(cmdName, "MGET", lineLength) == 0){return MGET;}

    //List cmds
    if(strncmp(cmdName, "HSET", lineLength) == 0){return HSET;}
    if(strncmp(cmdName, "HGET", lineLength) == 0){return HGET;}
    if(strncmp(cmdName, "HGETALL", lineLength) == 0){return HGETALL;}
    if(strncmp(cmdName, "HDEL", lineLength) == 0){return HDEL;}

    //Sets cmds
    if(strncmp(cmdName, "SADD", lineLength) == 0){return SADD;}
    if(strncmp(cmdName, "SREM", lineLength) == 0){return SREM;}
    if(strncmp(cmdName, "SMEMBERS", lineLength) == 0){return SMEMBERS;}
    if(strncmp(cmdName, "SISMEMBER", lineLength) == 0){return SISMEMBER;}

    return UNKOWN;
}