#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$PROJECT_ROOT/bin"
RUN_DIR="$PROJECT_ROOT/run"

build_project() {
    mkdir -p "$RUN_DIR"
    make -C "$PROJECT_ROOT" >/dev/null
    while IFS=: read -r id host port; do
        if [[ -n "$id" ]]; then
            ln -sf backend_worker "$BIN_DIR/$id"
        fi
    done < "$PROJECT_ROOT/data/backends.db"
}

start_worker() {
    local id="$1"
    "$BIN_DIR/$id" >"$RUN_DIR/$id.log" 2>&1 &
    echo $!
}

start_logger() {
    "$BIN_DIR/audit_logger" >"$RUN_DIR/audit.log" 2>&1 &
    echo $!
}

start_server() {
    "$BIN_DIR/lb_server" >"$RUN_DIR/server.log" 2>&1 &
    echo $!
}

client_cmd() {
    local user="$1"
    local pass="$2"
    local cmd="$3"
    printf '%s\n%s\n%s\n' "$user" "$pass" "$cmd" | "$BIN_DIR/lb_client"
}

wait_for_server() {
    local tries=0
    until client_cmd admin admin123 LIST_BACKENDS >/dev/null 2>&1; do
        tries=$((tries + 1))
        if [[ "$tries" -gt 40 ]]; then
            echo "server did not start" >&2
            exit 1
        fi
        sleep 0.25
    done
}

cleanup_stack() {
    local pids=("$@")
    for pid in "${pids[@]}"; do
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
        fi
    done
    wait || true
}