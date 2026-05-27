#define _POSIX_C_SOURCE 200809L

#include "lb_server.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int load_backends(ServerState *state){
    FILE *fp = fopen(BACKENDS_DB, "r");
    if(!fp){
        return -1;
    }

    int fd = fileno(fp);
    if(flock(fd, LOCK_SH) < 0){
        fclose(fp);
        return -1;
    }

    state->backend_count = 0;
    char line[MAX_LINE];
    while(fgets(line, sizeof(line), fp) != NULL && state->backend_count < MAX_BACKENDS){
        trim_newline(line);
        if(line[0] == '\0'){
            continue;
        }

        char *save = NULL;
        char *id = strtok_r(line, ":", &save);
        char *host = strtok_r(NULL, ":", &save);
        char *port = strtok_r(NULL, ":", &save);
        if(!id || !host || !port){
            continue;
        }

        BackendRecord *backend = &state->backends[state->backend_count++];
        snprintf(backend->id, sizeof(backend->id), "%s", id);
        snprintf(backend->host, sizeof(backend->host), "%s", host);
        backend->port = atoi(port);
        backend->active_jobs = 0;
    }

    flock(fd, LOCK_UN);
    fclose(fp);
    return state->backend_count;
}

static void release_backend(ServerState *state, int index){
    if(index >= 0 && index < state->backend_count && state->backends[index].active_jobs > 0){
        state->backends[index].active_jobs--;
    }
}

static int select_backend(ServerState *state){
    int chosen = -1;
    for(int i = 0; i < state->backend_count; i++){
        if(chosen < 0 || state->backends[i].active_jobs < state->backends[chosen].active_jobs){
            chosen = i;
        }
    }
    if(chosen >= 0){
        state->backends[chosen].active_jobs++;
    }
    return chosen;
}

int forward_job(ServerState *state, const char *payload, char *response, size_t response_len, const char *username, const char *role){
    pthread_mutex_lock(&state->state_mutex);
    state->queued_jobs++;
    int queued_before = state->queued_jobs;
    pthread_mutex_unlock(&state->state_mutex);

    char queue_stamp[64];
    iso_timestamp(queue_stamp, sizeof(queue_stamp));
    printf("[%s] queued request user=%s role=%s payload=%.256s queued_jobs=%d\n", queue_stamp, username, role, payload, queued_before);
    fflush(stdout);

    if(sem_wait(&state->job_slots) < 0){
        pthread_mutex_lock(&state->state_mutex);
        state->queued_jobs--;
        pthread_mutex_unlock(&state->state_mutex);
        snprintf(response, response_len, "ERR failed to enter job slot");
        return -1;
    }

    pthread_mutex_lock(&state->state_mutex);
    state->queued_jobs--;
    int queued_after = state->queued_jobs;
    int backend_index = select_backend(state);
    pthread_mutex_unlock(&state->state_mutex);

    if(backend_index < 0){
        sem_post(&state->job_slots);
        snprintf(response, response_len, "ERR no active backend available");
        return -1;
    }

    BackendRecord backend = state->backends[backend_index];
    char alloc_stamp[64];
    iso_timestamp(alloc_stamp, sizeof(alloc_stamp));
    printf("[%s] allocated %s to user=%s role=%s job=%.256s waiting=%d\n", alloc_stamp, backend.id, username, role, payload, queued_after);
    fflush(stdout);

    int worker_fd = connect_tcp(backend.host, backend.port);
    if(worker_fd < 0){
        pthread_mutex_lock(&state->state_mutex);
        release_backend(state, backend_index);
        pthread_mutex_unlock(&state->state_mutex);
        sem_post(&state->job_slots);
        snprintf(response, response_len, "ERR backend %s unreachable", backend.id);
        return -1;
    }

    char request[MAX_LINE];
    snprintf(request, sizeof(request), "JOB %s\n", payload);
    if(send_all(worker_fd, request, strlen(request)) < 0){
        close(worker_fd);
        pthread_mutex_lock(&state->state_mutex);
        release_backend(state, backend_index);
        pthread_mutex_unlock(&state->state_mutex);
        sem_post(&state->job_slots);
        snprintf(response, response_len, "ERR failed to send job to %s", backend.id);
        return -1;
    }

    char line[MAX_LINE];
    if(read_line(worker_fd, line, sizeof(line)) <= 0){
        close(worker_fd);
        pthread_mutex_lock(&state->state_mutex);
        release_backend(state, backend_index);
        pthread_mutex_unlock(&state->state_mutex);
        sem_post(&state->job_slots);
        snprintf(response, response_len, "ERR backend %s closed connection", backend.id);
        return -1;
    }
    trim_newline(line);
    close(worker_fd);

    char stamp[64];
    iso_timestamp(stamp, sizeof(stamp));
    char job_line[MAX_LINE];
    snprintf(job_line, sizeof(job_line), "%s|%s|%s|%s|%s", stamp, username, role, backend.id, payload);
    append_line_locked(REQUESTS_DB, job_line);

    pthread_mutex_lock(&state->state_mutex);
    release_backend(state, backend_index);
    pthread_mutex_unlock(&state->state_mutex);
    sem_post(&state->job_slots);

    snprintf(response, response_len, "%s", line);
    char audit_line[MAX_LINE];
    snprintf(audit_line, sizeof(audit_line), "%s request by %s used %s", stamp, username, backend.id);
    log_audit(state, audit_line);
    return 0;
}

void list_backends(ServerState *state, char *response, size_t response_len){
    size_t used = 0;
    used += snprintf(response + used, response_len - used, "BACKENDS\n");

    pthread_mutex_lock(&state->state_mutex);
    for(int i = 0; i < state->backend_count && used < response_len; i++){
        used += snprintf(response + used, response_len - used, "%s %s:%d load=%d\n",
                         state->backends[i].id,
                         state->backends[i].host,
                         state->backends[i].port,
                         state->backends[i].active_jobs);
    }
    used += snprintf(response + used, response_len - used, "waiting=%d\n", state->queued_jobs);
    pthread_mutex_unlock(&state->state_mutex);
    snprintf(response + used, response_len - used, "END\n");
}

void log_audit(ServerState *state, const char *message){
    if(state->audit_fd < 0){
        return;
    }
    char line[MAX_LINE];
    snprintf(line, sizeof(line), "%s", message);
    if(send_all(state->audit_fd, line, strlen(line)) >= 0){
        send_all(state->audit_fd, "\n", 1);
    }
}
