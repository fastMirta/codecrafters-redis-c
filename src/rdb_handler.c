#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "handler.h"

void handle_config(RespRequest *req, int client_fd){
    if(req->argc > 2){
        printf("DEBUG: arguments are less than 2");
        return;
    }

    printf("printing config\n");
    printRequest(req);


    toUpper(req->args[0]);
    if(req->command == CONFIG_GET){
        toUpper(req->args[0]);
        printf("Print after got upper\n");
        char directoryBuffer[1024];
        printf("after setting buffer\n");
        if(strcmp(req->args[0], "DIR") == 0){
            
            //*2\r\n$3\r\ndir\r\n$16\r\n/tmp/redis-files\r\n
            snprintf(directoryBuffer, sizeof(directoryBuffer), "*2\r\n$3\r\ndir\r\n$%ld\r\n%s\r\n",
             strlen(server_config.rdb_directory), server_config.rdb_directory);
            send(client_fd, directoryBuffer, strlen(directoryBuffer), 0);
            return;
        }

        if(strcmp(req->args[0], "DBFILENAME") == 0){
            printf("DBFILE NAME 4 EVEA\n");
             snprintf(directoryBuffer, sizeof(directoryBuffer), "*2\r\n$3\r\ndir\r\n$%ld\r\n%s\r\n",
             strlen(server_config.rdb_name), server_config.rdb_name);
            send(client_fd, directoryBuffer, strlen(directoryBuffer), 0);
            return;
        }

    }
}

int read_rdb_file(char **keys, int max_keys){
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", server_config.rdb_directory, server_config.rdb_name);
    FILE *rdb = fopen(full_path, "rb");

    if(!rdb){return -1;}

     int key_count = 0;
    uint8_t byte;

    fseek(rdb, 9, SEEK_SET);

    while (fread(&byte, 1, 1, rdb) == 1) {
        if (byte == 0xFF) break;  // EOF marker

        if (byte == 0xFA) {
            for (int i = 0; i < 2; i++) {
                uint8_t len;
                fread(&len, 1, 1, rdb);
                if ((len & 0xC0) == 0xC0) {
                    int enc = len & 0x3F;
                    int skip = (enc == 0) ? 1 : (enc == 1) ? 2 : 4;
                    fseek(rdb, skip, SEEK_CUR);
                } else {
                    fseek(rdb, len & 0x3F, SEEK_CUR);
                }
            }
            continue;
        }

        if (byte == 0xFE) {
            fseek(rdb, 1, SEEK_CUR);
            continue;
        }

        if (byte == 0xFB) {
            for (int i = 0; i < 2; i++) {
                uint8_t len;
                fread(&len, 1, 1, rdb);
                if ((len & 0xC0) != 0xC0) {
                }
            }
            continue;
        }

        if (byte == 0xFC || byte == 0xFD) {
            fseek(rdb, (byte == 0xFC) ? 8 : 4, SEEK_CUR);
            fread(&byte, 1, 1, rdb);
        }

        uint8_t key_len_byte;
        if (fread(&key_len_byte, 1, 1, rdb) != 1) break;

        int key_len = key_len_byte & 0x3F;  
        if (key_count < max_keys) {
            keys[key_count] = malloc(key_len + 1);
            if (!keys[key_count]) break;
            fread(keys[key_count], 1, key_len, rdb);
            keys[key_count][key_len] = '\0';
            key_count++;
        }

        if ((byte & 0xFF) == 0) {
            uint8_t val_len_byte;
            fread(&val_len_byte, 1, 1, rdb);
            int val_len = val_len_byte & 0x3F;
            fseek(rdb, val_len, SEEK_CUR);
        }
    }

    fclose(rdb);
    return key_count;  
    

}

void handle_key(RespRequest *req, int client_fd){
   char *keys[256];
    int count = read_rdb_file(keys, 256);

    if (count < 0) {
        send(client_fd, "*0\r\n", 4, 0);
        return;
    }

    char resp[8192];
    int offset = 0;

    offset += snprintf(resp + offset, sizeof(resp) - offset,
                       "*%d\r\n", count);

    for (int i = 0; i < count; i++) {
        offset += snprintf(resp + offset, sizeof(resp) - offset,
                           "$%zu\r\n%s\r\n", strlen(keys[i]), keys[i]);
        free(keys[i]);
    }

    send(client_fd, resp, offset, 0);
}

