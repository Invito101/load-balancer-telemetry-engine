#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/lib.sh"

build_project

mkdir -p "$RUN_DIR"
: > "$PROJECT_ROOT/data/requests.db"
: > "$PROJECT_ROOT/data/audit.log"

backends_backup=$(mktemp)
cp "$PROJECT_ROOT/data/backends.db" "$backends_backup"
trap 'cp "$backends_backup" "$PROJECT_ROOT/data/backends.db"; rm -f "$backends_backup"; cleanup_stack "$server" "$logger" "$worker1"' EXIT

cat > "$PROJECT_ROOT/data/backends.db" <<'EOF'
worker1:127.0.0.1:9101
EOF

worker1=$(start_worker worker1)
logger=$(start_logger)
server=$(start_server)

wait_for_server

for i in $(seq 1 6); do
    client_cmd alice alice123 "JOB mutex_test_$i" >/tmp/lb_mutex_$i.out 2>&1 &
done
wait

echo "worker counter trace:"
grep "active_requests=" "$RUN_DIR/worker1.log" || true

if ! grep -q "active_requests=2\|active_requests=3\|active_requests=4\|active_requests=5\|active_requests=6" "$RUN_DIR/worker1.log"; then
    echo "mutex test failed: did not observe concurrent active_requests growth" >&2
    exit 1
fi

echo "mutex test passed"