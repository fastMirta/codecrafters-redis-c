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
#include "parser.h"
#include "handler.h"

#define MAX_CLIENTS 100

const char *response = "+PONG\r\n";

void printWord(char word[]){
	for(int i = 0; i < strlen(word); i++){
		printf("%c", word[i]);
	}
}

int main(){
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);	

	struct pollfd watch_list[MAX_CLIENTS];
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	{
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(6379),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	printf("Client connected\n");
	watch_list[0].fd = server_fd;
	watch_list[0].events = POLLIN;
	int active_fds = 1;
	//Implementation of event loop Hopefuly will work
	while(1){
		poll(watch_list, active_fds, -1);
		for (int i = 0; i < active_fds; i++) {
            
            //Check if this specific socket have an event?
            if (watch_list[i].revents & POLLIN) {
                
                if (i == 0) {
                    //Creating new client
                    int new_client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                    
                    
                    watch_list[active_fds].fd = new_client_fd;
                    watch_list[active_fds].events = POLLIN;
                    active_fds++;
                    printf("New client joined the loop!\n");
                } 
                else {
                    //EXISTING CLIENT SENT DATA
                    char *buffer = calloc(1025, sizeof(char));
                    int bytes_received = recv(watch_list[i].fd, buffer, 1024, 0);
					

                    if (bytes_received <= 0) {
                        //Removing client from watch list
                        close(watch_list[i].fd);
                        watch_list[i] = watch_list[active_fds - 1];
                        active_fds--;
                    } else {
						buffer[bytes_received] = '\0';
						printWord(buffer); 
						RespRequest request;
						parse(buffer, &request);
						printf("\n");
						printf("\n");
						printf("client I fd: %d", watch_list[i].fd);
						if(handle(&request, watch_list[i].fd) == 0){
							printf("Success");
							printf("YAY");
						}
                        
                    }
                }
            }
        }
	}


	close(server_fd);

	return 0;
}



int countWord(const char *str, const char *target, int *counter){
	if (!str || !target || !counter || *target == '\0')
		return 1;

	*counter = 0;
	int target_len = strlen(target);

	while ((str = strstr(str, target)) != NULL)
	{
		(*counter)++;
		str += target_len;
	}

	return (*counter == 0) ? 1 : 0;
}

