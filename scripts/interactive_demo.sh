#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/lib.sh"

build_project

mkdir -p "$RUN_DIR"
: > "$PROJECT_ROOT/data/requests.db"
: > "$PROJECT_ROOT/data/audit.log"

echo "Starting backend workers..."
worker1=$(start_worker worker1)
worker2=$(start_worker worker2)
echo "  worker1 (PID $worker1)"
echo "  worker2 (PID $worker2)"
sleep 1

echo ""
echo "Starting audit logger..."
logger=$(start_logger)
echo "  audit logger (PID $logger)"
sleep 1

echo ""
echo "Starting load balancer server..."
server=$(start_server)
echo "  server (PID $server)"
sleep 1

wait_for_server

trap 'cleanup_stack "$server" "$logger" "$worker1" "$worker2"' EXIT

echo ""
echo "============================================"
echo "System ready. Running demo requests..."
echo "============================================"
echo ""

echo "[1] List available backends:"
client_cmd admin admin123 "LIST_BACKENDS"

echo ""
echo "[2] Submit job to worker1:"
client_cmd alice alice123 "JOB demo_job_1"

echo ""
echo "[3] Submit job to worker2:"
client_cmd alice alice123 "JOB demo_job_2"

echo ""
echo "[4] Submit concurrent jobs:"
for i in {1..10}; do
    client_cmd alice alice123 "JOB concurrent_$i" &
done
wait

echo ""
echo "============================================"
echo "Demo complete. System logs:"
echo "============================================"
echo ""

echo "SERVER LOG:"
cat "$RUN_DIR/server.log"

echo ""
echo "WORKER1 LOG:"
cat "$RUN_DIR/worker1.log"

echo ""
echo "WORKER2 LOG:"
cat "$RUN_DIR/worker2.log"

echo ""
echo "REQUEST DATABASE (data/requests.db):"
cat "$PROJECT_ROOT/data/requests.db"

echo ""
echo "Queue visibility note: run 'bash scripts/test_semaphore.sh' to see queued_jobs and waiting in the server log."

echo ""
echo "============================================"
echo ""

echo "[1] List available backends:"
client_cmd admin admin123 "LIST_BACKENDS"

echo ""
echo "[2] Submit job to worker1:"
client_cmd alice alice123 "JOB demo_job_1"

echo ""
echo "[3] Submit job to worker2:"
client_cmd alice alice123 "JOB demo_job_2"

echo ""
echo "[5] Submit concurrent jobs:"
for i in {1..10}; do
    client_cmd alice alice123 "JOB concurrent_$i" &
done
wait

echo ""
echo "============================================"
echo "Demo complete. System logs:"
echo "============================================"
echo ""

echo "SERVER LOG:"
cat "$RUN_DIR/server.log"

echo ""
echo "WORKER1 LOG:"
cat "$RUN_DIR/worker1.log"

echo ""
echo "WORKER2 LOG:"
cat "$RUN_DIR/worker2.log"

echo ""
echo "REQUEST DATABASE (data/requests.db):"
cat "$PROJECT_ROOT/data/requests.db"

echo ""
echo "Queue visibility note: run 'bash scripts/test_semaphore.sh' to see queued_jobs and waiting in the server log."

echo ""
echo "============================================"
