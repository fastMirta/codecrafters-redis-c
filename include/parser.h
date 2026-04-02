#ifndef UTILS_H
#define UTILS_H



typedef struct {
    REDIS_CMDS command;
    char **args;          
    int argc; 
} RespRequest;

typedef enum  {
    UNKOWN,

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

REDIS_CMDS findRedisCmd(char *cmdName);
int parse(char *client_input, RespRequest *request);

#endif