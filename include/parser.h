#ifndef PARSER_H
#define PARSER_H




typedef enum  {
    UNKNOWN,

    //Utility cmds
    ECHO,
    PING,
    AUTH,
    SELECT,
    COMMAND,

    //Core cmds
    SET,
    GET,
    DEL,
    EXISTS,
    EXPIRE,
    TTL,

    //Cmds for strings and numbers
    INCR,
    DECR,
    APPEND,
    STRLEN,
    MGET,

    //List cmds
    HSET,
    HGET,
    HGETALL,
    HDEL,

    //Sets cmds
    SADD,
    SREM,
    SMEMBERS,
    SISMEMBER,

} REDIS_CMDS;

typedef struct {
    REDIS_CMDS command;
    char *args[16];          
    int argc; 
} RespRequest;

REDIS_CMDS findRedisCmd(char *cmdName, int lineLength);
int parse(char *client_input, RespRequest *request);
char* getLine(char *client_input);
int getPrefix(char line[], char *prefix);

#endif