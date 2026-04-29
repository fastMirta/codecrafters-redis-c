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
        printf("Print after got upper\n");
        char directoryBuffer[2048];
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

        if(strcmp(req->args[0], "APPENDONLY") == 0){
            //TODO: implement logic for appendonly
            char appendOnlyMsg[1024];
            snprintf(appendOnlyMsg, sizeof(appendOnlyMsg), "*2\r\n$10\r\nappendonly\r\n$%zd\r\n%s\r\n", 
             strlen(server_config.appendOnly), server_config.appendOnly);

             printf("Append only msg: %s\n", appendOnlyMsg);
             printf("server config apo: %s\n", server_config.appendOnly);

            send(client_fd, appendOnlyMsg, strlen(appendOnlyMsg), 0);
            return;
        }

        if(strcmp(req->args[0], "APPENDDIRNAME") == 0){
            char appendDirNameMsg[2048];
            snprintf(appendDirNameMsg, sizeof(appendDirNameMsg), "*2\r\n$13\r\nappenddirname\r\n$%zd\r\n%s\r\n",
             strlen(server_config.appenddirname), server_config.appenddirname);

            send(client_fd, appendDirNameMsg, strlen(appendDirNameMsg), 0);
            return;
        }
        
        if(strcmp(req->args[0], "APPENDFILENAME") == 0){
            char appendFileNameMsg[1024];
            snprintf(appendFileNameMsg, sizeof(appendFileNameMsg), "*2\r\n$14\r\nappendfilename\r\n$%zd\r\n%s\r\n",
             strlen(server_config.appendfilename), server_config.appendfilename);

            send(client_fd, appendFileNameMsg, strlen(appendFileNameMsg) , 0);
            return;
        }

        if(strcmp(req->args[0], "APPENDFSYNC") == 0){
            char appendFsyncMsg[1024];
            snprintf(appendFsyncMsg, sizeof(appendFsyncMsg), "*2\r\n$11\r\nappendfsync\r\n$%zd\r\n%s\r\n", 
             strlen(server_config.appendfsync), server_config.appendfsync);

            send(client_fd, appendFsyncMsg, strlen(appendFsyncMsg), 0);
            return;
        }
    }
}

int read_rdb_file(char **keys, int max_keys){
    char full_path[2048];
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

void handle_key(RespRequest *req, int client_fd) {
    char *keys[256];
    int count = 0;

    for (int i = 0; i < TABLE_SIZE; i++) {
        Entry *entry = table[i];
        while (entry != NULL) {
            if (count < 256)
                keys[count++] = entry->key;
            entry = entry->next;
        }
    }

    char resp[8192];
    int offset = 0;
    offset += snprintf(resp + offset, sizeof(resp) - offset, "*%d\r\n", count);
    for (int i = 0; i < count; i++) {
        offset += snprintf(resp + offset, sizeof(resp) - offset,
            "$%zu\r\n%s\r\n", strlen(keys[i]), keys[i]);
    }
    send(client_fd, resp, offset, 0);
}



// Read a length-encoded integer from RDB, return -1 on error.
// Sets *is_encoded if it's a special int-encoded string.
static int rdb_read_length(FILE *f, int *is_encoded) {
    if (is_encoded) *is_encoded = 0;
    uint8_t byte;
    if (fread(&byte, 1, 1, f) != 1) return -1;

    uint8_t enc = (byte & 0xC0) >> 6;

    if (enc == 0) return byte & 0x3F;           // 6-bit length

    if (enc == 1) {                              // 14-bit length
        uint8_t next;
        if (fread(&next, 1, 1, f) != 1) return -1;
        return ((byte & 0x3F) << 8) | next;
    }

    if (enc == 2) {                              // 32-bit length (big-endian)
        uint8_t buf[4];
        if (fread(buf, 1, 4, f) != 4) return -1;
        return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    }

    if (is_encoded) *is_encoded = 1;
    return byte & 0x3F;  // 0=int8, 1=int16, 2=int32
}

// Read a length-prefixed string, returns heap-allocated string or NULL.
static char *rdb_read_string(FILE *f) {
    int is_encoded;
    int len = rdb_read_length(f, &is_encoded);
    if (len < 0) return NULL;

    if (is_encoded) {
        int nbytes = (len == 0) ? 1 : (len == 1) ? 2 : 4;
        int32_t val = 0;
        if (fread(&val, 1, nbytes, f) != (size_t)nbytes) return NULL;
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", val);
        return strdup(buf);
    }

    char *s = malloc(len + 1);
    if (!s) return NULL;
    if (len > 0 && fread(s, 1, len, f) != (size_t)len) { free(s); return NULL; }
    s[len] = '\0';
    return s;
}

// Skip a full RDB value by type without storing it.
static int rdb_skip_value(FILE *f, uint8_t type) {
    if (type == 0) { free(rdb_read_string(f)); return 0; }  // string
    return -1;
}



static Stream *rdb_read_stream(FILE *f) {
    Stream *stream = calloc(1, sizeof(Stream));
    if (!stream) return NULL;

    // Number of listpack entries (each listpack = one stream entry group)
    int lp_count = rdb_read_length(f, NULL);
    if (lp_count < 0) { free(stream); return NULL; }

    for (int i = 0; i < lp_count; i++) {
        // Each item is a raw listpack blob — read and parse it
        int is_enc;
        int blob_len = rdb_read_length(f, &is_enc);
        if (blob_len < 0) goto fail;

        uint8_t *blob = malloc(blob_len);
        if (!blob || fread(blob, 1, blob_len, f) != (size_t)blob_len) {
            free(blob); goto fail;
        }

        // Minimal listpack parser:
        // listpack layout: <total-bytes:4><num-elements:2><entries...><0xFF>
        uint8_t *p   = blob + 6;   // skip header
        uint8_t *end = blob + blob_len - 1;

        while (p < end && *p != 0xFF) {
            // Each entry: <encoding+data><backlen>
            uint8_t enc = *p;
            int64_t ival = 0;
            char   *sval = NULL;
            int     slen = 0;

            if ((enc & 0x80) == 0) {           // 7-bit uint
                ival = enc & 0x7F; p += 2;
            } else if ((enc & 0xE0) == 0x60) { // 13-bit int
                ival = ((enc & 0x1F) << 8) | p[1]; p += 3;
            } else if ((enc & 0xC0) == 0x80) { // 6-bit string
                slen = enc & 0x3F; sval = (char *)p + 1;
                p += 1 + slen + 1;             // enc + data + backlen
            } else if ((enc & 0xFF) == 0xF1) { // 16-bit int
                ival = (int16_t)(p[1] | (p[2] << 8)); p += 4;
            } else if ((enc & 0xFF) == 0xF2) { // 24-bit int
                ival = p[1] | (p[2] << 8) | (p[3] << 16); p += 5;
            } else if ((enc & 0xFF) == 0xF3) { // 32-bit int
                ival = (int32_t)(p[1] | (p[2]<<8) | (p[3]<<16) | (p[4]<<24));
                p += 6;
            } else if ((enc & 0xFF) == 0xF4) { // 64-bit int
                memcpy(&ival, p + 1, 8); p += 10;
            } else {
                // Unknown encoding — stop parsing this blob safely
                break;
            }

            // Build a stream entry from id + field/value pairs
            // First string entry in a listpack is the stream ID
            StreamEntry *se = calloc(1, sizeof(StreamEntry));
            if (!se) { free(blob); goto fail; }

            if (sval) {
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%.*s", slen, sval);
                se->id = strdup(tmp);
            } else {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%lld", (long long)ival);
                se->id = strdup(tmp);
            }

            se->next   = stream->head;
            stream->head = se;
            stream->length++;
            free(blob);
        }
        free(blob);
    }

    // Skip remaining stream metadata (groups, PEL, consumers) — 3 length fields
    for (int i = 0; i < 3; i++) rdb_read_length(f, NULL);
    return stream;

fail:
    // TODO: free stream entries on failure
    free(stream);
    return NULL;
}







int load_rdb_into_table(void) {
    char full_path[2048];
    snprintf(full_path, sizeof(full_path), "%s/%s",
             server_config.rdb_directory, server_config.rdb_name);

    FILE *f = fopen(full_path, "rb");
    if (!f) return -1;

    fseek(f, 9, SEEK_SET);

    long long expiry_ms = 0;  
    int loaded = 0;

    uint8_t op;
    while (fread(&op, 1, 1, f) == 1) {

        if (op == 0xFF) break;  // End of RDB

        if (op == 0xFA) { free(rdb_read_string(f)); free(rdb_read_string(f)); continue; }
        if (op == 0xFE) { rdb_read_length(f, NULL); continue; }
        if (op == 0xFB) { rdb_read_length(f, NULL); rdb_read_length(f, NULL); continue; }

        if (op == 0xFD) {
            uint32_t secs;
            fread(&secs, 4, 1, f);
            expiry_ms = (long long)secs * 1000;
            if (fread(&op, 1, 1, f) != 1) break; 
        } else if (op == 0xFC) {
            fread(&expiry_ms, 8, 1, f);
            if (fread(&op, 1, 1, f) != 1) break;
        }

        uint8_t value_type = op;
        char *key = rdb_read_string(f);
        if (!key) break;

        if (value_type == 0) { // STRING
            char *val = rdb_read_string(f);
            if (val) {
                printf("RDB LOADED: key=%s val=%s\n", key, val);
                store_set(key, val, NO_TIME_FLAG, -1, TYPE_STRING);
                if (expiry_ms != -1) {
                    Entry *entry = table[hash(key)]; 
                    if (entry) entry->expires_at = expiry_ms;
                }
                loaded++;
                free(val);
            }
        } else if (value_type == 19) {          // ── STREAM
            Stream *stream = rdb_read_stream(f);
            if (!stream) { free(key); break; }
            store_set(key, stream, NO_TIME_FLAG, -1, TYPE_STREAM);
            if (expiry_ms > 0) {
                table[hash(key)]->expires_at = expiry_ms;
            }
            loaded++;

        }
        //TODO: change to correct value type
        else if (value_type == 19) {          // ── LIST
            // List *list = rdb_read_stream(f);
            // if (!list) { free(key); break; }
            // store_set_list(key, list);
            // if (expiry_ms > 0) {
            //     table[hash(key)]->expires_at = expiry_ms;
            // }
            // loaded++;

        }
        else if (value_type == 19) {          // ── ZSET
            // ZSet *sortedSet = rdb_read_stream(f);
            // if (!sortedSet) { free(key); break; }
            // store_set_zset(key, sortedSet);
            // if (expiry_ms > 0) {
            //     table[hash(key)]->expires_at = expiry_ms;
            // }
            // loaded++;

        }  else {
            // Unknown/unsupported type skip safely by bailing out.
            fprintf(stderr, "load_rdb: unsupported type 0x%02X for key '%s', stopping\n",
                    value_type, key);
            free(key);
            break;
        }

        free(key);
        expiry_ms = -1;  // reset after each key
    }

    fclose(f);
    return loaded;  // number of keys loaded
}