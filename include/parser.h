#ifndef PARSER_H
#define PARSER_H




typedef enum {
    UNKNOWN,

    // Utility cmds
    ECHO,
    PING,
    INFO,
    REPLCONF,
    WAIT,
    PSYNC,
    SELECT,
    COMMAND,

    //RDB cmds
    CONFIG_GET,
    KEYS,

    // Core cmds 
    SET,
    GET,
    DEL,
    EXISTS,
    EXPIRE,
    TTL,
    TYPE,

    // String & Number specific cmds
    INCR,
    DECR,
    APPEND,
    STRLEN,
    MGET,

    // Transaction cmds (המיקום המומלץ)
    MULTI,
    EXEC,
    DISCARD,
    WATCH,

    // List cmds 
    LPUSH,
    RPUSH,
    LPOP,
    RPOP,
    LLEN,
    LRANGE,
    BLPOP,

    // Hash cmds
    HSET,
    HGET,
    HGETALL,
    HDEL,

    // Sets cmds
    SADD,
    SREM,
    SMEMBERS,
    SISMEMBER,

    // Sorted sets
    ZADD,
    ZRANK,
    ZRANGE,
    ZCARD,
    ZSCORE,
    ZREM,
    GEOADD,
    GEOPOS,
    GEODIST,
    GEOSEARCH,

    //Auth
    AUTH,
    ACL,

    // Stream cmds
    XADD,
    XREAD,
    XRANGE,
    XGROUP,

    //pub/sub
    SUBSCRIBE,
    PUBLISH,
    UNSUBSCRIBE,
    

} REDIS_CMDS;

typedef struct {
    REDIS_CMDS command;
    char *args[16];          
    int argc; 
} RespRequest;

REDIS_CMDS findRedisCmd(char *cmdName);
REDIS_CMDS findConfigCmd(char *cmdName);
int parse(char **client_input, RespRequest *request);
char* getLine(char *client_input);
int getPrefix(char line[], char *prefix);

#endif