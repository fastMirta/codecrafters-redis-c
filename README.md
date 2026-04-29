# Redis Clone in C

A from-scratch Redis server implementation in C, built as part of the
[CodeCrafters "Build Your Own Redis" Challenge](https://codecrafters.io/challenges/redis).

## What's implemented

- RESP protocol parser
- Core commands: `SET`, `GET`, `DEL`, `INCR`, `DECR`, `EXPIRE`, `TTL`, `TYPE`, `KEYS`
- List commands: `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`, `LLEN`, `BLPOP`
- Sorted set commands: `ZADD`, `ZRANK`, `ZRANGE`, `ZCARD`, `ZSCORE`, `ZREM`
- Geo commands: `GEOADD`, `GEOPOS`, `GEODIST`, `GEOSEARCH`
- Stream commands: `XADD`, `XREAD`, `XRANGE`
- Pub/Sub: `SUBSCRIBE`, `PUBLISH`, `UNSUBSCRIBE`
- Transactions: `MULTI`, `EXEC`, `DISCARD`, `WATCH`, `UNWATCH`
- Auth: `AUTH`, `ACL`
- Replication: master/replica handshake, `REPLCONF`, `PSYNC`, `WAIT`
- RDB persistence: loading keys from `.rdb` files on startup
- AOF persistence: writing commands to append-only file, replaying on startup
- Config: `CONFIG GET` for `dir`, `dbfilename`, `appendonly`, `appenddirname`, `appendfilename`, `appendfsync`

## Running

```sh
cmake -B build && cmake --build build
./your_program.sh
```

With options:
```sh
./your_program.sh --port 6380 --dir /tmp --dbfilename dump.rdb
./your_program.sh --appendonly yes --appenddirname mydir --appendfilename myfile.aof
./your_program.sh --replicaof "127.0.0.1 6379"
```

## Project structure

```
src/
  main.c              # entry point, event loop, config parsing
  parser.c            # RESP protocol parser
  handler.c           # command dispatch, AOF write
  utils.c             # hash table, store_set/get, stream helpers
  rdb_handler.c       # RDB file loading, CONFIG GET
  list_handler.c      # list commands
  set_handler.c       # sorted set and geo commands
  transaction_handler.c
  utility_handler.c   # INFO, REPLCONF, PSYNC, WAIT
  channels_handler.c  # pub/sub
  auth_handler.c      # AUTH, ACL
```