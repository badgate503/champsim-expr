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

parser.add_argument("--interval", "-i", help="指定仿真区间长度（指令数）")
parser.add_argument("--warmup", "-w", help="指定仿真预热长度（指令数）")
args = parser.parse_args()

from multiprocessing.managers import BaseManager
import multiprocessing as mp
from concurrent.futures import ProcessPoolExecutor
import time

task_queue = mp.Queue()

class QueueManager(BaseManager): pass

QueueManager.register('get_queue', callable=lambda: task_queue)


if args.interval is not None:
    INTERVAL = int(args.interval)

if args.warmup is not None:
    WARM_UP = int(args.warmup)

trace_path_map = dict()
for root, dirs, files in os.walk(TRACE_PATH):
    for f in files:
        if f.endswith(".xz") or f.endswith(".gz"):
            trace_name = f.replace('.champsimtrace.xz',"").replace('.champsimtrace.gz',"").replace('.champsim.gz',"")
            trace_path_map[trace_name] = os.path.join(root, f)

def launch_task(p, w, log, alias = ""):
    mode_is_mc = p.endswith(".mc")
    trace_all = [trace_path_map[v] for v in w]
    work = [f"/mnt/data/lyq/PRISM/bin/{p}", "--warmup-instructions", f"{WARM_UP}", "--simulation-instructions", f"{INTERVAL}"] + trace_all
    if alias == "":
        name = f"{w[0]}.log"
    else:
        name = f"{alias}.log"
    if mode_is_mc:
        out_path = f"{log}/{p.replace('.mc', '')}"
    else:
        if len(w) == 1:
            out_path = f"{log}/{p}"
        else:
            out_path = f"{log}/core{len(w)}/{p}"
    #print(out_path)
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
import time
def dummy_launch_task(p,w,l, alias = ""):
    mode_is_mc = p.endswith(".mc")
    trace_all = [trace_path_map[v] for v in w]
    work = [f"/mnt/data/lyq/PRISM/bin/{p}", "--warmup-instructions", f"{WARM_UP}", "--simulation-instructions", f"{INTERVAL}"] + trace_all
    print(f"模拟执行: {' '.join(work)}")
    time.sleep(30)
    return f"{YELLOW}{p}{END}@{RED}{w}{END}alias={alias}{END}"
from queue import Empty

if __name__ == "__main__":
    manager = QueueManager(address=('', 50003), authkey=b'abc')
    manager.start()
    print("Runner Manager started on port 50003")
    
    # 2. 获取受管理的共享队列对象 (关键！)
    shared_queue = manager.get_queue()
    
    # 3. 初始化线程/进程池
    # 注意：500个进程通常太多了，除非你的机器有几百个核心，否则上下文切换会拖慢速度
    executor = ProcessPoolExecutor(max_workers=500) 

    running = []
    total_n = 0
    outed = False

    print("等待任务提交...")
    
    # 4. 直接在主进程开始循环监控
    try:
        while True:
            # 尝试从共享队列获取任务
            try:
                while True:
                    # 使用从 manager 获取的 shared_queue
                    task = shared_queue.get_nowait()
                    p, w, log, ncore = task["prefetcher"], task["trace"], task["path"], task["numcore"]
                    
                    if ncore == 1:
                        future = executor.submit(launch_task, p, [w], log)
                        print(f"新增: {YELLOW}{p}{END}@{RED}{w}{END}, 队列中任务数: {total_n}，已加载: {len(running)}")
                    else:
                        with open(f"./multicore/sample_{ncore}core.csv", "r") as f:
                            lines = f.readlines()
                            candidates = []
                            for line in lines:
                                parts = line.strip().split(",")
                                if parts[0] == w:
                                    candidates = parts[1:]    
                                    #print(line)
                        future = executor.submit(launch_task, p, candidates, log, alias=f"{w}")
                        print(f"新增: {YELLOW}{p}{END}@{RED}{ncore}core-{w}{END}, 队列中任务数: {total_n}，已加载: {len(running)}")
                
                    running.append(future)
                    
                    total_n += 1
                    
                    outed = False
            except Empty:
                pass

            # 检查已完成的任务
            for f in [x for x in running if x.done()]:
                total_n -= 1
                print(f"完成: {f.result()}, 剩余 {total_n}，已加载: {len(running)}")
                running.remove(f)

            if total_n == 0 and not outed:
                print("所有任务已完成。等待新任务提交 ./submitter.py -p [prefetchers] -l [tracelist]")
                outed = True
            
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("正在停止管理器...")
        executor.shutdown(wait=False)
        manager.shutdown()