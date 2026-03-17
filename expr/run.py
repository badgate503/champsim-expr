#!/usr/bin/env python3


import os
import sys 
import argparse
from itertools import product
import json
import time
import random
import subprocess

from utils.defs import *
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
WARM_UP = 50_000_000
INTERVAL = 200_000_000
parser = argparse.ArgumentParser()


parser.add_argument("--prefetcher", "-p",nargs="+", required=True, help="选用的预取器名称")
parser.add_argument("--traces", "-t", nargs="+", help="指定 trace 文件名单列表")
parser.add_argument("--interval", "-i", help="指定仿真区间长度（指令数）")
parser.add_argument("--warmup", "-w", help="指定仿真预热长度（指令数）")
parser.add_argument("--auto", "-a", action="store_true", help="是否跳过所有交互提示")
parser.add_argument("--tracelist", "-l", nargs="*", type=str, help="指定某个 trace set（定义在 tracelist 文件中）")
parser.add_argument("--output", "-o", help="重定向实验结果输出目录")
parser.add_argument("--remain", "-r", action="store_true", help="对于已存在的输出文件的 trace，不重新跑；不指定则会覆盖之前的结果")
args = parser.parse_args()


if not args.traces and not args.tracelist:
    print(f"{RED}No trace assigned. Job done{END}")
    sys.exit(1)

if args.interval is not None:
    INTERVAL = int(args.interval)

if args.warmup is not None:
    WARM_UP = int(args.warmup)

# 读取 trace 名单
trace_name_set = set()

if args.traces is not None:
    trace_name_set.update(args.traces)


# 支持 -l 多参数，每个参数为 tracelist 文件的一行前缀
if args.tracelist is not None and len(args.tracelist) > 0:
    with open("./utils/tracelist", "r") as f:
        lines = f.readlines()
    for prefix in args.tracelist:
        for line in lines:
            if line.startswith(prefix + ":"):
                # 取冒号后所有 trace 名字
                traces = line.split(":", 1)[1].strip().split()
                trace_name_set.update(traces)
                print(f"Include {len(traces)} traces from {CYAN}{prefix}{END}: {RED}{' '.join(traces)}{END}\n")

task_set = {(p,t) for p in args.prefetcher for t in trace_name_set}
print(f"Prefetchers: {YELLOW}{' '.join(args.prefetcher)}{END}")
for pp in args.prefetcher:
    log_dir = LOG_PATH + f"/{pp}/"
    if os.path.isdir(log_dir):
        for fname in os.listdir(log_dir):
            if fname.endswith(".txt"):
                trace_name = fname[:-4]  # 去掉 .txt
                if trace_name in trace_name_set:
                    if args.remain:
                        task_set.remove((pp, trace_name))
                        print(f"Exclude {YELLOW}{pp}{END}@{RED}{trace_name}{END}")
                    else:
                        print(f"Overwrite {YELLOW}{pp}{END}@{RED}{trace_name}{END}")


task_map = {}

for t in task_set:
    key = t[0]
    task_map.setdefault(key, []).append(t[1])

print(f"All tasks: {' '.join([f'{YELLOW}{p}{END}@{RED}{t}{END}' for p,t in task_set])}")


trace_path_map = dict()
for root, dirs, files in os.walk(TRACE_PATH):
    for f in files:
        if f.endswith(".xz") or f.endswith(".gz"):
            trace_name = f.replace('.champsimtrace.xz',"").replace('.champsimtrace.gz',"")
            if trace_name in trace_name_set:
                p = os.path.join(root, f)
                trace_path_map[trace_name] = p

for pp in args.prefetcher:
    mode_is_mc = pp.endswith(".mc")
    if mode_is_mc:
        out_path = f"{LOG_PATH}/{pp.replace('.mc', '')}"
    else:
        out_path = f"{LOG_PATH}/{pp}"
    if args.output is not None:
        out_path = os.path.abspath(args.output)
    print(f"\n{'='*60}\n")
    print(f"> Executable: {CYAN}../bin/{pp}{END}")
    print(f"> Warm-up: {CYAN}{WARM_UP}{END}, Interval: {CYAN}{INTERVAL}{END}")
    
    os.system(f"stat ../bin/{pp} | grep 最近更改")
    if os.path.exists(f"../bin/champsim_config_{pp}.json"):
        with open(f"../bin/champsim_config_{pp}.json", "r", encoding="utf-8") as f:
            data = json.load(f)
            print(f"> L2C Config: {CYAN}{data['L2C']}{END}")
    else:
        print(f"> L2C Config: (File not exist)")
    print(f"> Traces: {YELLOW}{' '.join(task_map[pp])}{END}")
    if mode_is_mc:
        print(f"> Logs will be saved to {out_path}/ as {YELLOW}.log{END} (Champsim log) and {YELLOW}.txt{END} (Miss cause classification log) files")
    else:
        print(f"> Logs will be saved to {out_path}/ as {YELLOW}.log{END} (Champsim log) files")
    
    
    if f"{pp}" not in os.listdir("../bin/"):
        print(f"> {RED}Error: Executable {pp} not found in ../bin/. Please compile first with --compile flag.{END}")
        sys.exit(1)
print(f"> proceed?")
input() 
def launch_task(p, w):
    mode_is_mc = p.endswith(".mc")
    
    work = [f"/mnt/data/lyq/Kairos/bin/{p}", "--warmup-instructions", f"{WARM_UP}", "--simulation-instructions", f"{INTERVAL}", trace_path_map[w]]
    name = f"{w}.log"
    if mode_is_mc:
        out_path = f"{LOG_PATH}/{p.replace('.mc', '')}"
    else:
        out_path = f"{LOG_PATH}/{p}"
    if args.output is not None:
        out_path = os.path.abspath(args.output)
    #work = ["sleep", "10"]
    os.makedirs(f"{out_path}/", exist_ok=True)
    with open(f"{out_path}/{name}", "w") as f:
        f.write(" ".join(work))
        f.write("\n\n")
        data = None
        if os.path.exists(f"../bin/champsim_config_{pp}.json"):
            with open(f"../bin/champsim_config_{pp}.json", "r", encoding="utf-8") as rf:
                data = json.load(rf)
        if data is not None:
            for conf in data['L1D'].items():
                f.write(f"L1D_{conf[0]}:{conf[1]}\n")
            for conf in data['L2C'].items():
                f.write(f"L2C_{conf[0]}:{conf[1]}\n")
            for conf in data['LLC'].items():
                f.write(f"LLC_{conf[0]}:{conf[1]}\n")
            for conf in data['physical_memory'].items():
                f.write(f"DRAM_{conf[0]}:{conf[1]}\n")
        f.write("\n")
        process = subprocess.run(
            work,
            stdout=f,
            stderr=f,
            cwd=out_path
        )
    return f"{YELLOW}{p}{END}@{RED}{w}{END}"
import time
def dummy_launch_task(p,w):
    time.sleep(10)
    return f"{YELLOW}{p}{END}@{RED}{w}{END}"

from concurrent.futures import ProcessPoolExecutor, as_completed
all_task_n = len(task_set)
remaining = len(task_set)
print(f"\n总任务数: {all_task_n}，开始执行")
if __name__ == "__main__":
    with ProcessPoolExecutor(max_workers=500) as executor:
        futures = [executor.submit(launch_task, p,w) for p,w in task_set]
        for future in as_completed(futures):
            remaining -= 1
            print(future.result(), f"完成, 剩余: {remaining}/{all_task_n}")


