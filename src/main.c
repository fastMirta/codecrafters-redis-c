#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <ctype.h>
#include <time.h>
#include <arpa/inet.h>
#include "parser.h"
#include "handler.h"

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



int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    struct pollfd watch_list[MAX_CLIENTS];

    // Zero everything and set all fds to -1 so poll() skips unused slots
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
    }
    else{
        printf("DEUBG: didnt not find port\n");
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

    listen(server_fd, 5);

    printf("Waiting for a client to connect...\n");

    watch_list[0].fd = server_fd;
    watch_list[0].events = POLLIN;
    watch_list[0].revents = 0;
    int active_fds = 1;

    replicaofHandler(argc, argv);
    while(1) {
        int poll_count = poll(watch_list, active_fds, 100);

        long long now = get_current_time_ms();

        // Check for timed-out blocked clients
        for (int i = 1; i < active_fds; i++) {
            if (clients[i] && clients[i]->is_blocked
                    && clients[i]->timeout_at != 0
                    && now >= clients[i]->timeout_at) {
                send(watch_list[i].fd, "*-1\r\n", 5, 0);
                clients[i]->is_blocked = 0;
                printf("Checking client %d: now=%lld, target=%lld\n",
                    i, now, clients[i]->timeout_at);
                printf("Client at fd %d timed out\n", watch_list[i].fd);
            }
        }///home/tamir/codecrafters-redis-c/your_program.sh --port 6381 --replicaof "localhost 6379"

        if (poll_count <= 0) continue;

        for (int i = 0; i < active_fds; i++) {
            if (!(watch_list[i].revents & POLLIN)) continue;

            if (i == 0) {
                // New connection on server socket
                int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                if (new_fd < 0) continue;

                if (active_fds < MAX_CLIENTS) {
                    watch_list[active_fds].fd = new_fd;
                    watch_list[active_fds].events = POLLIN;
                    watch_list[active_fds].revents = 0; // explicitly zero — don't trigger this round

                    clients[active_fds] = calloc(1, sizeof(Client));
                    clients[active_fds]->fd = new_fd;
                    clients[active_fds]->is_blocked = 0;
                    clients[active_fds]->is_queued = 0;
                    clients[active_fds]->queuedCommands = 0;

                    active_fds++;
                    printf("New client joined! active_fds: %d\n", active_fds);
                } else {
                    close(new_fd);
                }
                continue;
            }

            // Existing client
            if (clients[i]->is_blocked) continue;

            char buffer[4096];
            int bytes_received = recv(watch_list[i].fd, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received <= 0) {
                printf("Client disconnected at index %d, fd %d\n", i, watch_list[i].fd);
                close(watch_list[i].fd);
                if (clients[i]) free(clients[i]);

                // Swap with last slot
                watch_list[i] = watch_list[active_fds - 1];
                clients[i] = clients[active_fds - 1];

                // Clear the old last slot
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
                    // Skip to next RESP command start
                    while (ptr < end && *ptr != '*') ptr++;
                    if (ptr >= end) break;

                    char *before = ptr;
                    RespRequest request = {0};
                    //server_config.master_repl_offset++;
                    int result = parse(&ptr, &request);

                    if (result != 0) {
                        ptr = before + 1;
                        continue;
                    }

                    printf("main loop: clients[%d] ptr = %p, fd = %d\n",
                        i, (void*)clients[i], clients[i]->fd);
                    printf("watch_list[%d].fd=%d  clients[%d]->fd=%d\n",
                        i, watch_list[i].fd, i, clients[i]->fd);

                    handle(&request, clients[i]);

                    for (int a = 0; a < request.argc; a++)
                        free(request.args[a]);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}