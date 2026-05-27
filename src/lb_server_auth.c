#define _POSIX_C_SOURCE 200809L

#include "lb_server.h"

#include <stdio.h>
#include <string.h>

int authorize_role(const char *role, const char *command){
    if(strcmp(role, "admin") == 0){
        return strcmp(command, "LIST_BACKENDS") == 0;
    }
    if(strcmp(role, "user") == 0){
        return strcmp(command, "JOB") == 0;
    }
    return 0;
}

int auth_user(const char *username, const char *password, char *role_out){
    UserRecord users[32];
    int count = load_users_file(users, 32);
    if(count < 0){
        return -1;
    }

    for(int i = 0; i < count; i++){
        if(strcmp(users[i].username, username) == 0 && strcmp(users[i].password, password) == 0){
            snprintf(role_out, MAX_ROLE, "%s", users[i].role);
            return 1;
        }
    }
    return 0;
}
