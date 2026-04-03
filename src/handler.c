#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "utils.h"



int handle_set_flags(RespRequest *req, int *expireAt, TIME_FLAGS *flag){
    printf("\n");
    printf("ENTERED FLAGS HANDLER\n");
    
    if(req->argc <= 1){
        printf("Doesnt have flags");
        return 1;
    }
    int indexSave = 0;
    for(int i = 1; i < req->argc; i++){
        printf("%s\n", req->args[i]);
        if(strcmp(req->args[i], "EX") == 0){
            indexSave = i;
            *flag = EX;
            break;
        }
        else if(strcmp(req->args[i], "PX") == 0){
            indexSave = i;
            *flag = PX;
            break;
        }
        
    }
    if(*flag == NO_TIME_FLAG){
        *expireAt = -1;
        return 0;
    }
    if(indexSave + 1 >= req->argc - 1){
        return 1;
    }
    *expireAt = atoi(req->args[indexSave + 1]);
    return 0;

    
}



void handle_ping(int client_fd){
    const char *ping_response = "+PONG\r\n";
    printf("Client fd: %d", client_fd);
    if(send(client_fd, ping_response, strlen(ping_response), 0) != -1){
        printf("SEND SUCCESS");
    }
    else{
        printf("\n");
        printf("SEND failed inside handle ping \n");
    }
}

void handle_echo(RespRequest *req, int client_fd){
    char response[1024];
    snprintf(response, sizeof(response), "$%zu\r\n%s\r\n", strlen(req->args[0]), req->args[0]);
	send(client_fd, response, strlen(response), 0);

}

void handle_set(RespRequest *req, int client_fd){
    if(req->argc < 3){
        printf("length smaller than 3");
        return;
    }
    TIME_FLAGS time = NO_TIME_FLAG;
    int seconds = 0;
    if(handle_set_flags(req, &seconds, &time) == 0){
        store_set(req->args[0], req->args[1], time, seconds);
        send(client_fd, "+OK\r\n", 5, 0);
    }
    else{
        send(client_fd, "+FAIL\r\n", 7, 0);
    }

}

void handle_get(RespRequest *req, int client_fd) {


    char response[1024];
    char *value = store_get(req->args[0]); 
    if (value == NULL) {
        send(client_fd, "$-1\r\n", 5, 0);  
        return;
    }
    snprintf(response, sizeof(response), "$%d\r\n%s\r\n", (int)strlen(value), value);
    send(client_fd, response, strlen(response), 0);
}

void handle_unknown(RespRequest *req, int client_fd){

}

int handle(RespRequest *req, int client_fd){
    printf("\n ENTERED HANDLER");
    /**    REDIS_CMDS command;
    char *args[16];          
    int argc;  
    */
    
    if(req == NULL){
        printf("ERROR IN HANDLE");
        send(client_fd, "Request parsed is null", 22, 0);
        return 1;
    }
   
    if(req->command== ECHO){
        handle_echo(req, client_fd);
        printf("ECHOED");
        return 0;
    }
    if(req->command == PING){
        handle_ping(client_fd);
        printf("Pinged\n");
        return 0; 
    }
    if(req->command == AUTH){
        return 0; 
    }
    if(req->command == SELECT){
        return 0; 
    }
    if(req->command == COMMAND){
        return 0; 
    }

    //Core cmds
    if(req->command == SET){
        printf("SET");
        handle_set(req, client_fd);
        return 0;
    }
    if(req->command == GET){
        printf("GET");
        handle_get(req, client_fd);
        return 0; 
    }
    if(req->command == DEL){
        return 0; 
    }
    if(req->command == EXISTS){
        return 0; 
    }
    if(req->command == EXPIRE){
        return 0; 
    }
    if(req->command == TTL){
        return 0;
    }

    //Cmds for strings and numbers
    if(req->command == INCR){
        return 0;
    }
    if(req->command == DECR){
        return 0;
    }
    if(req->command == APPEND){
        return 0;
    }
    if(req->command == STRLEN){
        return 0;
    }
    if(req->command == MGET){
        return 0;
    }

    //List cmds
    if(req->command == HSET){
        return 0;
    }
    if(req->command == HGET){
        return 0;
    }
    if(req->command == HGETALL){
        return 0;
    }
    if(req->command == HDEL){
        return 0;
    }

    //Sets cmds
    if(req->command == SADD){
        return 0;
    }
    if(req->command == SREM){
        return 0;
    }
    if(req->command == SMEMBERS){
        return 0;
    }
    if(req->command == SISMEMBER){
        return 0;
    }

    //TODO: implement handle_unknown();
    return 1; //Default = UNKNOWN;

}










