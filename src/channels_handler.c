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
        const char *channel = req->args[i];

        if (client->channel_count >= 64) break;

        int already = 0;
        for (int j = 0; j < client->channel_count; j++) {
            if (strcmp(client->channel_subed[j], channel) == 0) {
                already = 1;
                break;
            }
        }

        if (!already) {
            client->channel_subed[client->channel_count] = strdup(channel);
            client->channel_count++;
        }

        int written = snprintf(msg + offset, sizeof(msg) - offset,
            "*3\r\n$9\r\nsubscribe\r\n$%zu\r\n%s\r\n:%d\r\n",
            strlen(req->args[i]), req->args[i], client->channel_count);

        if (written > 0 && offset + written < sizeof(msg)) {
            offset += written;
        } else {
            break;
        }
    }
    printf("first val: %s\n", client->channel_subed[0]);
    printf("Msg is: %s\n", msg);
    printf("Offset: %d\n", offset);
    send(client->fd, msg, offset, 0);

}

void handle_publish(RespRequest *req){
    for(int i = 0; i < MAX_CLIENTS; i++){
        
    }
}