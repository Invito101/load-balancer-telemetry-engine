#define _POSIX_C_SOURCE 200809L

#include "lb_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void process_command(ServerState *state, Session *session, const char *input, char *response, size_t response_len){
    char line[MAX_LINE];
    snprintf(line, sizeof(line), "%s", input);
    trim_newline(line);

    if(line[0] == '\0'){
        snprintf(response, response_len, "ERR empty command\nEND\n");
        return;
    }

    char *space = strchr(line, ' ');
    char *command = line;
    char *args = space ? space + 1 : NULL;
    if(space){
        *space = '\0';
    }

    if(strcmp(command, "LOGIN") == 0){
        char *user = args;
        char *pass = user ? strchr(user, ' ') : NULL;
        if(!user || !pass){
            snprintf(response, response_len, "ERR usage: LOGIN user pass\nEND\n");
            return;
        }
        *pass++ = '\0';
        char role[MAX_ROLE];
        int result = auth_user(user, pass, role);
        if(result == 1){
            session->authenticated = 1;
            snprintf(session->username, sizeof(session->username), "%s", user);
            snprintf(session->role, sizeof(session->role), "%s", role);
            char stamp[64];
            iso_timestamp(stamp, sizeof(stamp));
            char audit_line[MAX_LINE];
            snprintf(audit_line, sizeof(audit_line), "%s login ok for %s role=%s", stamp, user, role);
            log_audit(state, audit_line);
            snprintf(response, response_len, "OK login accepted role=%s\nEND\n", role);
        } else if(result == 0){
            snprintf(response, response_len, "ERR invalid credentials\nEND\n");
        } else {
            snprintf(response, response_len, "ERR auth database unavailable\nEND\n");
        }
        return;
    }

    if(!session->authenticated){
        snprintf(response, response_len, "ERR please LOGIN first\nEND\n");
        return;
    }

    if(!authorize_role(session->role, command)){
        snprintf(response, response_len, "ERR role %s cannot run %s\nEND\n", session->role, command);
        return;
    }

    if(strcmp(command, "LIST_BACKENDS") == 0){
        list_backends(state, response, response_len);
        return;
    }

    if(strcmp(command, "JOB") == 0){
        if(!args || args[0] == '\0'){
            snprintf(response, response_len, "ERR usage: JOB payload\nEND\n");
            return;
        }
        char job_response[MAX_LINE];
        if(forward_job(state, args, job_response, sizeof(job_response), session->username, session->role) == 0){
            snprintf(response, response_len, "%s\nEND\n", job_response);
        } else {
            snprintf(response, response_len, "%s\nEND\n", job_response);
        }
        return;
    }

    snprintf(response, response_len, "ERR unknown command\nEND\n");
}

void *handle_client(void *arg){
    ClientTask *task = arg;
    ServerState *state = task->state;
    int client_fd = task->client_fd;
    free(task);

    Session session;
    memset(&session, 0, sizeof(session));

    char input[MAX_LINE];
    while(1){
        int got = read_line(client_fd, input, sizeof(input));
        if(got <= 0){
            break;
        }

        char response[MAX_LINE * 2];
        memset(response, 0, sizeof(response));
        process_command(state, &session, input, response, sizeof(response));
        send_all(client_fd, response, strlen(response));
        if(strncmp(response, "OK server shutting down", 23) == 0){
            break;
        }
    }

    close(client_fd);
    return NULL;
}
