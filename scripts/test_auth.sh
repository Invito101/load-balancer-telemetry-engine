#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/lib.sh"

build_project

mkdir -p "$RUN_DIR"
: > "$PROJECT_ROOT/data/requests.db"
: > "$PROJECT_ROOT/data/audit.log"

worker1=$(start_worker worker1 9101)
worker2=$(start_worker worker2 9102)
logger=$(start_logger)
server=$(start_server)

trap 'cleanup_stack "$server" "$logger" "$worker1" "$worker2"' EXIT

wait_for_server

client_cmd alice alice123 "JOB auth_job"

if client_cmd alice alice123 "LIST_BACKENDS" >/tmp/lb_auth_user.out 2>&1; then
    echo "user list-backends unexpectedly succeeded" >&2
    exit 1
fi

client_cmd admin admin123 "LIST_BACKENDS"

if client_cmd admin admin123 "JOB should_fail" >/tmp/lb_auth_admin.out 2>&1; then
    echo "admin job unexpectedly succeeded" >&2
    exit 1
fi

echo "auth test passed"