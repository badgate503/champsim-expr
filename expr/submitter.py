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
parser.add_argument("--tracelist", "-l", nargs="*", type=str, help="指定某个 trace set（定义在 tracelist 文件中）")
parser.add_argument("--output", "-o", help="重定向实验结果输出目录")
parser.add_argument("--skip", "-s", action="store_true", help="对于已存在的输出文件的 trace，不重新跑；不指定则会覆盖之前的结果")
parser.add_argument("--auto", "-a", action="store_true", help="是否跳过所有交互提示")
parser.add_argument("--ncore","-n", type=int, default=1, help="每个实验 tuple 中的核心数量，默认为 1，即单核实验；如果指定为大于 1 的数值，则会从 sample_{ncore}core.csv 中读取对应的多核实验配置")

args = parser.parse_args()

if args.ncore == 1:
    if not args.traces and not args.tracelist:

        print(f"{RED}No trace assigned. Job done{END}")
        sys.exit(1)

# 读取 trace 名单
trace_name_set = set()

if args.traces is not None:
    trace_name_set.update(args.traces)


if args.ncore == 1:
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
    log_path = LOG_PATH
    if args.output is not None:
        log_path = os.path.abspath(args.output)
    task_set = {(p,t,log_path) for p in args.prefetcher for t in trace_name_set}
    print(f"Prefetchers: {YELLOW}{' '.join(args.prefetcher)}{END}")
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
                                    print(f"Exclude {YELLOW}{pp}{END}@{RED}{trace_name}{END}")
                            else:
                                print(f"Overwrite {YELLOW}{pp}{END}@{RED}{trace_name}{END}")
else:
    print(f"ncore = {args.ncore}")
    trace_mix_set = set()
    with open(f"./multicore/sample_{args.ncore}core.csv", "r") as f:
        lines = f.readlines()
        for line in lines:
            parts = line.strip().split(",")
            trace_mix_set.add(parts[0])
    print(f"Included multi-core trace sets from sample_{args.ncore}core.csv: {YELLOW}{' '.join(trace_mix_set)}{END}")
    log_path = LOG_PATH
    if args.output is not None:
        log_path = os.path.abspath(args.output)
    task_set = {(p,t,log_path) for p in args.prefetcher for t in trace_mix_set}

    for pp in args.prefetcher:
        if pp.endswith(".mc"):
            pp_basename = pp[:-3]
        else:
            pp_basename = pp    
        if os.path.isdir(log_path + "/" + pp_basename):
            for fname in os.listdir(log_path + "/" + pp_basename):
                if fname.endswith(".log"):
                    trace_name = fname[:-4]  # 去掉 .log
                    if trace_name in trace_mix_set:
                        
                        with open(log_path + "/" + pp_basename + "/" + fname, "r") as f:
                            if "ChampSim completed all CPUs" in f.read():
                                if args.skip:
                                    task_set.remove((pp, trace_name, log_path))
                                    print(f"Exclude {YELLOW}{pp}{END}@{RED}{trace_name}{END}")
                            else:
                                print(f"Overwrite {YELLOW}{pp}{END}@{RED}{trace_name}{END}")


task_map = {}

for t in task_set:
    key = t[0]
    task_map.setdefault(key, []).append(t[1])






for pp in args.prefetcher:
    if pp in task_map:
        mode_is_mc = pp.endswith(".mc")
        if mode_is_mc:
            out_path = f"{LOG_PATH}/{pp.replace('.mc', '')}"
        else:
            out_path = f"{LOG_PATH}/{pp}"
        if args.output is not None:
            out_path = os.path.abspath(args.output+f"/{pp}")
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

task_lines = '\n'.join([f'{YELLOW}{p}{END}@{RED}{t}{END} > {BLUE}{log_path}/{p}/{t}.log{END}' for p, t, log_path in task_set])
print(f"All tasks ({len(task_set)}):\n{task_lines}")
print(f"> proceed?")
input() 


print(f"submitted")


from multiprocessing.managers import BaseManager

class QueueManager(BaseManager): pass
QueueManager.register('get_queue')

manager = QueueManager(address=('127.0.0.1', 50003), authkey=b'abc')
manager.connect()
queue = manager.get_queue()

for pf, t, path in task_set:
    queue.put({"prefetcher":pf, "trace":t,"path":path, "numcore": args.ncore})