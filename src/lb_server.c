#define _POSIX_C_SOURCE 200809L

#include "lb_server.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int ensure_fifo_open(ServerState *state){
    char dir_buffer[256];
    snprintf(dir_buffer, sizeof(dir_buffer), "%s", state->fifo_path);
    char *slash = strrchr(dir_buffer, '/');
    if(slash){
        *slash = '\0';
        mkdir(dir_buffer, 0755);
    }

    if(mkfifo(state->fifo_path, 0666) < 0 && errno != EEXIST){
        return -1;
    }

    state->audit_fd = open(state->fifo_path, O_RDWR | O_NONBLOCK);
    return state->audit_fd;
}

int main(void){
    ServerState state;
    memset(&state, 0, sizeof(state));
    snprintf(state.host, sizeof(state.host), "%s", SERVER_HOST);
    state.port = SERVER_PORT;
    snprintf(state.fifo_path, sizeof(state.fifo_path), "%s", FIFO_PATH);
    state.audit_fd = -1;
    state.job_slot_limit = SERVER_JOB_SLOTS;
    state.queued_jobs = 0;

    pthread_mutex_init(&state.state_mutex, NULL);
    sem_init(&state.job_slots, 0, state.job_slot_limit);

    if(ensure_fifo_open(&state) < 0){
        die("open fifo");
    }

    if(load_backends(&state) < 0){
        die("load_backends");
    }

    state.listen_fd = listen_tcp(state.host, state.port);
    if(state.listen_fd < 0){
        die("listen_tcp");
    }

    printf("load balancer listening on %s:%d (job slots=%d)\n", state.host, state.port, state.job_slot_limit);
    fflush(stdout);

    while(1){
        struct sockaddr_storage peer;
        socklen_t peer_len = sizeof(peer);
        int client_fd = accept(state.listen_fd, (struct sockaddr *)&peer, &peer_len);
        if(client_fd < 0){
            if(errno == EINTR){
                continue;
            }
            die("accept");
        }

        ClientTask *task = malloc(sizeof(*task));
        task->client_fd = client_fd;
        task->state = &state;

        pthread_t thread;
        if(pthread_create(&thread, NULL, handle_client, task) != 0){
            close(client_fd);
            free(task);
            continue;
        }
        pthread_detach(thread);
    }

    close(state.listen_fd);
    if(state.audit_fd >= 0){
        close(state.audit_fd);
    }
    sem_destroy(&state.job_slots);
    pthread_mutex_destroy(&state.state_mutex);
    return 0;
}
