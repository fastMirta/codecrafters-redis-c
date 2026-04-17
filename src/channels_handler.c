#include "channels_handler.h"



void get_channels(RespRequest *req, char *buffer, int sizeOfBuffer){
    printf("req->argc < 1? %d\n", req->argc < 1);
    if(req->argc < 1){return;}
    printf("Start of cha cha cha channel\n");
    int offset = 0;

    for (int i = 0; i < req->argc; i++) {
        int written = snprintf(buffer + offset, sizeOfBuffer - offset, 
                            "%s", req->args[i]);
        
        if (written > 0 && offset + written < sizeOfBuffer) {
            offset += written;
        }
        else {
            break;
        }
    }
    printf("End of cha cha cha channel\n");
    
}

void handle_subscribe(RespRequest *req, Client *client){
    client->is_subscribed = 1;
    printf("Entered sub\n");
    
    char msg[1024];
    int offset = 0;
    for (int i = 0; i < req->argc; i++) {
        client->channel_subed[i] = strdup(req->args[i]);

        int written = snprintf(msg + offset, sizeof(msg) - offset,
            "*3\r\n$9\r\nsubscribe\r\n$%zu\r\n%s\r\n:%d\r\n",
            strlen(req->args[i]), req->args[i], i + 1);

        if (written > 0 && offset + written < sizeof(msg)) {
            offset += written;
        } else {
            break;
        }
    }
    printf("first val: %s\n", client->channel_subed[0]);
    printf("Msg is: %s\n", msg);
    printf("Offset: %d\n", offset);
    send(client->fd, msg, strlen(msg), 0);

}

void handle_publish(RespRequest *req){
    for(int i = 0; i < MAX_CLIENTS; i++){
        
    }
}