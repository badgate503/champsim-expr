#!/usr/bin/env python3

import os
import sys 
import argparse
from itertools import product
import json
import time
import random
import multiprocessing as mp
import subprocess
from multiprocessing import Pool
from tqdm import tqdm

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


parser.add_argument("--prefetcher", "-p",nargs="+", required=True, help="Name(s) of the prefetcher(s) to run (e.g., baseline triangel prophet prism).")
parser.add_argument("--traces", "-t", nargs="+", help="Name(s) of the trace(s) to run (e.g., bc-0 bc-12).")
parser.add_argument("--tracelist", "-l", nargs="*", type=str, help="Name(s) of the trace Set(s) to run (e.g., ligra gap spec17 ml google).")
parser.add_argument("--output", "-o", help="Directory to redirect experiment results to")
parser.add_argument("--skip", "-s", action="store_true", help="Skip re-running those whose output files already exist; otherwise, overwrite previous results")
parser.add_argument("--nproc", "-j", type=int, default=90, help="Number of parallel jobs to use")

args = parser.parse_args()

trace_path_map = dict()
for root, dirs, files in os.walk(TRACE_PATH):
    for f in files:
        if f.endswith(".xz") or f.endswith(".gz"):
            trace_name = f.replace('.champsimtrace.xz',"").replace('.champsimtrace.gz',"").replace('.champsim.gz',"")
            trace_path_map[trace_name] = os.path.join(root, f)


if not args.traces and not args.tracelist:

    print(f"{RED}No trace assigned. Job done{END}")
    sys.exit(1)

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
               # print(f"Include {len(traces)} traces from {CYAN}{prefix}{END}: {RED}{' '.join(traces)}{END}\n")
log_path = LOG_PATH
if args.output is not None:
    log_path = os.path.abspath(args.output)
task_set = {(p,t,log_path) for p in args.prefetcher for t in trace_name_set}
#print(f"Prefetchers: {YELLOW}{' '.join(args.prefetcher)}{END}")
for pp in args.prefetcher:
    if pp.endswith(".mc"):
        pp_basename = pp[:-3]
    else:
        pp_basename = pp    
    if os.path.isdir(log_path + "/" + pp_basename):
        for fname in os.listdir(log_path + "/" + pp_basename):
            if fname.endswith(".log"):
                trace_name = fname[:-4]  # 去掉 .log
                if trace_name in trace_name_set:
                    
                    with open(log_path + "/" + pp_basename + "/" + fname, "r") as f:
                        if "ChampSim completed all CPUs" in f.read():
                            if args.skip:
                                task_set.remove((pp, trace_name, log_path))

task_map = {}

for t in task_set:
    key = t[0]
    task_map.setdefault(key, []).append(t[1])






for pp in args.prefetcher:
    if pp in task_map:
        mode_is_mc = pp.endswith(".mc")
        out_path = f"{LOG_PATH}/{pp}"
        if args.output is not None:
            out_path = os.path.abspath(args.output+f"/{pp}")
        # print(f"\n{'='*60}\n")
        # print(f"> Executable: {CYAN}../bin/{pp}{END}")
        # print(f"> Warm-up: {CYAN}{WARM_UP}{END}, Interval: {CYAN}{INTERVAL}{END}")
        
        # os.system(f"stat ../bin/{pp} | grep 最近更改")
        # if os.path.exists(f"../bin/champsim_config_{pp}.json"):
        #     with open(f"../bin/champsim_config_{pp}.json", "r", encoding="utf-8") as f:
        #         data = json.load(f)
        #         print(f"> L2C Config: {CYAN}{data['L2C']}{END}")
        # else:
        #     print(f"> L2C Config: (File not exist)")
        # print(f"> Traces: {YELLOW}{' '.join(task_map[pp])}{END}")
        # if mode_is_mc:
        #     print(f"> Logs will be saved to {out_path}/ as {YELLOW}.log{END} (Champsim log) and {YELLOW}.txt{END} (Miss cause classification log) files")
        # else:
        #     print(f"> Logs will be saved to {out_path}/ as {YELLOW}.log{END} (Champsim log) files")
        
        
        if f"{pp}" not in os.listdir("../bin/"):
            print(f"> {RED}Error: Executable {pp} not found in ../bin/. Please compile first with --compile flag.{END}")
            sys.exit(1)

task_lines = ', '.join([f'{YELLOW}{p}{END}@{RED}{t}{END}' for p, t, log_path in task_set])
n_proc = args.nproc
print(f"\nNumber of Champsim tasks: {len(task_set)}. Number of processes: {n_proc}")

def launch_task(task):
    p, w, log = task
    w = [w]
    mode_is_mc = p.endswith(".mc")
    trace_all = [trace_path_map[v] for v in w]
    work = [f"{CHAMPSIM_PATH}/bin/{p}", "--warmup-instructions", f"{WARM_UP}", "--simulation-instructions", f"{INTERVAL}"] + trace_all
    name = f"{w[0]}.log"
    out_path = f"{log}/{p}"
    os.makedirs(f"{out_path}/", exist_ok=True)
    with open(f"{out_path}/{name}", "w") as f:
        f.write(" ".join(work))
        f.write("\n\n")
        data = None
        if os.path.exists(f"../bin/champsim_config_{p}.json"):
            with open(f"../bin/champsim_config_{p}.json", "r", encoding="utf-8") as rf:
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
        f.flush()
        process = subprocess.run(
            work,
            stdout=f,
            stderr=f,
            cwd=out_path
        )
    if len(w) == 1:
        return f"{YELLOW}{p}{END}@{RED}{w[0]}{END}"
    else:
        return f"{YELLOW}{p}{END}@{RED}{len(w)}core-{alias}{END}"

if __name__ == '__main__':
    
    # 2. 创建进程池
    # total 参数告诉 tqdm 总共有多少个任务，以便正确计算百分比
    with Pool(processes=n_proc) as pool:
        results = []
        
        # 使用 pool.imap 结合 tqdm 动态显示进度
        # 如果对结果顺序没有要求，用 imap_unordered 性能会更好一点
        # 使用 tqdm 包裹 imap_unordered，这样哪个进程先算完就先弹出来
        pbar = tqdm(
            pool.imap_unordered(launch_task, task_set), 
            total=len(task_set), 
            desc="Running Champsim tasks"
        )
        
        results = []
        for message in pbar:
            # 【核心】：使用 pbar.write() 代替 print()
            # 它会自动把光标移到进度条上方输出日志，并将进度条牢牢固定在最底下
            pbar.write("Finished: "+message)
            
            
    print(f"{len(task_set)} Champsim task completed.")