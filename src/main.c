#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <ctype.h>
#include <time.h>
#include "parser.h"
#include "handler.h"

const char *response = "+PONG\r\n";
Client *clients[MAX_CLIENTS];

void printWord(char word[]){
    printf("%s", word);
}

long long get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)(ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    struct pollfd watch_list[MAX_CLIENTS];

    for (int i = 0; i < MAX_CLIENTS; i++) clients[i] = NULL;

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

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(6379),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_fd, 5);

    printf("Waiting for a client to connect...\n");

    watch_list[0].fd = server_fd;
    watch_list[0].events = POLLIN;
    int active_fds = 1;

    while(1) {
        int poll_count = poll(watch_list, active_fds, 100);

        long long now = get_current_time_ms();

        for (int i = 1; i < active_fds; i++) {
            if (clients[i] && clients[i]->is_blocked && now >= clients[i]->timeout_at && clients[i]->timeout_at != 0) {
                send(watch_list[i].fd, "*-1\r\n", 5, 0);
                clients[i]->is_blocked = 0;
                printf("Checking client %d: now=%lld, target=%lld\n",
                    i, now, clients[i]->timeout_at);
                printf("Client at fd %d timed out\n", watch_list[i].fd);
            }
        }

        if (poll_count <= 0) continue;

        for (int i = 0; i < active_fds; i++) {
            if (watch_list[i].revents & POLLIN) {

                if (i == 0) {
                    int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                    if (active_fds < MAX_CLIENTS) {
                        watch_list[active_fds].fd = new_fd;
                        watch_list[active_fds].events = POLLIN;

                        clients[active_fds] = calloc(1, sizeof(Client));
                        clients[active_fds]->fd = new_fd;
                        clients[active_fds]->is_blocked = 0;

                        active_fds++;
                        printf("New client joined! active_fds: %d\n", active_fds);
                    } else {
                        close(new_fd);
                    }
                }
                else {
                    if (clients[i]->is_blocked) continue;

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
                        active_fds--;
                        i--;
                    } else {
                        buffer[bytes_received] = '\0';
                        char *ptr = buffer;
                        char *end = buffer + bytes_received;

                        while (ptr < end) {
                            // skip to next RESP command start
                            while (ptr < end && *ptr != '*') ptr++;
                            if (ptr >= end) break;

                            char *before = ptr;
                            RespRequest request = {0};
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
        }
    }

    close(server_fd);
    return 0;
}