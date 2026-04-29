#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <ctype.h>
#include <time.h>
#include <arpa/inet.h>
#include "parser.h"
#include "handler.h"
#include "rdb_handler.h"
#include "list_hander.h"

RedisConfig server_config;
Client *clients[MAX_CLIENTS];


void printWord(char word[]){
    printf("%s", word);
}

long long get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)(ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

int findPortIndex(int argc, char *argv[], int *portIndex){
    for(int i = 0; i < argc; i++){
        printf("argv[%d] = %s\n", i, argv[i]);
        if(strcmp(argv[i], "--port") == 0 && i + 1 < argc){
            *portIndex = i + 1;
            return 0;
        }
    }
    return 1;
}

int connect_to_master(const char *host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in master_addr;
    
    master_addr.sin_family = AF_INET;
    master_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &master_addr.sin_addr); 

    if (connect(sock, (struct sockaddr *)&master_addr, sizeof(master_addr)) < 0) {
        perror("Connect to master failed");
        return -1;
    }
    return sock; 
}

/**Handles replicaof cmd */
void replicaofHandler(int argc, char *argv[]) {    
    server_config.role = "master"; 

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--replicaof") == 0 && i + 1 < argc) {
            server_config.role = "slave";

            char *arg = argv[i + 1];
            
            char *space = strchr(arg, ' ');

            //Checks if there are spaces in one argument so it would seperate them
            if (space != NULL) {
                int hostLen = space - arg;
                server_config.master_host = malloc(hostLen + 1);
                strncpy(server_config.master_host, arg, hostLen);
                server_config.master_host[hostLen] = '\0';
                server_config.master_port = atoi(space + 1);
            } else if (i + 2 < argc) {
                server_config.master_host = strdup(argv[i + 1]);
                server_config.master_port = atoi(argv[i + 2]);
            } else {
                printf("ERROR: --replicaof missing host/port\n");
                return;
            }

            printf("Connecting to master %s:%d\n", server_config.master_host, server_config.master_port);
            server_config.master_fd = connect_to_master(server_config.master_host, server_config.master_port);
            
            if (server_config.master_fd < 0) {
                printf("ERROR: Failed to connect to master\n");
                return;
            }

            const char *ping_cmd = "*1\r\n$4\r\nPING\r\n";
            send(server_config.master_fd, ping_cmd, strlen(ping_cmd), 0);
            printf("Sent PING to master\n");
            return;
        }
    }

    printf("No --replicaof flag, running as master\n");

}

/**Creates directory based on configed path */
void create_dir(){
    char path[2048];
    char file_path[2350];
    char manifest_file_path[2350];


    snprintf(path, sizeof(path), "%s/%s", server_config.rdb_directory, server_config.appenddirname);

    if (mkdir(path, 0777) == -1) {
        if (errno == EEXIST) {
            printf("Directory already exists: %s\n", path);
        } else {
            perror("Error creating directory");
        }
    } else {
        printf("Directory created successfully: %s\n", path);
    }

    snprintf(file_path, sizeof(file_path), "%s/%s.1.incr.aof", path, server_config.appendfilename);
    FILE *file = fopen(file_path, "a+b");
    if (file == NULL) {
        perror("Failed to create AOF file");
        return;
    }
    fclose(file);
    
    snprintf(manifest_file_path, sizeof(manifest_file_path), "%s/%s.manifest", path, server_config.appendfilename);
    FILE *manifestFile = fopen(manifest_file_path, "a+b");
    if (manifestFile == NULL) {
        perror("Failed to create AOF file");
        return;
    }
    fclose(manifestFile);
    
    printf("AOF file created/opened at: %s\n", file_path);
    
}

void rdb_config_handler(int argc, char *argv[]){
    for (int i = 0; i < argc; i++) {
        // printf("argv[%d]: = %s\n", i, argv[i]);
        // printf("argv[%d]: = %s\n", i+1, argv[i+1]);

        if(strcmp(argv[i], "--dir") == 0 && i + 1 < argc){
            strcpy(server_config.rdb_directory, argv[i + 1]);
        }

        if(strcmp(argv[i], "--dbfilename") == 0 && i + 1 < argc){
            strcpy(server_config.rdb_name, argv[i + 1]);
        }

        if(strcmp(argv[i], "--appendonly") == 0 && i + 1 < argc){
            printf("argv[%d]: = %s\n", i, argv[i]);
            printf("argv[%d]: = %s\n", i+1, argv[i+1]);
            strcpy(server_config.appendOnly, argv[i + 1]);
        }
        
        if(strcmp(argv[i], "--appenddirname") == 0 && i + 1 < argc){
            strcpy(server_config.appenddirname, argv[i + 1]);
        }

        if(strcmp(argv[i], "--appendfilename") == 0 && i + 1 < argc){
            strcpy(server_config.appendfilename, argv[i + 1]);
        }

        if(strcmp(argv[i], "--appendfsync") == 0 && i + 1 < argc){
            strcpy(server_config.appendfsync, argv[i + 1]);
        }
    }
    
    if(strcmp(to_upper(server_config.appendOnly), "YES") == 0){
        create_dir();
    }

    printf("found both?: name: %s, path: %s\n", server_config.rdb_name, server_config.rdb_directory);


}

int validate_server_response(int stepLevel, char *serversResponse){
    printf("%s\n", serversResponse);
    printf("step: %d\n", stepLevel);
    printf("server response is +PONG? %d\n", strcmp(serversResponse, "+PONG\r\n") == 0);
    printf("server response len %ld\n", strlen(serversResponse));

    if(stepLevel == 0 && strcmp(serversResponse, "+PONG\r\n") == 0){return 0;}
    if(stepLevel == 1 && strcmp(serversResponse, "+OK\r\n") == 0){return 0;}
    if (stepLevel == 2) {
        if (strncmp(serversResponse, "+FULLRESYNC", 11) == 0) {
            return 0; 
        }
    }

    return 1;

}


int copy_request_toBuffer(RespRequest *req, char *buffer, int bufferSize) {
    int offset = 0;
    const char *cmd_str = cmd_to_string(req->command);

    // List prefix "*"
    offset += snprintf(buffer + offset, bufferSize - offset, "*%d\r\n", req->argc + 1);

    // command name
    offset += snprintf(buffer + offset, bufferSize - offset, "$%zu\r\n%s\r\n", strlen(cmd_str), cmd_str);

    // args
    for (int i = 0; i < req->argc; i++) {
        offset += snprintf(buffer + offset, bufferSize - offset, "$%zu\r\n%s\r\n", strlen(req->args[i]), req->args[i]);
    }

    return offset;
}


static int recv_and_validate(char *buf, int bufSize, int step) {
    memset(buf, 0, bufSize);
    recv(server_config.master_fd, buf, bufSize - 1, 0);
    return validate_server_response(step, buf);
}

static char* drain_rdb(char *buf, long long bytesRead) {
    char *rdb_start = strchr(buf, '$');
    if (!rdb_start) return buf + bytesRead;

    int rdb_len = atoi(rdb_start + 1);
    char *data_start = strstr(rdb_start, "\r\n");
    if (!data_start) return buf + bytesRead;

    data_start += 2;
    char *rdb_end = data_start + rdb_len;

    int remaining = rdb_len - (int)((buf + bytesRead) - data_start);
    while (remaining > 0) {
        char discard[1024];
        int n = recv(server_config.master_fd, discard,
                     remaining < 1024 ? remaining : 1024, 0);
        if (n <= 0) break;
        remaining -= n;
    }

    return rdb_end;
}

static void add_master_to_watchlist(struct pollfd *watch_list, int *active_fds) {
    watch_list[*active_fds].fd     = server_config.master_fd;
    watch_list[*active_fds].events = POLLIN;
    watch_list[*active_fds].revents = 0;
    clients[*active_fds] = calloc(1, sizeof(Client));
    clients[*active_fds]->fd = server_config.master_fd;
    clients[*active_fds]->is_master = 1;
    (*active_fds)++;
    printf("Added master_fd to watch list\n");
}

static void do_slave_handshake(struct pollfd *watch_list, int *active_fds) {
    char buf[4096];

    if (recv_and_validate(buf, sizeof(buf), 0) != 0) {
        printf("Handshake failed at PING step\n");
        return;
    }
    printf("Data received from server: %s\n", buf);

    handle_replconf(buf, sizeof(buf));
    if (validate_server_response(1, buf) != 0) {
        printf("Handshake failed at REPLCONF step\n");
        return;
    }

    handle_psync("?", -1);
    long long bytesRead = recv(server_config.master_fd, buf, sizeof(buf) - 1, 0);
    buf[bytesRead] = '\0';
    printf("bytes read: %lld\nresponse to last step: %s\n", bytesRead, buf);

    char *after_rdb = drain_rdb(buf, bytesRead);

    add_master_to_watchlist(watch_list, active_fds);

    // Handle any commands that arrived in the same buffer as the RDB
    char *end = buf + bytesRead;
    char *ptr = after_rdb;
    while (ptr < end) {
        while (ptr < end && *ptr != '*') ptr++;
        if (ptr >= end) break;

        char *before = ptr;
        RespRequest request = {0};
        int result = parse(&ptr, &request);
        if (result != 0) {
            ptr = before + 1;
            continue;
        }

        int cmd_byte_len = ptr - before;
        handle(&request, clients[*active_fds - 1]);
        server_config.slave_repl_offset += cmd_byte_len;

        for (int a = 0; a < request.argc; a++)
            free(request.args[a]);
    }
}

/**Sends Write cmd to all replica */
void pass_data_toReplica(RespRequest *req){
    printf("pass_data_toReplica called, cmd=%d\n", req->command);
    if(isWriteCmd(req->command) == 0){
        printf("is write cmd, scanning clients\n");
        for(int i = 0; i < MAX_CLIENTS; i++){
            if(clients[i]) printf("clients[%d] is_replica=%d\n", i, clients[i]->is_replica);
            if(clients[i] && clients[i]->is_replica){
                char requestAsStr[1024];
                int len = copy_request_toBuffer(req, requestAsStr, sizeof(requestAsStr));
                printf("data sent to replica: %s", requestAsStr);
                send(clients[i]->fd, requestAsStr, len, 0);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    struct pollfd watch_list[MAX_CLIENTS];


    memset(watch_list, 0, sizeof(watch_list));
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = NULL;
        watch_list[i].fd = -1;
    }

    int server_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        return 1;
    }

    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int portIndex = -1;
    int port = 6379;
    if(findPortIndex(argc, argv, &portIndex) == 0){
        printf("WORKED!!!\n");
        printf("DEBUG: port index: argv[%d] = %s\n", portIndex, argv[portIndex]);
        port = atoi(argv[portIndex]);
    } else {
        printf("DEBUG: did not find port\n");
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
        perror("Bind failed");
        return 1;
    }

    server_config.port = port;
    server_config.master_replid = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
    server_config.master_repl_offset = 0;
    server_config.master_fd = -1;

    server_config.rdb_directory[0] = '\0';
    server_config.rdb_name[0] = '\0';
    server_config.appendOnly[0] = '\0';
    server_config.appenddirname[0] = '\0';
    server_config.appendfilename[0] = '\0';
    server_config.appendfsync[0] = '\0';

    strcpy(server_config.rdb_directory, "/app");
    strcpy(server_config.appenddirname, "appendonlydir");
    strcpy(server_config.appendOnly, "no");
    strcpy(server_config.appendfilename, "appendonly.aof");
    strcpy(server_config.appendfsync, "everysec");

    listen(server_fd, 5);

    printf("Waiting for a client to connect...\n");

    watch_list[0].fd = server_fd;
    watch_list[0].events = POLLIN;
    watch_list[0].revents = 0;
    int active_fds = 1;

    replicaofHandler(argc, argv);
    rdb_config_handler(argc, argv);
    int keysLoaded = load_rdb_into_table();
    printf("Keys loaded: %d\n", keysLoaded);

    // Does handshake only for slave
    if (strcmp(server_config.role, "slave") == 0) {
        do_slave_handshake(watch_list, &active_fds);
    }

    // for(int i = 0; i < MAX_CLIENTS; i++){
    //     clients[i]->channel_subed = strdup();
    // }
    memset(watchers, 0, sizeof(watchers));
    for(int i = 0; i < WATCHERS_SIZE; i++) {
            watchers[i] = NULL;
    }

    server_config.wait_client_fd = -1;
    while(1) {
        int poll_count = poll(watch_list, active_fds, 100);

        long long now = get_current_time_ms();


        // Handle blocked client timeouts (XREAD)
        for (int i = 1; i < active_fds; i++) {
            if (clients[i] && clients[i]->is_blocked
                    && clients[i]->timeout_at != 0
                    && now >= clients[i]->timeout_at
                    && !clients[i]->is_replica
                    && !clients[i]->is_waiting
                    && !clients[i]->is_blpop) {
                        printf("Entered timeout of xread");
                send(watch_list[i].fd, "*-1\r\n", 5, 0);
                clients[i]->is_blocked = 0;
            }
            
            handle_endblpop(clients[i]);
        }


        // Handle WAIT
        if (server_config.wait_client_fd != -1) {
            int count = 0;
            int timed_out = 0;
            printf("Entered wait timeout");
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] && clients[i]->is_replica &&
                        clients[i]->repl_offset >= server_config.captured_master_offset)
                    count++;
            }

            for (int i = 1; i < active_fds; i++) {
                if (clients[i] && clients[i]->fd == server_config.wait_client_fd &&
                        clients[i]->timeout_at != 0 && now >= clients[i]->timeout_at)
                    timed_out = 1;
            }

            if (count >= server_config.wantedReplicas || timed_out) {
                char reply[32];
                int len = snprintf(reply, sizeof(reply), ":%d\r\n", count);
                send(server_config.wait_client_fd, reply, len, 0);

                for (int i = 1; i < active_fds; i++) {
                    if (clients[i] && clients[i]->fd == server_config.wait_client_fd)
                        clients[i]->is_blocked = 0;
                        clients[i]->is_waiting = 0;
                }
                server_config.wait_client_fd = -1;
            }
        }

        if (poll_count <= 0) continue;

        for (int i = 0; i < active_fds; i++) {
            if (!(watch_list[i].revents & POLLIN)) continue;

            if (i == 0) {
                int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                if (new_fd < 0) continue;

                if (active_fds < MAX_CLIENTS) {
                    watch_list[active_fds].fd = new_fd;
                    watch_list[active_fds].events = POLLIN;
                    watch_list[active_fds].revents = 0;

                    clients[active_fds] = calloc(1, sizeof(Client));
                    clients[active_fds]->fd = new_fd;
                    clients[active_fds]->is_blpop = 0;
                    clients[active_fds]->is_blocked = 0;
                    clients[active_fds]->is_queued = 0;
                    clients[active_fds]->queuedCommands = 0;
                    clients[active_fds]->is_auth = 0;
                    clients[active_fds]->has_nopass = 1;
                    
                    clients[active_fds]->watch_keys_size = 0;
                    memset(clients[active_fds]->watch_keys, 0, sizeof(clients[active_fds]->watch_keys));
                    //clients[active_fds]->password = "";

                    active_fds++;
                    printf("New client joined! active_fds: %d\n", active_fds);
                } else {
                    close(new_fd);
                }
                continue;
            }

            if (clients[i]->is_blocked && !clients[i]->is_replica && !clients[i]->is_subscribed) continue;

            char buffer[4096];
            int bytes_received = recv(watch_list[i].fd, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received <= 0) {
                printf("Client disconnected at index %d, fd %d\n", i, watch_list[i].fd);
                close(watch_list[i].fd);
                if (clients[i]) free(clients[i]);

                watch_list[i] = watch_list[active_fds - 1];
                clients[i] = clients[active_fds - 1];

                clients[active_fds - 1] = NULL;
                watch_list[active_fds - 1].fd = -1;
                watch_list[active_fds - 1].revents = 0;

                active_fds--;
                i--;
            } else {
                buffer[bytes_received] = '\0';
                char *ptr = buffer;
                char *end = buffer + bytes_received;

                while (ptr < end) {
                    while (ptr < end && *ptr != '*') ptr++;
                    if (ptr >= end) break;

                    char *before = ptr;
                    RespRequest request = {0};
                    int result = parse(&ptr, &request);

                    if (result != 0) {
                        ptr = before + 1;
                        continue;
                    }

                    // byte length of this RESP message
                    int cmd_byte_len = ptr - before;
                    printf("Printing request\n");
                    printRequest(&request);
                    handle(&request, clients[i]);
                    
                    // Only propagate to replicas if this came from a real client, not from master
                    if (clients[i]->fd == server_config.master_fd) {
                        // came from master, updates slave offset
                        server_config.slave_repl_offset += cmd_byte_len;
                    } else {
                        
                        if (isWriteCmd(request.command) == 0) {
                            server_config.master_repl_offset += cmd_byte_len;
                        }
                        // came from real client, propagate to replicas
                        pass_data_toReplica(&request);
                    }

                    for (int a = 0; a < request.argc; a++)
                        free(request.args[a]);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}