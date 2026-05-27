#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void die(const char *message){
    perror(message);
    exit(EXIT_FAILURE);
}

ssize_t send_all(int fd, const void *buf, size_t len){
    size_t sent = 0;
    const char *ptr = buf;

    while(sent < len){
        ssize_t wrote = send(fd, ptr + sent, len - sent, 0);
        if(wrote < 0){
            if(errno == EINTR){
                continue;
            }
            return -1;
        }
        sent += (size_t)wrote;
    }

    return (ssize_t)sent;
}

int read_line(int fd, char *buf, int max_len){
    int used = 0;

    while(used < max_len - 1){
        char ch;
        ssize_t got = recv(fd, &ch, 1, 0);
        if(got == 0){
            break;
        }
        if(got < 0){
            if(errno == EINTR){
                continue;
            }
            return -1;
        }
        if(ch == '\r'){
            continue;
        }
        buf[used++] = ch;
        if(ch == '\n'){
            break;
        }
    }

    buf[used] = '\0';
    return used;
}

void trim_newline(char *s){
    if(!s){
        return;
    }
    size_t len = strlen(s);
    while(len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t')){
        s[len - 1] = '\0';
        len--;
    }
}

void iso_timestamp(char *buf, size_t len){
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%S", &tm_now);
}

static int create_socket_and_connect(struct addrinfo *info){
    int fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if(fd < 0){
        return -1;
    }
    if(connect(fd, info->ai_addr, info->ai_addrlen) < 0){
        close(fd);
        return -1;
    }
    return fd;
}

int connect_tcp(const char *host, int port){
    char port_text[16];
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *current = NULL;

    snprintf(port_text, sizeof(port_text), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(host, port_text, &hints, &result) != 0){
        return -1;
    }

    int fd = -1;
    for(current = result; current != NULL; current = current->ai_next){
        fd = create_socket_and_connect(current);
        if(fd >= 0){
            break;
        }
    }

    freeaddrinfo(result);
    return fd;
}

int listen_tcp(const char *host, int port){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        return -1;
    }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if(!host || strcmp(host, "0.0.0.0") == 0){
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if(inet_pton(AF_INET, host, &addr.sin_addr) != 1){
        close(fd);
        return -1;
    }

    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        close(fd);
        return -1;
    }
    if(listen(fd, 16) < 0){
        close(fd);
        return -1;
    }

    return fd;
}

int append_line_locked(const char *path, const char *line){
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd < 0){
        return -1;
    }
    if(flock(fd, LOCK_EX) < 0){
        close(fd);
        return -1;
    }

    size_t len = strlen(line);
    if(write(fd, line, len) != (ssize_t)len || write(fd, "\n", 1) != 1){
        flock(fd, LOCK_UN);
        close(fd);
        return -1;
    }

    flock(fd, LOCK_UN);
    close(fd);
    return 0;
}

int load_users_file(UserRecord *users, int max_users){
    FILE *fp = fopen(USERS_DB, "r");
    if(!fp){
        return -1;
    }

    int fd = fileno(fp);
    if(flock(fd, LOCK_SH) < 0){
        fclose(fp);
        return -1;
    }

    int count = 0;
    char line[MAX_LINE];
    while(fgets(line, sizeof(line), fp) != NULL && count < max_users){
        trim_newline(line);
        if(line[0] == '\0'){
            continue;
        }

        char *save = NULL;
        char *user = strtok_r(line, ":", &save);
        char *pass = strtok_r(NULL, ":", &save);
        char *role = strtok_r(NULL, ":", &save);
        if(!user || !pass || !role){
            continue;
        }

        snprintf(users[count].username, sizeof(users[count].username), "%s", user);
        snprintf(users[count].password, sizeof(users[count].password), "%s", pass);
        snprintf(users[count].role, sizeof(users[count].role), "%s", role);
        count++;
    }

    flock(fd, LOCK_UN);
    fclose(fp);
    return count;
}

int load_backend_by_id(const char *id, BackendRecord *backend){
    FILE *fp = fopen(BACKENDS_DB, "r");
    if(!fp){
        return -1;
    }

    int fd = fileno(fp);
    if(flock(fd, LOCK_SH) < 0){
        fclose(fp);
        return -1;
    }

    char line[MAX_LINE];
    while(fgets(line, sizeof(line), fp) != NULL){
        trim_newline(line);
        if(line[0] == '\0'){
            continue;
        }

        char *save = NULL;
        char *line_id = strtok_r(line, ":", &save);
        char *host = strtok_r(NULL, ":", &save);
        char *port = strtok_r(NULL, ":", &save);
        if(!line_id || !host || !port){
            continue;
        }

        if(strcmp(line_id, id) == 0){
            snprintf(backend->id, sizeof(backend->id), "%s", line_id);
            snprintf(backend->host, sizeof(backend->host), "%s", host);
            backend->port = atoi(port);
            backend->active_jobs = 0;
            flock(fd, LOCK_UN);
            fclose(fp);
            return 0;
        }
    }

    flock(fd, LOCK_UN);
    fclose(fp);
    return -1;
}