#!/usr/bin/env python3
import os
import time
import shutil
import subprocess
from pathlib import Path

# ===== 配置 =====
HINT_SRC = Path("../Kairos/expr/hint").resolve()
HINT_DST = Path("./hint").resolve()
TRACE_PATH = Path("../../champtraces").resolve()
LOG_BASE = Path("./log/prophet")
PREFETCHER = "prophet"   # 改成你要的
BIN = f"../bin/champsim.{PREFETCHER}"

WARM_UP = 0
INTERVAL = 250_000_000
MAX_CONCURRENT = 16
POLL_INTERVAL = 10  # 秒

LOG_BASE.mkdir(parents=True, exist_ok=True)
HINT_DST.mkdir(parents=True, exist_ok=True)

# ===== 运行状态 =====
running = []   # (proc, trace, start_time)
finished = set()
seen_hint_files = set()

# ===== 工具函数 =====
def find_trace_file(trace_name: str):
    for root, _, files in os.walk(TRACE_PATH):
        for f in files:
            if f.startswith(trace_name) and (f.endswith(".gz") or f.endswith(".xz")):
                return os.path.join(root, f)
    return None

def launch_trace(trace, trace_path):
    log_file = LOG_BASE / f"{trace}.log"
    cmd = [
        BIN,
        "--warmup-instructions", str(WARM_UP),
        "--simulation-instructions", str(INTERVAL),
        trace_path
    ]
    with open(log_file, "w") as f:
        f.write(" ".join(cmd) + "\n")
        p = subprocess.Popen(cmd, stdout=f, stderr=f)
    return (p, trace, time.time())

def print_status():
    os.system("clear")
    print("=== Running ===")
    now = time.time()
    for p, t, st in running:
        if p.poll() is None:
            dt = int(now - st)
            print(f"{t:<30} RUNNING   {dt//3600:02d}:{dt%3600//60:02d}:{dt%60:02d}")
    print("\n=== Finished ===")
    for t in sorted(finished):
        print(f"{t}")

# ===== 主循环 =====
while True:
    # 1. 扫描新 hint 文件
    for f in HINT_SRC.glob("*.txt"):
        if f.name in seen_hint_files:
            continue

        print(f"[+] Detected new hint file: {f.name}")
        seen_hint_files.add(f.name)

        # 复制到本地
        dst = HINT_DST / f.name
        shutil.copy2(f, dst)

        # 读取 trace 列表
        with open(f, "r") as hf:
            traces = [line.strip() for line in hf if line.strip()]

        for trace in traces:
            if trace in finished:
                continue

            trace_file = find_trace_file(trace)
            if trace_file is None:
                print(f"[!] Trace not found: {trace}")
                continue

            # 等并发空位
            while len(running) >= MAX_CONCURRENT:
                time.sleep(2)
                new_running = []
                for p, t, st in running:
                    if p.poll() is None:
                        new_running.append((p, t, st))
                    else:
                        finished.add(t)
                running = new_running

            running.append(launch_trace(trace, trace_file))

    # 2. 更新运行状态
    new_running = []
    for p, t, st in running:
        if p.poll() is None:
            new_running.append((p, t, st))
        else:
            finished.add(t)
    running = new_running

    # 3. 输出状态
    print_status()

    # 4. sleep
    time.sleep(POLL_INTERVAL)