#!/usr/bin/env bash
set -euo pipefail

echo "=============================="
echo " KV Store — Build + Benchmark"
echo "=============================="

# ── Machine info ──────────────────────────────────────────────────────────────
echo ""
echo "## Machine"
echo "OS     : $(uname -srm)"
echo "Cores  : $(nproc)"
echo "RAM    : $(grep MemTotal /proc/meminfo | awk '{printf "%.0f GB", $2/1024/1024}')"
echo "CPU    : $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"

# ── Build ─────────────────────────────────────────────────────────────────────
echo ""
echo "## Build"
PROJ="$HOME/KV_STORE_BENCH"
rm -rf "$PROJ"
cp -r "/mnt/c/Users/tiwar/OneDrive/Desktop/mini distributed kv store" "$PROJ"
cd "$PROJ"
mkdir -p build && cd build
echo "Configuring..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ \
    -DFETCHCONTENT_QUIET=ON 2>&1 | grep -E "CMake Error|error:" | head -5 || true
echo "Compiling..."
make -j"$(nproc)" 2>&1 | grep -E "error:|Built target kv_server$" | head -20 || true

SERVER="$PROJ/build/server/kv_server"
if [[ ! -f "$SERVER" ]]; then
    echo "BUILD FAILED — binary not found"
    cd "$PROJ/build" && make 2>&1 | tail -30
    exit 1
fi
echo "Binary: $(ls -lh "$SERVER" | awk '{print $5, $9}')"
echo "BUILD OK"

# ── Start server ──────────────────────────────────────────────────────────────
echo ""
echo "## Server"
cd "$PROJ"
"$SERVER" config/config.json > /tmp/kv_bench.log 2>&1 &
SERVER_PID=$!
echo "Starting (PID=$SERVER_PID)..."
for i in $(seq 1 20); do
    if python3 -c "import socket,sys; s=socket.socket(); s.settimeout(0.5); s.connect(('127.0.0.1',7379)); s.sendall(b'PING\r\n'); sys.exit(0 if b'PONG' in s.recv(64) else 1)" 2>/dev/null; then
        echo "Server UP"
        break
    fi
    sleep 0.2
done

# ── Throughput + latency sweep ────────────────────────────────────────────────
echo ""
echo "## Throughput & Latency"
for T in 1 10 50 100 200 500 1000; do
    echo "--- $T threads ---"
    python3 "$PROJ/bench.py" "$T" 2>&1 || echo "  (failed at $T threads)"
    sleep 1
done

# ── Load data for recovery test ───────────────────────────────────────────────
echo ""
echo "## Loading 10,000 keys..."
python3 - <<'PYEOF'
import socket
s = socket.socket(); s.connect(('127.0.0.1', 7379))
for i in range(10000):
    s.sendall(f'SET rk{i} v{i}\r\n'.encode()); s.recv(64)
s.close(); print("10,000 keys written")
PYEOF

# ── Crash + recovery ──────────────────────────────────────────────────────────
echo ""
echo "## Crash Recovery"
kill -9 "$SERVER_PID" 2>/dev/null; sleep 1
echo "Server killed. Restarting..."
T_START=$(($(date +%s%N)/1000000))
"$SERVER" config/config.json >> /tmp/kv_bench.log 2>&1 &
SERVER_PID=$!

READY=0
for i in $(seq 1 100); do
    if python3 -c "import socket,sys; s=socket.socket(); s.settimeout(0.3); s.connect(('127.0.0.1',7379)); s.sendall(b'PING\r\n'); sys.exit(0 if b'PONG' in s.recv(64) else 1)" 2>/dev/null; then
        T_END=$(($(date +%s%N)/1000000))
        echo "Recovery time: $((T_END - T_START)) ms  (10,000 keys from WAL)"
        READY=1; break
    fi
    sleep 0.1
done
[[ $READY -eq 0 ]] && echo "Recovery timed out"

# ── Data integrity ────────────────────────────────────────────────────────────
echo ""
echo "## Data Integrity Post-Recovery"
python3 - <<'PYEOF'
import socket
s = socket.socket(); s.connect(('127.0.0.1', 7379))
passed = 0
checks = [0, 1234, 5000, 7777, 9999]
for i in checks:
    s.sendall(f'GET rk{i}\r\n'.encode())
    r = s.recv(1024).decode().strip()
    ok = (r == f'+v{i}')
    print(f"  rk{i:5d}: got '{r}'  -> {'PASS' if ok else 'FAIL'}")
    if ok: passed += 1
s.close()
print(f"Result: {passed}/{len(checks)} keys verified")
PYEOF

# ── Cleanup ───────────────────────────────────────────────────────────────────
kill "$SERVER_PID" 2>/dev/null || true

echo ""
echo "=============================="
echo " ALL DONE"
echo "=============================="
