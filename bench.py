import socket
import threading
import time
import statistics
import sys

HOST = "127.0.0.1"
PORT = 7379
DURATION_SEC = 10
NUM_THREADS = int(sys.argv[1]) if len(sys.argv) > 1 else 50

latencies = []
lat_lock = threading.Lock()
op_count = [0]
stop_flag = threading.Event()
errors = [0]

def worker(thread_id):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect((HOST, PORT))
    except Exception as e:
        with lat_lock:
            errors[0] += 1
        return

    local_ops = 0
    local_lat = []
    i = 0
    while not stop_flag.is_set():
        key = f"k{thread_id}_{i}"
        val = "benchval"
        cmd = f"SET {key} {val}\r\n".encode()
        try:
            start = time.perf_counter()
            sock.sendall(cmd)
            resp = sock.recv(1024)
            end = time.perf_counter()
            if resp:
                local_lat.append((end - start) * 1000)
                local_ops += 1
        except Exception:
            break
        i += 1

    with lat_lock:
        op_count[0] += local_ops
        latencies.extend(local_lat)
    sock.close()

def main():
    print(f"Running {NUM_THREADS} threads for {DURATION_SEC}s against {HOST}:{PORT}...")
    threads = []
    t0 = time.perf_counter()
    for t in range(NUM_THREADS):
        th = threading.Thread(target=worker, args=(t,), daemon=True)
        th.start()
        threads.append(th)

    time.sleep(DURATION_SEC)
    stop_flag.set()
    for th in threads:
        th.join(timeout=3)
    elapsed = time.perf_counter() - t0

    total_ops = op_count[0]
    print(f"\n--- RESULTS ({NUM_THREADS} threads, {DURATION_SEC}s) ---")
    print(f"Total ops completed : {total_ops:,}")
    print(f"Throughput          : {total_ops / elapsed:,.0f} ops/sec")
    if errors[0]:
        print(f"Connection errors   : {errors[0]}")
    if latencies:
        s = sorted(latencies)
        n = len(s)
        p50 = s[int(n * 0.50)]
        p95 = s[int(n * 0.95)]
        p99 = s[int(n * 0.99)]
        avg = statistics.mean(latencies)
        print(f"p50 latency         : {p50:.3f} ms")
        print(f"p95 latency         : {p95:.3f} ms")
        print(f"p99 latency         : {p99:.3f} ms")
        print(f"avg latency         : {avg:.3f} ms")
    return total_ops / elapsed

if __name__ == "__main__":
    main()
