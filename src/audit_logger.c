#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void){
    char fifo_path[MAX_LINE] = FIFO_PATH;
    char log_path[MAX_LINE] = AUDIT_LOG;

    char dir_buffer[256];
    snprintf(dir_buffer, sizeof(dir_buffer), "%s", fifo_path);
    char *slash = strrchr(dir_buffer, '/');
    if(slash){
        *slash = '\0';
        mkdir(dir_buffer, 0755);
    }

    if(mkfifo(fifo_path, 0666) < 0 && errno != EEXIST){
        die("mkfifo");
    }

    int fifo_fd = open(fifo_path, O_RDONLY);
    if(fifo_fd < 0){
        die("open fifo");
    }

    printf("[AUDIT] listening on %s\n", fifo_path);
    fflush(stdout);

    char line[MAX_LINE];
    int event_count = 0;
    while(1){
        int got = read_line(fifo_fd, line, sizeof(line));
        if(got <= 0){
            break;
        }
        trim_newline(line);
        if(line[0] == '\0'){
            continue;
        }

        char stamp[64];
        iso_timestamp(stamp, sizeof(stamp));

        char entry[MAX_LINE + 96];
        snprintf(entry, sizeof(entry), "%s | %s", stamp, line);
        if(append_line_locked(log_path, entry) < 0){
            fprintf(stderr, "audit logger: failed to append log\n");
        } else {
            event_count++;
            printf("[AUDIT #%d] %s\n", event_count, entry);
            fflush(stdout);
        }
    }

    close(fifo_fd);
    return 0;
}