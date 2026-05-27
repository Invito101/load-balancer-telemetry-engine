#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_response(int fd){
    char line[MAX_LINE];
    int saw_error = 0;
    while(1){
        int got = read_line(fd, line, sizeof(line));
        if(got <= 0){
            return -1;
        }
        trim_newline(line);
        if(strcmp(line, "END") == 0){
            break;
        }
        if(strncmp(line, "ERR", 3) == 0){
            saw_error = 1;
        }
        printf("%s\n", line);
    }
    return saw_error ? -1 : 0;
}

static int prompt_line(const char *prompt, char *buf, size_t len){
    printf("%s", prompt);
    fflush(stdout);
    if(fgets(buf, (int)len, stdin) == NULL){
        return -1;
    }
    trim_newline(buf);
    return 0;
}

int main(void){
    char host[MAX_HOST] = CLIENT_HOST;
    int port = CLIENT_PORT;
    char user[MAX_USER];
    char pass[MAX_PASS];
    char cmd[MAX_LINE];

    if(prompt_line("Username: ", user, sizeof(user)) < 0){
        return EXIT_FAILURE;
    }
    if(prompt_line("Password: ", pass, sizeof(pass)) < 0){
        return EXIT_FAILURE;
    }
    if(prompt_line("Command: ", cmd, sizeof(cmd)) < 0){
        return EXIT_FAILURE;
    }

    int fd = connect_tcp(host, port);
    if(fd < 0){
        die("connect_tcp");
    }

    char line[MAX_LINE];
    printf("[CLIENT] logging in as %s...\n", user);
    snprintf(line, sizeof(line), "LOGIN %s %s\n", user, pass);
    if(send_all(fd, line, strlen(line)) < 0 || read_response(fd) < 0){
        fprintf(stderr, "[CLIENT] login failed\n");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[CLIENT] login ok, sending command: %s\n", cmd);
    fflush(stdout);

    strncpy(line, cmd, sizeof(line) - 2);
    line[sizeof(line) - 2] = '\0';
    strcat(line, "\n");
    if(send_all(fd, line, strlen(line)) < 0 || read_response(fd) < 0){
        fprintf(stderr, "[CLIENT] command failed\n");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[CLIENT] command complete\n");
    fflush(stdout);

    close(fd);
    return EXIT_SUCCESS;
}