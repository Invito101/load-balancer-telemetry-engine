#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    char id[MAX_ID];
    int port;
    int listen_fd;
    pthread_mutex_t mutex;
    int active_requests;
} WorkerState;

static void respond_line(int fd, const char *text){
    send_all(fd, text, strlen(text));
    send_all(fd, "\nEND\n", 5);
}

static void derive_worker_id(const char *argv0, char *id, size_t len){
    const char *base = strrchr(argv0, '/');
    base = base ? base + 1 : argv0;
    snprintf(id, len, "%s", base);
}

static void *handle_client(void *arg){
    void **bundle = arg;
    WorkerState *state = bundle[0];
    int client_fd = (int)(intptr_t)bundle[1];
    free(bundle);

    pthread_mutex_lock(&state->mutex);
    state->active_requests++;
    int active_now = state->active_requests;
    pthread_mutex_unlock(&state->mutex);

    char accepted_stamp[64];
    iso_timestamp(accepted_stamp, sizeof(accepted_stamp));
    printf("[%s] %s: active_requests=%d after accept\n", accepted_stamp, state->id, active_now);
    fflush(stdout);

    char line[MAX_LINE];
    int got = read_line(client_fd, line, sizeof(line));
    if(got > 0){
        trim_newline(line);
        if(strncmp(line, "JOB ", 4) == 0){
            const char *payload = line + 4;
            char before_stamp[64];
            iso_timestamp(before_stamp, sizeof(before_stamp));
            printf("[%s] %s: processing job: %.256s (sleeping %d sec)\n", before_stamp, state->id, payload, DEFAULT_SLEEP_SECONDS);
            fflush(stdout);
            sleep(DEFAULT_SLEEP_SECONDS);
            char after_stamp[64];
            iso_timestamp(after_stamp, sizeof(after_stamp));

            char response[MAX_LINE * 2];
            snprintf(response, sizeof(response), "OK %s at %s completed %.1400s", state->id, after_stamp, payload);
            printf("[%s] %s: job complete\n", after_stamp, state->id);
            fflush(stdout);
            respond_line(client_fd, response);
        } else {
            respond_line(client_fd, "ERR worker understands only JOB");
        }
    }

    close(client_fd);
    pthread_mutex_lock(&state->mutex);
    state->active_requests--;
    int remaining = state->active_requests;
    pthread_mutex_unlock(&state->mutex);

    char release_stamp[64];
    iso_timestamp(release_stamp, sizeof(release_stamp));
    printf("[%s] %s: active_requests=%d after release\n", release_stamp, state->id, remaining);
    fflush(stdout);
    return NULL;
}

int main(int argc, char **argv){
    WorkerState state;
    memset(&state, 0, sizeof(state));
    pthread_mutex_init(&state.mutex, NULL);

    (void)argc;
    derive_worker_id(argv[0], state.id, sizeof(state.id));

    BackendRecord backend;
    if(load_backend_by_id(state.id, &backend) < 0){
        fprintf(stderr, "worker %s not found in backends.db\n", state.id);
        return EXIT_FAILURE;
    }

    state.port = backend.port;

    state.listen_fd = listen_tcp("127.0.0.1", state.port);
    if(state.listen_fd < 0){
        die("worker listen_tcp");
    }

    printf("%s listening on 127.0.0.1:%d\n", state.id, state.port);
    fflush(stdout);

    while(1){
        struct sockaddr_storage peer;
        socklen_t peer_len = sizeof(peer);
        int client_fd = accept(state.listen_fd, (struct sockaddr *)&peer, &peer_len);
        if(client_fd < 0){
            if(errno == EINTR){
                continue;
            }
            die("worker accept");
        }

        pthread_t thread;
        void **bundle = calloc(2, sizeof(void *));
        bundle[0] = &state;
        bundle[1] = (void *)(intptr_t)client_fd;
        if(pthread_create(&thread, NULL, handle_client, bundle) != 0){
            free(bundle);
            close(client_fd);
            continue;
        }
        pthread_detach(thread);
    }

    close(state.listen_fd);
    return 0;
}