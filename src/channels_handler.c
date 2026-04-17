#include "channels_handler.h"



void get_channels(RespRequest *req, char *buffer, int sizeOfBuffer){
    if(req->argc < 1){return;}

    int offset = 0;

    for (int i = 0; i < req->argc; i++) {
        int written = snprintf(buffer + offset, sizeof(buffer) - offset, 
                            "%s", req->args[i]);
        
        if (written > 0 && offset + written < sizeof(buffer)) {
            offset += written;
        }
        else {
            break;
        }
    }
    
}

void handle_subscribe(RespRequest *req, Client *client){
    client->is_subscribed = 1;

    char channels[1024];
    get_channels(req, channels, 1024);

    char msg[1024];
    int offset = snprintf(msg, sizeof(msg), "*%ld\r\n$9\r\nsubscribe\r\n", strlen(channels) + 1);
    for(int i = 0; i < req->argc; i++){

        int remaining = sizeof(msg) - offset;
        if (remaining <= 0) break;

        int written = snprintf(msg + offset, sizeof(msg) - offset, 
                            "$%zu\r\n%s\r\n", strlen(req->args[i]), req->args[i]);
        
        if (written > 0 && offset + written < sizeof(msg)) {
            offset += written;
        }
        else {
            break;
        }
    }
    send(client->fd, msg, offset, 0);

}

void handle_publish(RespRequest *req){
    for(int i = 0; i < MAX_CLIENTS; i++){
        
    }
}