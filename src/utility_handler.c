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
            "role:%s\r\nmaster_replid:%s\r\nmaster_repl_offset:%lld\r\n",
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


void handle_replconf(char *response, long responseSize) {
    printf("Entered handle_replconf\n");
    
    char replconfData[1024];
    int len;

    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%d", server_config.port);

    len = snprintf(replconfData, sizeof(replconfData),
        "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$%zu\r\n%s\r\n",
        strlen(portStr), portStr);
    send(server_config.master_fd, replconfData, len, 0);

    memset(response, 0, responseSize);
    recv(server_config.master_fd, response, responseSize - 1, 0);
    printf("Master response to REPLCONF listening-port: %s\n", response);

    len = snprintf(replconfData, sizeof(replconfData),
        "*5\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$3\r\neof\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n");

    send(server_config.master_fd, replconfData, len, 0);

    memset(response, 0, responseSize);
    recv(server_config.master_fd, response, responseSize - 1, 0);
    printf("Master response to REPLCONF capabilities: %s\n", response);
}

void handle_psync(char *replicationId, int offset){
    char offsetBuf[16];
    int offsetLen = snprintf(offsetBuf, sizeof(offsetBuf), "%d", offset);

    char psyncData[1024];
   snprintf(psyncData, sizeof(psyncData), 
             "*3\r\n$5\r\nPSYNC\r\n$%zu\r\n%s\r\n$%d\r\n%d\r\n", 
             strlen(replicationId), replicationId, offsetLen, offset);
    send(server_config.master_fd, psyncData, strlen(psyncData), 0);
}

void replconf_handle_getack(int client_fd) {
    char buf[128];
    char offsetStr[32];
    int offsetLen = snprintf(offsetStr, sizeof(offsetStr), "%lld", server_config.slave_repl_offset);

    int len = snprintf(buf, sizeof(buf),
        "*3\r\n$8\r\nREPLCONF\r\n$3\r\nACK\r\n$%d\r\n%s\r\n",
        offsetLen, offsetStr);

    send(client_fd, buf, len, 0);
    printf("send buffer: %s\n", buf);
}

void replconf_handle_ack(RespRequest *request, Client *client) {
    if (request->argc < 2) {
        printf("Error: ACK missing offset argument\n");
        return;
    }
    client->repl_offset = atoll(request->args[1]);
    printf("Replica fd=%d acknowledged offset %lld\n", client->fd, client->repl_offset);
}

void handle_wait(RespRequest *req, int client_fd) {
    printf("Called wait");
    server_config.captured_master_offset = server_config.master_repl_offset;
    int wantedReplicas = atoi(req->args[0]);
    long long ms = atoll(req->args[1]);
    server_config.wantedReplicas = wantedReplicas;

    // Count currently connected replicas that are caught up to date with master server
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->is_replica &&
                clients[i]->repl_offset >= server_config.captured_master_offset)
            count++;
    }

    if (count >= wantedReplicas) {
        char reply[32];
        int len = snprintf(reply, sizeof(reply), ":%d\r\n", count);
        send(client_fd, reply, len, 0);
        return;
    }


    server_config.wait_client_fd = client_fd;
    server_config.wantedReplicas = wantedReplicas;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->is_replica) {
            char buf[128];
            int len = snprintf(buf, sizeof(buf), "*3\r\n$8\r\nREPLCONF\r\n$6\r\nGETACK\r\n$1\r\n*\r\n");
            send(clients[i]->fd, buf, len, 0);
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->fd == client_fd) {
            clients[i]->is_waiting = 1;
            clients[i]->is_blocked = 1;
            clients[i]->timeout_at = (ms == 0) ? 0 : get_current_time_ms() + ms;
            break;
        }
    }
}



void handle_handshake(){
    
}
