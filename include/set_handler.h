#ifndef SET_HANDLER_H
#define SET_HANDLER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "handler.h"
#include "utils.h"

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#ifndef EARTH_RAD
    #define EARTH_RAD 6372797.560856 
#endif

void handle_zadd(RespRequest *req, int client_fd);
void handle_zrank(RespRequest *req, int client_fd);
void handle_zrange(RespRequest *req, int client_fd);
void handle_zcard(RespRequest *req, int client_fd);
void handle_zscore(RespRequest *req, int client_fd);
void handle_zrem(RespRequest *req, int client_fd);
void handle_geoadd(RespRequest *req, int client_fd);
void handle_geopos(RespRequest *req, int client_fd);
void handle_geodist(RespRequest *req, int client_fd);
void handle_geosearch(RespRequest *req, int client_fd);

#endif