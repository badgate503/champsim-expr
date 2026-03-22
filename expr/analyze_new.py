#!/usr/bin/env python3

# Read log/{prefetcher}/{trace}.log and analyze the cause of each miss -> data/{prefetcher}/{trace}_breif.txt.
# Read log/{prefetcher}/{trace}.out and calculate ipc -> data/{prefetcher}/ipc.txt


import os
import sys 
import argparse
from itertools import product
import json
from tqdm import tqdm
import time
import random
import subprocess
from concurrent.futures import ProcessPoolExecutor, as_completed
from utils.defs import *
from utils.get_measure import get_ipc

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
INTERVAL = 250000000
parser = argparse.ArgumentParser()
parser.add_argument("--traces", "-t", nargs="+")
parser.add_argument("--print", "-a", action="store_true")
parser.add_argument("--prefetcher", "-p")
parser.add_argument("--json", "-j", action="store_true")
args = parser.parse_args()

from enum import Enum

class miss_cause(Enum):
    COMPULSORY_MISS       = 0
    UNSEEN_CORRELATION    = 1
    LATE_PREFETCH         = 2
    METADATA_LOSS         = 3


import glob

data_dir = os.path.join(RESULT_PATH, "data", args.prefetcher)
os.makedirs(data_dir, exist_ok=True)
already_analyzed = []
for result in glob.glob(os.path.join(data_dir, "*_breif.txt")):
    t = os.path.splitext(os.path.basename(result))[0]
    already_analyzed.append(t.replace("_breif", ""))
print(f"Already analyzed {len(already_analyzed)} traces: {already_analyzed}")



#all_counters = {}
print(LOG_PATH)
NPROC = 500
working_queue = []
for log_file in glob.glob(os.path.join(LOG_PATH, args.prefetcher, '*.txt')):
    
    t = os.path.splitext(os.path.basename(log_file))[0]
    if not args.traces:
        args.traces = []
    
    if t in already_analyzed and t not in args.traces:
        continue
    if len(args.traces) > 0 and t not in args.traces:
        continue
    with open(os.path.join(LOG_PATH, args.prefetcher, f"{t}.log"), "r") as f:

        if not "ChampSim completed all CPUs" in f.read():
            continue
    working_queue.append((t,log_file))

print(f"Analyzing: {' '.join([t for t, _ in working_queue])}, total {len(working_queue)} traces.")


def analyze_one(t, log_file):
    add = 0
    evict = 0
    issue = 0
    miss = 0

    misses = set()
    md_targets = {}

    #metadata = {}
    i = 0
    counters = {cause.name:0 for cause in miss_cause}
    warmed = False
    #####
    last_addr_is_0 = set()
    last_addr_is_addr = set()
    real_last = {}
    issue_status = dict()
    demand_timestamp = 0
    last_evict_timestamp = dict() # addr -> last evict timestamp
    distances = []
    #print(f"{CYAN}{t}{END}: Reading logs from {YELLOW}{log_file}{END}")
    with open(log_file) as f:
        if args.print:
            full_log = open(f"{RESULT_PATH}/data/{args.prefetcher}/{t}_full.txt", "w")
        for line in f:
            lst = line.strip().split(" ")
            if lst[1] == "HIT":
                demand_timestamp += 1
                addr = int(lst[2], 16)
                ip = int(lst[3], 16)
                #last_addr = int(lst[4], 16) # from pcTable
                last_addr = real_last.get(ip) if real_last.get(ip) is not None else 0 # real last addr
                triggers = [int(x, 16) for x in lst[5:]]

                #logs.append(hit_log(int(lst[0]), int(lst[2], 16), int(lst[3], 16), int(lst[4],16), [int(x, 16) for x in lst[5:]]))
                if last_addr == 0:
                    last_addr_is_0.add(addr)
                else:
                    last_addr_is_0.discard(addr)
                if last_addr == addr:
                    last_addr_is_addr.add(addr)
                else:
                    last_addr_is_addr.discard(addr)
                if args.print:
                    full_log.write(f"[{lst[0]:^12}] HIT (PC = {ip:#x}, access = {addr:#x}, last access = {last_addr:#x}, exist triggers: {[f'{x:#x}' for x in triggers]})\n")
                real_last[ip] = addr
            if lst[1] == "MISS":
                demand_timestamp += 1
                miss += 1
                late = lst[2]
                addr = int(lst[3], 16)
                ip = int(lst[4], 16)
                #last_addr = int(lst[5], 16) # from pcTable
                last_addr = real_last.get(ip) if real_last.get(ip) is not None else 0 # real last addr
                miss_type = lst[5]
                triggers = [int(x, 16) for x in lst[6:]]
                
                
                if addr not in misses:
                    misses.add(addr)
                    cause = miss_cause.COMPULSORY_MISS
                elif late != "NO":
                        cause = miss_cause.LATE_PREFETCH
                else:
                    entries = md_targets.get(addr)
                    if entries:
                        if last_addr in entries.keys():
                            if entries[last_addr] == "exist":
                                cause = miss_cause.LATE_PREFETCH
                            else:
                                cause = miss_cause.METADATA_LOSS
                        else:
                            cause = miss_cause.UNSEEN_CORRELATION
                    else:
                        cause = miss_cause.UNSEEN_CORRELATION
                if warmed:
                    counters[cause.name] += 1

                ####
                if last_addr == 0:
                    last_addr_is_0.add(addr)
                else:
                    last_addr_is_0.discard(addr)
                if last_addr == addr:
                    last_addr_is_addr.add(addr)
                else:
                    last_addr_is_addr.discard(addr)
                real_last[ip] = addr
                ####
                #logs.append(miss_log(int(lst[0]), lst[2], int(lst[3], 16), int(lst[4], 16), int(lst[5],16), [int(x, 16) for x in lst[6:]]))
                if args.print:
                    full_log.write(f"[{lst[0]:^12}] MISS ({cause.name}, {late}, PC = {ip:#x}, access = {addr:#x}, last access = {last_addr:#x}, exist triggers: {[f'{x:#x}' for x in triggers]})\n")
            elif lst[1] == "ADD":
                add+=1
                target = int(lst[3],16)
                trigger = int(lst[2],16)
                entries = md_targets.setdefault(target, {})
                entries[trigger] = "exist"
                #logs.append(add_log(int(lst[0]), int(lst[2],16), int(lst[3],16)))
                if args.print:
                    full_log.write(f"[{lst[0]:^12}] ADD ({trigger:#x} -> {target:#x})\n")
            elif lst[1] == "EVICT":
                evict += 1
                trigger = int(lst[3],16)
                target = int(lst[4],16)
                reason = lst[2]
                entries = md_targets.get(target)
                entries[trigger] = reason
                #logs.append(evict_log(int(lst[0]), lst[2], int(lst[3],16), int(lst[4],16)))
                if args.print:
                    full_log.write(f"[{lst[0]:^12}] EVICT ({trigger:#x} -> {target:#x}, {reason})\n")
            elif lst[0] == "WARMUP":
                warmed = True
                if args.print:
                    full_log.write(f"WARMUP DONE\n")
            elif lst[1] == "ISSUE":
                status = lst[6]
                issue_status[int(lst[5],16)] = (status, demand_timestamp, lst[0]) # merge/drop/success

                if args.print:
                    full_log.write(f"[{lst[0]:^12}] ISSUE ({lst[2]}, PC = {int(lst[3],16):#x}, trigger = {int(lst[4],16):#x}: issue {int(lst[5],16):#x})\n")
                    # full_log.write(f"[{lst[0]:^12}] ISSUE ({lst[2]}, PC = {int(lst[3],16):#x}, trigger = {int(lst[4],16):#x}: issue {int(lst[5],16):#x}, LA = {lst[6]}, {lst[7]}, PQ_index = {lst[8]}, DG = {lst[9]})\n")
            elif lst[1] == "CACHEFILL":
                evicted_addr = int(lst[3],16)
                last_evict_timestamp[evicted_addr] = lst[0]
                if args.print:
                    full_log.write(f"[{lst[0]:^12}] CACHEFILL (access = {int(lst[2],16):#x}, evicted = {int(lst[3],16):#x}, prefetch = {lst[4]})\n")
    with open(f"{RESULT_PATH}/data/{args.prefetcher}/{t}_breif.txt", "w") as f:
        for k,v in counters.items():
            f.write(f"{k} {v}\n")
       

max_workers = min(500,len(working_queue))
if max_workers != 0:
    with ProcessPoolExecutor(max_workers = max_workers) as executor:
        futures = [executor.submit(analyze_one, x, y) for x, y in working_queue]
        for f in tqdm(as_completed(futures), total=len(futures)):
            pass
    print("Done.")


trace_all=[]
with open("./utils/tracelist", "r") as f:
    lines = f.readlines()
    for line in lines:
        traces = line.split(":", 1)[1].strip().split()
        trace_all.extend(traces)
print(trace_all)

result = {a:{} for a in trace_all}
n = 0
for path in glob.glob(f"./result/data/{args.prefetcher}/*.txt"):

    name = os.path.splitext(os.path.basename(path))[0].replace("_breif", "")  # xxx.txt -> xxx
    if name == "ipc":
        continue
    data = {}

    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            key, value = line.split()
            if key in ["AVG_DISTANCE", "MIN_DISTANCE", "MAX_DISTANCE"]:
                continue
            data[key] = int(value)
    n+=1
    result[name] = data

with open(f"./result/data/{args.prefetcher}/result.json", "w") as f:
    json.dump(result, f, indent=2)

print("Generated result.json, total analyzed traces:", n)


