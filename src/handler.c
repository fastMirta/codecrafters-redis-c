#include <stdlib.h>
#include <string.h>
#include <parser.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <utils.h>


const char *ping_response = "+PONG\r\n";

int handle(RespRequest *req, int client_fd){
    
    /**    REDIS_CMDS command;
    char *args[16];          
    int argc;  
    */
   
    if(req == NULL){
        send(client_fd, "Request parsed is null", 22, 0);
        return 1;
    }
   
    if(strcmp(req->command, "ECHO") == 0){
        handle_echo(req, client_fd);
        return 0;
    }
    if(strcmp(req->command, "PING") == 0){
        handle_ping(client_fd);
        return 0; 
    }
    if(strcmp(req->command, "AUTH") == 0){
        return 0; 
    }
    if(strcmp(req->command, "SELECT") == 0){
        return 0; 
    }
    if(strcmp(req->command, "COMMAND") == 0){
        return 0; 
    }

    //Core cmds
    if(strcmp(req->command, "SET") == 0){
        handle_set(req, client_fd);
        return 0;
    }
    if(strcmp(req->command, "GET") == 0){
        handle_get(req, client_fd);
        return 0; 
    }
    if(strcmp(req->command, "DEL") == 0){
        return 0; 
    }
    if(strcmp(req->command, "EXISTS") == 0){
        return 0; 
    }
    if(strcmp(req->command, "EXPIRE") == 0){
        return 0; 
    }
    if(strcmp(req->command, "TTL") == 0){
        return 0;
    }

    //Cmds for strings and numbers
    if(strcmp(req->command, "INCR") == 0){
        return 0;
    }
    if(strcmp(req->command, "DECR") == 0){
        return 0;
    }
    if(strcmp(req->command, "APPEND") == 0){
        return 0;
    }
    if(strcmp(req->command, "STRLEN") == 0){
        return 0;
    }
    if(strcmp(req->command, "MGET") == 0){
        return 0;
    }

    //List cmds
    if(strcmp(req->command, "HSET") == 0){
        return 0;
    }
    if(strcmp(req->command, "HGET") == 0){
        return 0;
    }
    if(strcmp(req->command, "HGETALL") == 0){
        return 0;
    }
    if(strcmp(req->command, "HDEL") == 0){
        return 0;
    }

    //Sets cmds
    if(strcmp(req->command, "SADD") == 0){
        return 0;
    }
    if(strcmp(req->command, "SREM") == 0){
        return 0;
    }
    if(strcmp(req->command, "SMEMBERS") == 0){
        return 0;
    }
    if(strcmp(req->command, "SISMEMBER") == 0){
        return 0;
    }

    
    return 0; UNKOWN;

}







void handle_ping(int client_fd){
    send(client_fd, ping_response, strlen(ping_response), 0);
    free(ping_response);
}

void handle_echo(RespRequest *req, int client_fd){
    char response[1024];
	snprintf(response, sizeof(response), "$%d\r\n%s\r\n", (int)strlen(req->args[0]), req->args[0]);
	send(client_fd, response, strlen(response), 0);

    free(response);
}

void handle_set(RespRequest *req, int client_fd){
    if(strlen(req->args) < 2){return;}
    store_set(req->args[0], req->args[1]);
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

void handle_unkown(RespRequest *req, int client_fd){

}


