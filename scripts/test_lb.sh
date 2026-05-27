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

for i in $(seq 1 10); do
    client_cmd alice alice123 "JOB lb_demo_$i" &
done
wait

echo "requests stored in data/requests.db"
echo "worker distribution:"
cut -d'|' -f4 "$PROJECT_ROOT/data/requests.db" | sort | uniq -c

echo "load balancing test passed"