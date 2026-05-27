- `data/backends.db` lists the backend workers and their ports.
# Load Balancer

## 1. Build

```bash
make clean && make
```

## 2. Start the full demo

```bash
bash scripts/interactive_demo.sh
```

- The client connects to the server over TCP on port `9000`.
- The server and client use hardcoded local endpoints, so there is no runtime config file anymore.
- The client prompts for username, password, and command instead of taking CLI arguments.
- The worker processes are started through the generated `bin/worker1` and `bin/worker2` symlinks.
- The server authenticates the user and then either lists backends for `admin` or forwards jobs for `user`.
- The server logs which worker was selected, which user submitted the job, and the role used for authorization.
- The worker logs when it accepts a request and when it finishes it.
- The request is appended to `data/requests.db` under file lock, so concurrent writes do not corrupt the database.

## 3. Why mutexes

Mutexes are used for shared memory inside processes.

In the server modules, `state_mutex` protects the shared backend array and the per-backend `active_jobs` counters. Without it, two threads could pick the same backend at the same time and update the counters inconsistently.

In `src/backend_worker.c`, the worker mutex protects `active_requests`. Without it, the counter printed in the log could be wrong because multiple connection-handler threads run at the same time.

## 4. Why semaphores

The semaphore in the server entrypoint and shared server state is `job_slots`.

It limits how many forwarded jobs can be in flight at once. This is back-pressure. If the server is already handling too many jobs, it must wait before forwarding another one.

In this project the limit is hardcoded to `2`, which makes the queueing behavior easy to show during the demo.

## 5. How to test mutex behavior

```bash
bash scripts/test_mutex.sh
```

What the script does:

- Starts the server and one worker.
- Fires several concurrent jobs to the same worker.
- Prints the worker log lines showing `active_requests` increasing and decreasing under mutex protection.

## 6. How to test semaphore behavior

```bash
bash scripts/test_semaphore.sh
```

What the script does:

- Starts the server with the hardcoded semaphore limit of `2`.
- Sends ten jobs with no explicit sleep value, so the worker uses the default 2 seconds.
- Measures total elapsed time and prints server allocation logs.
- The server log shows `queued_jobs=` and `waiting=` so you can point out the backlog.

## 7. Server modules

The server is split into smaller source files now:

- `src/lb_server.c` starts the listener and owns the process lifecycle.
- `src/lb_server_state.c` handles backend loading, persistence, selection, and queue/back-pressure logging.
- `src/lb_server_auth.c` handles roles and login validation.
- `src/lb_server_runtime.c` handles command parsing and per-client session flow.

## 8. Runtime files

- `fifo_path` is the named pipe the server writes audit events to and the audit logger reads from.
- The FIFO itself is hardcoded as `run/lb_events.fifo`.
- `data/backends.db` lists the active backend workers and their ports.
- `data/users.db` stores the demo usernames and roles.

## 9. How to test file locking and consistency

```bash
bash scripts/test_locking.sh
```

This stresses `data/requests.db` with concurrent writes. The file stays valid because `append_line_locked()` uses `flock(LOCK_EX)` before writing.

## 10. How to test role-based authorization

```bash
bash scripts/test_auth.sh
```

Use these credentials:

- `admin` / `admin123`
- `alice` / `alice123`

Expected behavior:

- `alice` can submit jobs but cannot list backends.
- `admin` can list backends but cannot submit jobs.

The only authenticated commands left are `JOB` for `user` and `LIST_BACKENDS` for `admin`.

## 11. Log Files

- `run/server.log`
- `run/worker1.log`
- `run/worker2.log`
- `data/requests.db`

## 12. OS Concepts Used

- Role-based authorization
- File locking with `flock`
- Concurrency control with mutexes and semaphores
- Data consistency in shared state and file-backed state
- Socket programming with client-server-worker TCP connections
- IPC using a named pipe for audit events

## 13. Cleanup and reset

If you want a fresh demo state:

```bash
rm -rf bin run data/audit.log data/requests.db
make
```

Keep `data/users.db` and `data/backends.db` as the seed databases.
