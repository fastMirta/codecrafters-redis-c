#include "auth_handler.h"


void sha256_hex(const char *input, char *output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH]; // 32 bytes
    SHA256((unsigned char*)input, strlen(input), hash);
    
    // convert to hex string (64 chars + null)
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        snprintf(output_hex + (i * 2), 3, "%02x", hash[i]);
    }
    output_hex[64] = '\0';
}

void handle_getuser(RespRequest *req, Client *client){
    toUpper(req->args[1]);
    if(strcmp(req->args[1], "DEFAULT") == 0){
        if(client->has_nopass){
            send(client->fd, "*4\r\n$5\r\nflags\r\n*1\r\n$6\r\nnopass\r\n$9\r\npasswords\r\n*0\r\n", 50, 0);
        }
        else{
            char buffer[1026];
            snprintf(buffer, sizeof(buffer), "*4\r\n$5\r\nflags\r\n*0\r\n$9\r\npasswords\r\n*1\r\n$%zd\r\n%s\r\n",
            strlen(client->password), client->password);

            send(client->fd, buffer, strlen(buffer), 0);
        }

    }
    else{
        send(client->fd, "*2\r\n$5\r\nflags\r\n*0\r\n", 19, 0);
    }
    return;
}

void handle_setuser(RespRequest *req, Client *client){
    if(req->argc < 3 || req->args[2][0] != '>'){
        printf("Bad request\n");
        send(client->fd, "-ERR bad request sonion\r\n", 25, 0);
        return;
    }
    client->has_nopass = 0;
    toUpper(req->args[1]);
    if(strcmp(req->args[1], "DEFAULT") == 0){
        sha256_hex(req->args[2] + 1, client->password);
        send(client->fd, "+OK\r\n", 5, 0);
    }
    else{

    }
    return;
}

void handle_acl(RespRequest *req, Client *client){
    toUpper(req->args[0]);
    if(strcmp(req->args[0], "WHOAMI") == 0){
        int is_authed = client->is_auth || !client->has_nopass;
        is_authed
        ? (send(client->fd, "$7\r\ndefault\r\n", 13, 0), printf("sent default\n"), printRequest(req))
        : (send(client->fd, "-NOAUTH Authentication required.\r\n", 34, 0), printf("sent noauth error\n"), printRequest(req));
        
        return;
    }
    if(strcmp(req->args[0], "GETUSER") == 0){
        handle_getuser(req, client);
        return;
    }
    if(strcmp(req->args[0], "SETUSER") == 0){
        handle_setuser(req, client);
        client->is_auth = 1;
        return;
    }
}

void handle_auth(RespRequest *req, Client *client){
    //TODO: add support to the single word auth cmd (only pw)
    if(req->argc < 2) return;

    char hashed_input[65];;
    sha256_hex(req->args[1], hashed_input);
    if(strcmp(client->password, hashed_input) == 0){
        send(client->fd, "+OK\r\n", 5, 0);
        printf("EQUALS\n");
        return;
    }
    
    
    send(client->fd, "-WRONGPASS invalid username-password pair or user is disabled\r\n", 63, 0);
}

