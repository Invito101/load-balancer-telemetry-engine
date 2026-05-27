#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/lib.sh"

build_project

mkdir -p "$RUN_DIR"
: > "$PROJECT_ROOT/data/requests.db"
: > "$PROJECT_ROOT/data/audit.log"

worker1=$(start_worker worker1)
worker2=$(start_worker worker2)
logger=$(start_logger)
server=$(start_server)

trap 'cleanup_stack "$server" "$logger" "$worker1" "$worker2"' EXIT

wait_for_server

start_ts=$(date +%s)
for i in $(seq 1 10); do
    client_cmd alice alice123 "JOB sem_test_$i" >/tmp/lb_sem_$i.out 2>&1 &
done
wait
end_ts=$(date +%s)
elapsed=$((end_ts - start_ts))

echo "elapsed_seconds=$elapsed"
echo "server allocations:"
grep "allocated" "$RUN_DIR/server.log" || true

if [[ "$elapsed" -lt 9 ]]; then
    echo "semaphore test failed: jobs completed too quickly for slots=2" >&2
    exit 1
fi

echo "semaphore test passed"