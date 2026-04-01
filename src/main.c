#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

const char *response = "+PONG\r\n";

int main(){
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

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

	int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
	if (client_fd == -1){
		printf("Accept failed: %s \n", strerror(errno));
		close(server_fd);
		return 1;
	}

	char buffer[1000];
	while (1){
		int received = recv(client_fd, buffer, sizeof(buffer), 0);

		if (received <= 0)
		{

			break;
		}

		if (send(client_fd, response, strlen(response), 0) == -1)
		{

			printf("Send failed: %s \n", strerror(errno));
		}
	}

	close(client_fd);
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

void toUpper(char *str){
	for (int i = 0; str[i]; i++)
		str[i] = toupper((unsigned char)str[i]);
}