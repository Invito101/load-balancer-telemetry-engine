#ifndef COMMON_H
#define COMMON_H

#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stddef.h>

#define MAX_LINE 1024
#define MAX_BACKENDS 32
#define MAX_ID 32
#define MAX_HOST 64
#define MAX_USER 32
#define MAX_PASS 64
#define MAX_ROLE 16
#define MAX_PAYLOAD 768

#define USERS_DB "data/users.db"
#define BACKENDS_DB "data/backends.db"
#define REQUESTS_DB "data/requests.db"
#define AUDIT_LOG "data/audit.log"
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 9000
#define CLIENT_HOST "127.0.0.1"
#define CLIENT_PORT 9000
#define DEFAULT_SLEEP_SECONDS 2
#define FIFO_PATH "run/lb_events.fifo"
#define SERVER_JOB_SLOTS 2

typedef struct {
    char id[MAX_ID];
    char host[MAX_HOST];
    int port;
    int active_jobs;
} BackendRecord;

typedef struct {
    char username[MAX_USER];
    char password[MAX_PASS];
    char role[MAX_ROLE];
} UserRecord;

void die(const char *message);
ssize_t send_all(int fd, const void *buf, size_t len);
int read_line(int fd, char *buf, int max_len);
void trim_newline(char *s);
void iso_timestamp(char *buf, size_t len);
int connect_tcp(const char *host, int port);
int listen_tcp(const char *host, int port);
int append_line_locked(const char *path, const char *line);
int load_users_file(UserRecord *users, int max_users);
int load_backend_by_id(const char *id, BackendRecord *backend);

#endif