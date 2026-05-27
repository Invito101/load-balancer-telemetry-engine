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

for i in $(seq 1 20); do
    client_cmd alice alice123 "JOB lock_test_$i sleep=1" >/tmp/lb_lock_$i.out 2>&1 &
done
wait

lines=$(wc -l < "$PROJECT_ROOT/data/requests.db")
if [[ "$lines" -lt 20 ]]; then
    echo "expected at least 20 locked job records, got $lines" >&2
    exit 1
fi

if grep -vE '^20[0-9]{2}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\|alice\|user\|worker[12]\|lock_test_' "$PROJECT_ROOT/data/requests.db" >/dev/null; then
    echo "lock test found malformed rows" >&2
    exit 1
fi

echo "locking test passed"