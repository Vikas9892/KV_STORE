#!/usr/bin/env bash
# All output goes to the Windows-accessible results file
PROJ_WIN="/mnt/c/Users/tiwar/OneDrive/Desktop/mini distributed kv store"
OUT="$PROJ_WIN/bench_out.txt"
exec > "$OUT" 2>&1

set -euo pipefail
PROJ="$HOME/KV_STORE"
PORT=7379

log() { echo "[$(date '+%H:%M:%S')] $*"; }

log "=== STEP 0: Copy project to WSL filesystem ==="
rm -rf "$PROJ"
cp -r "$PROJ_WIN" "$PROJ"
log "Copy done."

log "=== STEP 1: Build ==="
cd "$PROJ"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ \
  -DFETCHCONTENT_QUIET=ON 2>&1 | grep -E 'CMake Error|error:|Configuring|Generating|Build files' || true
make -j"$(nproc)" 2>&1
log "Build done."

SERVER_BIN="$PROJ/build/server/kv_server"
ls -lh "$SERVER_BIN"

log "=== STEP 2: Start server ==="
cd "$PROJ"
"$SERVER_BIN" config/config.json > /tmp/kv_server.log 2>&1 &
SERVER_PID=$!
sleep 1

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
  echo "SERVER FAILED TO START:"
  cat /tmp/kv_server.log
  exit 1
fi
log "Server started PID=$SERVER_PID"

log "=== STEP 3: Throughput / latency sweep ==="
for T in 1 10 50 100 200 500; do
  log "--- $T threads ---"
  python3 "$PROJ/bench.py" "$T" || true
  sleep 1
done

log "=== STEP 4: Connection ceiling ==="
ulimit -n 65535 2>/dev/null || true
for T in 500 1000 2000 3000; do
  log "--- Ceiling: $T threads ---"
  timeout 15 python3 "$PROJ/bench.py" "$T" 2>&1 || echo "CEILING HIT at $T threads"
  sleep 2
done

log "=== STEP 5: Load 10k keys ==="
python3 - <<'PYEOF'
import socket
sock = socket.socket(); sock.connect(('127.0.0.1', 7379))
for i in range(10000):
    sock.sendall(f'SET recoverykey{i} val{i}\r\n'.encode()); sock.recv(64)
print("Loaded 10,000 keys"); sock.close()
PYEOF

log "=== STEP 6: Crash + recovery timing ==="
kill -9 "$SERVER_PID" 2>/dev/null; sleep 1
log "Server killed. Restarting..."
RESTART_NS=$(date +%s%N)
cd "$PROJ"
"$SERVER_BIN" config/config.json >> /tmp/kv_server.log 2>&1 &
SERVER_PID=$!
for i in $(seq 1 50); do
  if python3 -c "
import socket,sys; s=socket.socket(); s.settimeout(0.5)
try: s.connect(('127.0.0.1',7379)); s.sendall(b'PING\r\n'); r=s.recv(64); s.close(); sys.exit(0 if b'PONG' in r else 1)
except: sys.exit(1)" 2>/dev/null; then
    READY_NS=$(date +%s%N)
    MS=$(( (READY_NS - RESTART_NS) / 1000000 ))
    log "Recovery complete in ${MS}ms"
    break
  fi
  sleep 0.1
done

log "=== STEP 7: Data integrity ==="
python3 - <<'PYEOF'
import socket
sock = socket.socket(); sock.connect(('127.0.0.1', 7379))
passed = 0
for i in [0, 1234, 5000, 7777, 9999]:
    sock.sendall(f'GET recoverykey{i}\r\n'.encode())
    resp = sock.recv(1024).decode().strip()
    ok = resp == f'+val{i}'
    print(f"  GET recoverykey{i}: '{resp}' -> {'PASS' if ok else 'FAIL'}")
    if ok: passed += 1
sock.close(); print(f"Integrity: {passed}/5 keys verified")
PYEOF

kill "$SERVER_PID" 2>/dev/null || true
log "=== ALL DONE ==="
