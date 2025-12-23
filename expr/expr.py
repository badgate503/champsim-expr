#!/usr/bin/env python3


import os
import sys 
import argparse
from itertools import product
import json
import time
import random
import subprocess
RED = '\033[91m'
GREEN = '\033[92m'
YELLOW = '\033[93m'
BLUE = '\033[94m'
MAGENTA = '\033[95m'
CYAN = '\033[96m'
WHITE = '\033[97m'
BOLD = '\033[1m'
UNDERLINE = '\033[4m'
END = '\033[0m'
WARM_UP = 0
INTERVAL = 250_000_000
LOG_BASE = "./log"
TRACE_PATH =  "../../champtraces/"
parser = argparse.ArgumentParser()

parser.add_argument("--compile", "-c", action="store_true", help="do compile champsim")
parser.add_argument("--traces", "-t", nargs="+")
parser.add_argument("--interval", "-i")
parser.add_argument("--tracelist", "-l", nargs="*", type=str, help="specify tracelist lines to use")



args = parser.parse_args()
if args.interval is not None:
    INTERVAL = int(args.interval)

# 读取 trace 名单
trace_name_set = set()

if args.traces is not None:
    trace_name_set.update(args.traces)


# 支持 -l 多参数，每个参数为 tracelist 文件的一行前缀
if args.tracelist is not None and len(args.tracelist) > 0:
    with open("./tracelist", "r") as f:
        lines = f.readlines()
    for prefix in args.tracelist:
        for line in lines:
            if line.startswith(prefix + ":"):
                # 取冒号后所有 trace 名字
                traces = line.split(":", 1)[1].strip().split()
                trace_name_set.update(traces)
                print(f"Include {len(traces)} traces from {CYAN}{prefix}{END}: {' '.join(traces)}\n")

# 检查 ./log 下是否存在 XXX.txt，如果存在且 trace_name_set 中有 XXX，则移除 XXX
log_dir = LOG_BASE
if os.path.isdir(log_dir):
    for fname in os.listdir(log_dir):
        if fname.endswith(".txt"):
            trace_name = fname[:-4]  # 去掉 .txt
            if trace_name in trace_name_set:
                trace_name_set.remove(trace_name)
                print(f"Exclude {RED}{trace_name}{END}")

print(f"\nTotal traces: {len(trace_name_set)}, proceed?")
input()


all_trace_list = []
for root, dirs, files in os.walk(TRACE_PATH):
    for f in files:
        if f.endswith(".xz") or f.endswith(".gz"):
            trace_name = f.replace('.champsimtrace.xz',"").replace('.champsimtrace.gz',"")
            if not trace_name_set or trace_name in trace_name_set:
                p = os.path.join(root, f)
                all_trace_list.append((trace_name, p))


if args.compile:
    with open("../champsim_config.json", "r", encoding="utf-8") as f:
        data = json.load(f)
        data["L1D"]["prefetcher"] = "no"
        data["L2C"]["prefetcher"] = "prophet_profile"
        data["LLC"]["prefetcher"] = "no"
        data["executable_name"] = f"champsim.expr"
    with open("../champsim_config.json", "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=4)
    os.system("cd .. && ./config.sh champsim_config.json && make -j32")

# 并发限制
MAX_CONCURRENT = 32
pending_tasks = list(all_trace_list)
processes = []  # (process, w, start_time)

def launch_task(w, path):
    work = [f"../bin/champsim.expr", "--warmup-instructions", f"{WARM_UP}", "--simulation-instructions", f"{INTERVAL}", path]
    with open(f"{LOG_BASE}/{w}.log", "w") as f:
        f.write(" ".join(work))
        f.write("\n")
        process = subprocess.Popen(
            work,
            stdout=f,
            stderr=f
        )
    return (process, w, time.time())

# 先启动最多 MAX_CONCURRENT 个
while pending_tasks and len(processes) < MAX_CONCURRENT:
    w, path = pending_tasks.pop(0)
    processes.append(launch_task(w, path))

while processes:
    os.system("clear")
    still_run = False
    new_processes = []
    for p, w, st in processes:
        if p.poll() is None:
            run_time = int(time.time() - st)
            h = run_time // 3600
            m = (run_time % 3600) // 60
            s = run_time % 60
            print(f"{w:<30}Running   {h:02d}:{m:02d}:{s:02d}")
            still_run = True
            new_processes.append((p, w, st))
        else:
            run_time = int(time.time() - st)
            h = run_time // 3600
            m = (run_time % 3600) // 60
            s = run_time % 60
            print(f"{w:<30}Terminated {h:02d}:{m:02d}:{s:02d}")
    # 启动新任务补足并发
    while pending_tasks and len(new_processes) < MAX_CONCURRENT:
        w, path = pending_tasks.pop(0)
        new_processes.append(launch_task(w, path))
        still_run = True
    processes = new_processes
    if not still_run:
        break
    time.sleep(5)

