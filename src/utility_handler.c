#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "handler.h"


void handle_info(RespRequest *req ,int client_fd){
    if(req->argc < 1){
        //TODO: send full info data
        return;
    }

    toUpper(req->args[0]);
    if(strcmp(req->args[0], "REPLICATION") == 0){
        char info_content[1024];

        int content_len = snprintf(info_content, sizeof(info_content),
            "role:%s\r\nmaster_replid:%s\r\nmaster_repl_offset:%d\r\n",
            server_config.role, 
            server_config.master_replid, 
            server_config.master_repl_offset);

        char final_response[1200];
        int final_len = snprintf(final_response, sizeof(final_response),
            "$%d\r\n%s\r\n", 
            content_len, 
            info_content);
        
        send(client_fd, final_response, strlen(final_response), 0);
    }
}