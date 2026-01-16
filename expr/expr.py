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

parser.add_argument("--mode", "-m", choices=["ipc", "missclass"], required=True, help="选择是否输出 miss cause classification 日志")
parser.add_argument("--compile", "-c", action="store_true", help="是否先对可执行文件进行编译")
parser.add_argument("--debug", "-d", choices=["default", "asan"], help="选择编译模式，Default 为开启 -g -O0; Asan 为开启 AddressSanitizer")
parser.add_argument("--prefetcher", "-p" , required=True, help="选用的预取器名称")


parser.add_argument("--traces", "-t", nargs="+", help="指定 trace 文件名单列表")
parser.add_argument("--interval", "-i", help="指定仿真区间长度（指令数）")
parser.add_argument("--warmup", "-w", help="指定仿真预热长度（指令数）")
parser.add_argument("--auto", "-a", action="store_true", help="是否跳过所有交互提示")
parser.add_argument("--tracelist", "-l", nargs="*", type=str, help="指定某个 trace set（定义在 tracelist 文件中）")
parser.add_argument("--output", "-o", help="重定向输出目录")
parser.add_argument("--exename", "-e", help="指定可执行文件名称")
parser.add_argument("--remain", "-r", action="store_true", help="对于已存在的输出文件的 trace，不重新跑；不指定则会覆盖之前的结果")
args = parser.parse_args()

extra_cflags=[]
extra_ldflags=[]

if args.debug != None:
    if args.debug == "default":
        extra_cflags.extend(["-g", "-O0"])
    elif args.debug == "asan":
        extra_cflags.extend(["-fsanitize=address", "-g", "-O0"])
        extra_ldflags.append("-fsanitize=address")


if args.mode == "missclass":
    extra_cflags.append("-DELABORATE_LOG")



if args.compile:
    with open("../champsim_config.json", "r", encoding="utf-8") as f:
        data = json.load(f)
        data["L1D"]["prefetcher"] = "no"
        data["L2C"]["prefetcher"] = args.prefetcher
        data["LLC"]["prefetcher"] = "no"
        if args.mode == "missclass":
            data["executable_name"] = f"champsim.{args.prefetcher}.mc"
        else:
            data["executable_name"] = f"champsim.{args.prefetcher}"
        if args.exename is not None:
            data["executable_name"] = args.exename
    with open("../champsim_config.json", "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=4)

    print("=============== Compiling ================")
    print(f"Executable: {data['executable_name']}")
    print(f"CXXFLAGS: {' '.join(extra_cflags)}")
    print(f"LDFLAGS: {' '.join(extra_ldflags)}")
    print("=============== Compiling ================")
    if not args.auto:
        input()
    os.system("cd .. && make clean")
    os.system(f"cd .. && ./config.sh champsim_config.json && make CXXFLAGS=\"{' '.join(extra_cflags)}\" LDFLAGS=\"{' '.join(extra_ldflags)}\" -j32")
    os.system(f"cp ../champsim_config.json ../bin/champsim_config_{data['executable_name']}.json")
    
if not args.traces and not args.tracelist:
    print(f"{RED}No trace assigned. Job done{END}")
    sys.exit(1)

if args.mode == "ipc":
    WARM_UP = 50_000_000
    INTERVAL = 200_000_000

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
                print(f"Include {len(traces)} traces from {CYAN}{prefix}{END}: {' '.join(traces)}\n")

log_dir = LOG_PATH + f"/{args.prefetcher}/"
if os.path.isdir(log_dir):
    for fname in os.listdir(log_dir):
        if fname.endswith(".txt"):
            trace_name = fname[:-4]  # 去掉 .txt
            if trace_name in trace_name_set:
                if args.remain:
                    trace_name_set.remove(trace_name)
                    print(f"Exclude {RED}{trace_name}{END}")
                else:
                    print(f"Overwrite {GREEN}{trace_name}{END}")
all_trace_list = []
for root, dirs, files in os.walk(TRACE_PATH):
    for f in files:
        if f.endswith(".xz") or f.endswith(".gz"):
            trace_name = f.replace('.champsimtrace.xz',"").replace('.champsimtrace.gz',"")
            if trace_name in trace_name_set:
                p = os.path.join(root, f)
                all_trace_list.append((trace_name, p))

exe_name = f"champsim.{args.prefetcher}" if args.mode == "ipc" else f"champsim.{args.prefetcher}.mc"




out_path = f"{LOG_PATH}/{args.prefetcher}"
if args.output is not None:
    out_path = os.path.abspath(args.output)

print(f"\n{'='*60}\n")
print(f"Mode: {CYAN}{args.mode}{END}, Warm-up: {CYAN}{WARM_UP}{END}, Interval: {CYAN}{INTERVAL}{END}")
print(f"Executable: {CYAN}../bin/{exe_name}{END}")
os.system(f"stat ../bin/{exe_name} | grep 最近更改")
print("\n===============================================\n")
with open(f"../bin/champsim_config_{exe_name}.json", "r", encoding="utf-8") as f:
    data = json.load(f)
    print(f"L1D Config: {CYAN}{data['L1D']}{END}\n")
    print(f"L2C Config: {CYAN}{data['L2C']}{END}\n")
    print(f"LLC Config: {CYAN}{data['LLC']}{END}\n")
print("\n===============================================\n")
if args.mode == "missclass":
    print(f"Logs will be saved to {out_path}/ as {YELLOW}.log{END} (Champsim log) and {YELLOW}.txt{END} (Miss cause classification log) files")
else:
    print(f"Logs will be saved to {out_path}/ as {YELLOW}.log{END} (Champsim log) files")

if exe_name not in os.listdir("../bin/"):
    print(f"{RED}Error: Executable {exe_name} not found in ../bin/. Please compile first with --compile flag.{END}")
    sys.exit(1)
print(f"\nTotal traces: {len(all_trace_list)}, proceed?")
if not args.auto:
    input("Press Enter to continue...")







# 并发限制
MAX_CONCURRENT = 89
pending_tasks = list(all_trace_list)
processes = []  # (process, w, start_time)

def launch_task(w, path):
    if args.mode == "missclass":
        work = [f"../bin/champsim.{args.prefetcher}.mc", "--warmup-instructions", f"{WARM_UP}", "--simulation-instructions", f"{INTERVAL}", path]
        name = f"{w}.log"
    elif args.mode == "ipc":
        work = [f"../bin/champsim.{args.prefetcher}", "--warmup-instructions", f"{WARM_UP}", "--simulation-instructions", f"{INTERVAL}", path]
        name = f"{w}.log"
    #work = ["sleep", "10"]
    os.makedirs(f"{out_path}/", exist_ok=True)
    with open(f"{out_path}/{name}", "w") as f:

        f.write(" ".join(work))
        f.write("\n\n")
        for conf in data['L1D'].items():
            f.write(f"L1D_{conf[0]}:{conf[1]}\n")
        for conf in data['L2C'].items():
            f.write(f"L2C_{conf[0]}:{conf[1]}\n")
        for conf in data['LLC'].items():
            f.write(f"LLC_{conf[0]}:{conf[1]}\n")
        for conf in data['physical_memory'].items():
            f.write(f"DRAM_{conf[0]}:{conf[1]}\n")
        f.write("\n")
        process = subprocess.Popen(
            work,
            stdout=f,
            stderr=f
        )
    return (process, w, time.time())
N_PROC = len(pending_tasks)
# 先启动最多 MAX_CONCURRENT 个
while pending_tasks and len(processes) < MAX_CONCURRENT:
    w, path = pending_tasks.pop(0)
    processes.append(launch_task(w, path))


while processes:
    os.system("clear")
    still_run = False
    running = 0

    for p, w, st in processes:
        if p.poll() is None:
            run_time = int(time.time() - st)
            h = run_time // 3600
            m = (run_time % 3600) // 60
            s = run_time % 60
            print(f"{w:<30}Running   {h:02d}:{m:02d}:{s:02d}")
            still_run = True
            running+=1
            #new_processes.append((p, w, st))
        else:
            run_time = int(time.time() - st)
            h = run_time // 3600
            m = (run_time % 3600) // 60
            s = run_time % 60
            print(f"{w:<30}Terminated {h:02d}:{m:02d}:{s:02d}")
    print(f"Current Prefetcher: {args.prefetcher}, Total: {N_PROC}, Running: {running}, Pending: {len(pending_tasks)}, Terminated: {N_PROC - len(pending_tasks) - running}")
    # 启动新任务补足并发
    while pending_tasks and running < MAX_CONCURRENT:
        running+=1
        w, path = pending_tasks.pop(0)
        processes.append(launch_task(w, path))
        still_run = True
    if not still_run:
        break
    time.sleep(1)

