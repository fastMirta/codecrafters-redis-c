#ifndef AUTH_HANDLER_H
#define AUTH_HANDLER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include "handler.h"
#include "utils.h"

extern int default_has_nopass;
extern char default_password[65];

void handle_acl(RespRequest *req, Client *client);
void handle_auth(RespRequest *req, Client *client);


#endif