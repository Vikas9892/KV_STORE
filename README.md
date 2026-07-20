# High-Performance Concurrent Key-Value Store

A production-grade, in-memory key-value store built from scratch in **C++20**. Covers the full systems stack: TCP networking, custom binary protocol, readers-writer locking, thread pool, write-ahead log, binary snapshots, epoll-based async I/O, TTL expiry, lock-free metrics, and leader→replica replication.

Designed across 20 phases, each adding one layer of production quality — from a bare TCP echo server to a replicated, persistent store. The phase roadmap below traces the full design journey.

---

## Table of Contents

- [Architecture](#architecture)
- [Design Decisions](#design-decisions)
- [Components](#components)
- [Protocol](#protocol)
- [Persistence Model](#persistence-model)
- [Concurrency Model](#concurrency-model)
- [Replication](#replication)
- [Build](#build)
- [Run](#run)
- [Commands](#commands)
- [Configuration](#configuration)
- [Benchmarks](#benchmarks)
- [Tests](#tests)
- [Phase Roadmap](#phase-roadmap)
---

## Architecture

```
                          ┌─────────────────────────────────────────┐
                          │              TcpServer                   │
  Client ──TCP──►  listen │                                          │
                  socket  │  ┌──────────┐     ┌────────────────┐    │
                          │  │ThreadPool│────►│ ClientSession  │    │
                          │  │(N workers│     │ parse + dispatch│    │
                          │  └──────────┘     └───────┬────────┘    │
                          │                           │              │
                          │                    ┌──────▼──────┐      │
                          │                    │   KVStore   │      │
                          │                    │shared_mutex │      │
                          │                    │ + TTL reaper│      │
                          │                    └──────┬──────┘      │
                          │                           │              │
                          │              ┌────────────┼───────────┐  │
                          │              │            │           │  │
                          │           WAL file   Snapshot     Replicator
                          │           (kv.log)   (kv.snap)   ──TCP──► replica
                          └─────────────────────────────────────────┘

  EpollServer (kv_server_epoll) — alternative entry point:
  single event loop, edge-triggered epoll, non-blocking accept4 + recv,
  per-fd read buffers; dispatches into same ThreadPool.
```

---

## Design Decisions

### Why `std::shared_mutex` instead of `std::mutex`?

A key-value store is read-heavy in production (cache workloads skew 80–90% reads). `shared_mutex` lets N threads execute `GET`/`EXISTS`/`SIZE` concurrently. `SET`/`DELETE`/`CLEAR` take an exclusive write lock. This eliminates false serialization on the read path entirely.

### Why a thread pool instead of thread-per-connection?

Each OS thread consumes ~8 MB of stack. At 10 k concurrent connections that is 80 GB — not feasible. A fixed pool of N workers (default: `hardware_concurrency`) handles all connections through a shared task queue. The pool bounds memory and avoids the cost of spawning/destroying threads per request.

### Why Write-Ahead Log + Snapshot instead of just a log?

Replaying a million-entry WAL on every restart is O(n) in total history — unbounded. Instead: snapshot the full dataset periodically (binary, O(n) once), then truncate the WAL. On restart, load the snapshot (fast), then replay only the delta WAL entries since the last snapshot. Recovery time is bounded by the snapshot interval, not total history.

### Why `epoll` with edge-triggered (EPOLLET)?

Level-triggered epoll fires on every `select` iteration if data is available — fine, but wastes syscalls during bursts. Edge-triggered fires once per transition from no-data to data. The catch: you must drain the socket until `EAGAIN` on every event, or you will miss data. `EpollServer` does exactly that — it loops `recv` until `EAGAIN` with per-fd read buffers for partial lines.

### Why self-pipe for signal handling?

`select()`/`epoll_wait()` block the thread. A naive `volatile sig_atomic_t` flag only gets checked on the next loop iteration — but if the server is idle, it never wakes. The self-pipe trick: `SIGINT`/`SIGTERM` handlers write one byte to a pipe, and the server adds the pipe's read-end to `select`/`epoll`. The signal unblocks the wait immediately.

### Why CRC32 on snapshots?

A partially-written snapshot (crash mid-write) would corrupt all data on next load. CRC32 (Ethernet/ZIP polynomial `0xEDB88320`) is computed over the entire payload before writing. The load path verifies CRC first — a corrupt file is silently ignored rather than crashing the server. Snapshot writes are also atomic: write to `path.tmp`, then `rename()` — rename is atomic on POSIX filesystems.

---

## Components

### `core/kv_store` — Storage Engine
- `std::unordered_map<string, string>` — O(1) average get/set/del
- `std::unordered_map<string, TimePoint>` — parallel expiry map for TTL keys
- `std::shared_mutex` — concurrent reads, exclusive writes
- Background reaper thread wakes every second to bulk-erase expired keys (active expiry)
- Lazy expiry on `GET`/`EXISTS` — key is checked and removed inline (passive expiry)
- On construction: loads snapshot → replays WAL → starts reaper thread

### `core/wal` — Write-Ahead Log
- Append-only, newline-delimited text log: `SET key value\n`, `DEL key\n`, `CLEAR\n`
- Every write is `fflush()`'d immediately — no buffered data loss on crash
- `replay(callback)` — called on startup to reconstruct state
- `truncate()` — called after each snapshot to reclaim disk space

### `core/snapshot` — Binary Point-in-Time Snapshot
- Format: `"KVSS"` magic + `uint16` version + `uint64` timestamp + `uint64` count + entries + `uint32` CRC32
- Atomic write: `path.tmp` then `rename()` — no partial-write corruption ever visible
- CRC verified on load before any data is applied

### `core/snapshot_manager` — Background Snapshotting
- Daemon thread wakes every N seconds, calls `get_all()` under shared lock, writes snapshot, truncates WAL
- Also triggered on clean shutdown via `trigger()` to capture final state

### `network/socket` — RAII TCP Socket
- Wraps raw file descriptor; moveable, not copyable
- `Socket::listen_on(port)` — `SO_REUSEADDR`, `bind`, `listen`
- `accept_client()` — returns new `Socket` + peer address string
- `send_all()` — loops `send()` with `MSG_NOSIGNAL` until all bytes sent
- `recv_line()` — buffers partial TCP reads internally, returns one `\r\n`-terminated line

### `network/protocol` — Command Parser
- Zero-copy `string_view` tokenizer — no heap allocation during parse
- Case-insensitive command matching via `sv_ieq()`
- `Response::serialize()` — `+value\r\n` (VALUE), `:42\r\n` (INTEGER), `-ERR msg\r\n` (ERROR), `+OK\r\n` (OK)

### `server/server` — Blocking TCP Server
- `select()` multiplexes listener fd and signal pipe fd
- Accepts connection → enqueues `ClientSession::handle` lambda to thread pool
- Graceful shutdown: drains pool, triggers final snapshot

### `server/epoll_server` — Non-blocking epoll Server
- `epoll_create1(EPOLL_CLOEXEC)` + `EPOLLIN | EPOLLET` on all fds
- `accept4()` with `SOCK_NONBLOCK | SOCK_CLOEXEC` — no extra `fcntl()` call
- Drains all pending connections and all data per event (edge-triggered requirement)
- Signal pipe registered in epoll — unified event loop, no special-case polling

### `server/client_session` — Per-Connection Handler
- Reads lines via `recv_line()`, parses with `CommandParser`, dispatches, serializes response
- Updates `Metrics` atomically on every command (bytes, counters, connected clients)
- Forwards writes to `Replicator` when replication is enabled

### `server/signal_handler` — Signal-Safe Shutdown
- `sigaction(SIGINT/SIGTERM)` → writes 1 byte to self-pipe (async-signal-safe)
- `pipe_read_fd()` exposed for `select()`/`epoll` integration

### `server/replicator` — Async Write Forwarding
- Background thread maintains a persistent TCP connection to the replica
- `forward_set/setex/del/clear()` enqueues a command string — non-blocking, O(1) for caller
- On send failure: re-queues all pending commands in order, reconnects after 5 s — no writes dropped
- Heartbeat `PING` every 10 s when idle — detects dead TCP connections before the next real write

### `threadpool/thread_pool` — Fixed Worker Pool
- N worker threads block on `condition_variable`; `enqueue()` pushes task + `notify_one()`
- Destructor sets `m_stop = true`, `notify_all()`, joins all workers
- Workers drain the full queue before exiting — no task dropped on shutdown

### `client/connection_pool` — Client-Side Connection Pooling
- Pool of M persistent TCP connections; RAII `Lease` auto-returns connection on destruction
- `acquire()` blocks on `condition_variable` until a free slot is available
- Used by `kv_netbench` to drive concurrent network load with sustained connections

### `config/config` — JSON Configuration
- Parsed with `nlohmann/json`; all fields guarded with `j.contains()` — missing keys fall back to defaults
- `write_default()` emits a starter `config.json` if none exists

### `utils/logger` — Thread-Safe Structured Logging
- Singleton; `DEBUG/INFO/WARN/ERROR` levels
- Mutex-protected `fprintf` to stderr; `fflush()` after every line — safe under concurrent writes

### `utils/metrics` — Lock-Free Telemetry
- All counters are `std::atomic<uint64_t>` with `memory_order_relaxed` — zero contention, no locking
- `METRICS_INC(counter)` / `METRICS_ADD(counter, n)` macros for ergonomic updates anywhere
- `STATS` command returns a live snapshot: uptime, connected clients, per-command counts, bytes in/out

---

## Protocol

Line-delimited text over TCP (`\r\n` terminated). Simple enough to test with `nc` or `telnet`.

```
Request:   COMMAND [key] [arg...]\r\n

Response:  +OK\r\n              ← success, no payload
           +<value>\r\n         ← string result
           :<integer>\r\n       ← integer result
           -ERR <message>\r\n   ← error
```

All commands are case-insensitive. `set`, `SET`, and `Set` are identical.

---

## Commands

| Command | Syntax | Response | Notes |
|---------|--------|----------|-------|
| `PING` | `PING` | `+PONG` | Health check |
| `SET` | `SET key value` | `+OK` | Upsert; clears any existing TTL |
| `GET` | `GET key` | `+value` or `-ERR NOT_FOUND` | Lazy TTL expiry on hit |
| `SETEX` | `SETEX key seconds value` | `+OK` | Set with TTL |
| `TTL` | `TTL key` | `:<seconds>` | `-1` = no TTL, `-2` = missing |
| `DELETE` | `DELETE key` | `+OK` or `-ERR NOT_FOUND` | |
| `EXISTS` | `EXISTS key` | `:1` or `:0` | |
| `SIZE` | `SIZE` | `:<n>` | Count of live (non-expired) keys |
| `CLEAR` | `CLEAR` | `+OK` | Removes all keys |
| `STATS` | `STATS` | `+<metrics>` | Live server telemetry |

---

## Persistence Model

```
Startup sequence:
  1. Load snapshot (kv.snap) — fast binary restore, O(n) in entry count
  2. Replay WAL  (kv.log)   — apply only writes since last snapshot

Runtime:
  Every write → append to WAL (fflush'd immediately)
  Every N seconds → snapshot full dataset → truncate WAL

Crash recovery:
  snapshot + WAL delta = complete, consistent state
  Corrupt snapshot (bad CRC) → ignored; WAL replayed from scratch
```

TTL metadata is **not** persisted. A `SETEX` key survives a restart as a non-expiring key. This is a deliberate simplification; production systems persist TTL deadlines separately.

---

## Concurrency Model

```
┌─────────────────────────────────────────────────┐
│  Thread Pool  (N = hardware_concurrency)         │
│                                                 │
│  worker-0  worker-1  worker-2  ...  worker-N-1  │
│     │          │         │                      │
│     └──────────┴─────────┘                      │
│                   │                             │
│            shared_mutex (KVStore)               │
│            ┌──────────────────┐                 │
│            │  shared_lock     │ ← concurrent GETs│
│            │  unique_lock     │ ← serialized SETs│
│            └──────────────────┘                 │
└─────────────────────────────────────────────────┘

Background threads (independent of pool):
  SnapshotManager thread  — periodic snapshot + WAL truncate
  KVStore reaper thread   — bulk TTL expiry every 1 s
  Replicator thread       — drains write-forward queue to replica
```

---

## Replication

Leader–replica model. The replica is a plain `kv_server` — no special configuration on its side.

```
Leader                              Replica
  │   SET foo bar                    │
  ├──► forward_set("foo", "bar")     │
  │         │                        │
  │   [Replicator queue]             │
  │         │                        │
  │   background thread ──TCP──────► │ SET foo bar\r\n
  │                                  │ +OK\r\n  (discarded)
```

Enable in `config.json`:
```json
{
  "replica_host": "192.168.1.10",
  "replica_port": 7380
}
```

**Failure handling:** if the replica goes down, the failed command and all queued commands are re-queued in order. The replicator reconnects every 5 seconds. No write is silently dropped.

**Heartbeat:** `PING` is sent every 10 seconds when idle so dead TCP connections are detected before the next real write hits them.

---

## Build

Requires: GCC 11+ or Clang 13+, CMake 3.20+, Linux or WSL2.

```bash
git clone https://github.com/Vikas9892/KV_STORE.git
cd KV_STORE

# Release build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Debug build (AddressSanitizer + UBSanitizer enabled)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

Binaries produced:

| Binary | Description |
|--------|-------------|
| `build/server/kv_server` | Blocking TCP server (select-based) |
| `build/server/kv_server_epoll` | High-concurrency epoll server |
| `build/benchmark/kv_bench` | In-process KVStore benchmark |
| `build/benchmark/kv_netbench` | Network throughput + latency benchmark |
| `build/tests/kv_tests` | GoogleTest suite (28 tests) |

---

## Run

**Start the server:**
```bash
./build/server/kv_server               # port 7379, reads config.json if present
./build/server/kv_server my_config.json
```

**Connect manually:**
```bash
nc 127.0.0.1 7379
SET name Vikas
GET name
SETEX token abc123 3600
TTL token
STATS
```

**Start a replica:**
```bash
# Terminal 2 — replica listens on 7380
./build/server/kv_server replica.json

# Terminal 1 — leader config.json has replica_host + replica_port set
./build/server/kv_server config.json
```

---

## Configuration

`config.json` — all fields are optional and fall back to the defaults shown:

```json
{
    "port":                7379,
    "workers":             0,
    "max_clients":         1024,
    "log_level":           "INFO",
    "wal_path":            "kv.log",
    "snapshot_path":       "kv.snap",
    "snapshot_interval_s": 300,
    "kv_initial_capacity": 0,
    "replica_host":        "",
    "replica_port":        7380
}
```

| Field | Effect |
|-------|--------|
| `workers` | Thread pool size; `0` = `std::thread::hardware_concurrency()` |
| `snapshot_interval_s` | `0` disables background snapshotting |
| `kv_initial_capacity` | Pre-allocates hash table buckets to reduce rehash on warmup |
| `replica_host` | Empty string disables replication entirely |
| `log_level` | `DEBUG` / `INFO` / `WARN` / `ERROR` |

---

## Benchmarks

Measured on WSL2 Ubuntu — AMD Ryzen 5 7520U (8 cores, 7 GB RAM).  
Protocol: raw TCP `SET key value\r\n`, 10-second sustained load. Full table in [`BENCHMARKS.md`](BENCHMARKS.md).

### Throughput vs Concurrent Connections

| Connections | Throughput | p50 latency | p99 latency |
|------------:|----------:|:-----------:|:-----------:|
| 1 | **9,619 ops/sec** | 0.087 ms | 0.213 ms |
| 10 | 5,496 ops/sec | 1.337 ms | 3.580 ms |
| 100 | 5,108 ops/sec | 1.441 ms | 3.882 ms |
| 500 | 4,885 ops/sec | 1.472 ms | 4.516 ms |
| **1,000** | **4,831 ops/sec** | 1.487 ms | 4.624 ms |

Throughput drops only **~16 %** from 10 → 1,000 concurrent connections — the `shared_mutex` readers-writer lock keeps the read path contention-free.

### Crash Recovery

| Dataset | Kill method | Recovery time |
|---------|-------------|:-------------:|
| 10,000 keys | `kill -9` (hard crash) | **410 ms** |

WAL is `fflush()`'d after every write — zero committed keys lost, 5/5 spot-checked keys verified correct after replay.

```bash
# Run the benchmark yourself
bash go.sh 2>&1 | tee bench_out.txt
```

---

## Tests

```bash
cd build && ctest --output-on-failure -V
```

| Suite | File | Tests |
|-------|------|-------|
| KVStore core | `tests/test_kv_store.cpp` | 11 — basic ops, overwrite, TTL, concurrent reads/writes |
| Protocol | `tests/test_protocol.cpp` | 13 — all commands, case-insensitivity, serialization |
| Thread pool | `tests/test_thread_pool.cpp` | 4 — task execution, thread count, concurrent counter, shutdown |

GoogleTest is fetched automatically at configure time via `FetchContent` — no manual install needed.

---

## Phase Roadmap

Full design journey across 20 phases, each introducing one system capability:

| Phase | What it adds |
|-------|-------------|
| 1 | TCP server — `socket / bind / listen / accept / recv / send` |
| 2 | KVStore — `unordered_map`, GET / SET / DELETE / EXISTS / SIZE / CLEAR |
| 3 | Protocol — `enum class Command`, `CommandParser`, `Response::serialize()` |
| 4 | OOP design — `TcpServer`, `ClientSession`, `Socket` RAII wrapper |
| 5 | Multithreading — `std::thread` per connection |
| 6 | Thread safety — `std::shared_mutex` readers-writer lock |
| 7 | Thread pool — fixed worker pool, producer-consumer with `condition_variable` |
| 8 | Structured logging — `Logger` singleton, `LOG_*` macros, levels, stderr flush |
| 9 | Configuration — `nlohmann/json`, `config.json`, `Config::from_file()` |
| 10 | Write-Ahead Log — append-only log, `replay()`, `truncate()` |
| 11 | Snapshotting — binary format v1, CRC32, atomic rename, `SnapshotManager` |
| 12 | Benchmarks — in-process + network throughput, ops/sec, µs/op |
| 13 | Unit tests — GoogleTest via FetchContent, 28 tests across 3 suites |
| 14 | Graceful shutdown — self-pipe signal handling, final snapshot on SIGINT |
| 15 | epoll server — edge-triggered, `accept4`, non-blocking I/O, `kv_server_epoll` |
| 16 | Memory optimization — `string_view` parser, `reserve()`, `rehash(0)` on CLEAR |
| 17 | Metrics — lock-free `std::atomic` counters, `STATS` command, uptime |
| 18 | Connection pool — client-side RAII `Lease`, `condition_variable`, `kv_netbench` |
| 19 | TTL — `SETEX` / `TTL`, lazy expiry on GET, background reaper thread |
| 20 | Replication — leader→replica TCP forwarding, heartbeat, re-queue on failure |

---

## Author

Built by [Vikas Tiwari](https://github.com/Vikas9892).

