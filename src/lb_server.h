#ifndef LB_SERVER_H
#define LB_SERVER_H

#include "common.h"

typedef struct {
    int listen_fd;
    char host[MAX_HOST];
    int port;
    char fifo_path[256];
    int audit_fd;
    pthread_mutex_t state_mutex;
    sem_t job_slots;
    int job_slot_limit;
    int queued_jobs;
    BackendRecord backends[MAX_BACKENDS];
    int backend_count;
} ServerState;

typedef struct {
    int client_fd;
    ServerState *state;
} ClientTask;

typedef struct {
    int authenticated;
    char username[MAX_USER];
    char role[MAX_ROLE];
} Session;

void log_audit(ServerState *state, const char *message);
int load_backends(ServerState *state);
int authorize_role(const char *role, const char *command);
int auth_user(const char *username, const char *password, char *role_out);
int forward_job(ServerState *state, const char *payload, char *response, size_t response_len, const char *username, const char *role);
void list_backends(ServerState *state, char *response, size_t response_len);
void process_command(ServerState *state, Session *session, const char *input, char *response, size_t response_len);
void *handle_client(void *arg);

#endif
