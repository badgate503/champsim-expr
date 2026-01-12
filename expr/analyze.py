#!/usr/bin/env python3.11

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
    TARGET_FIRST_APPEAR = 1
    PF_TOO_LATE = 2
    PF_TOO_EARLY = 3
    EVICTED_MD_CAPACITY = 4
    EVICTED_MD_CONFLICT = 5
    NO_MD_LAST_ADDR_0 = 6
    
    NO_TRIGGER = 7
    #EVICTED_NO_TRIGGER = 8
    OTHER = 9

    NO_MD_LAST_ADDR_REPEAT = 11


import glob
already_analyzed = []
for result in glob.glob(os.path.join(f"{RESULT_PATH}/data/{args.prefetcher}",f"*_breif.txt")):
    t = os.path.splitext(os.path.basename(result))[0]
    already_analyzed.append(t.replace(f"_breif",""))
print(f"Already analyzed {len(already_analyzed)} traces: {already_analyzed}")



all_counters = {}
print(LOG_PATH)

for log_file in glob.glob(os.path.join(LOG_PATH, args.prefetcher, '*.txt')):
    
    t = os.path.splitext(os.path.basename(log_file))[0]
    if not args.traces:
        args.traces = []
    if t in already_analyzed and t not in args.traces:
        print(f"\n> Skipping {CYAN}{t}{END}")
        continue
    print(f"\n> Analyzing {CYAN}{t}{END}")
    
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
    print(f"{CYAN}{t}{END}: Reading logs from {YELLOW}{log_file}{END}")
    with open(log_file) as f:
        for line in tqdm(f):
            lst = line.strip().split(" ")
            if lst[1] == "HIT":
                addr = int(lst[2], 16)
                ip = int(lst[3], 16)
                last_addr = int(lst[4], 16)
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

            if lst[1] == "MISS":
                miss += 1
                late = lst[2]
                addr = int(lst[3], 16)
                ip = int(lst[4], 16)
                last_addr = int(lst[5], 16)
                triggers = [int(x, 16) for x in lst[6:]]

                if addr not in misses:
                    misses.add(addr)
                    cause = miss_cause.TARGET_FIRST_APPEAR
                elif late != "NO":
                    cause = miss_cause.PF_TOO_LATE
                else:
                    entries = md_targets.get(addr)
                    if entries:
                        if last_addr in entries.keys():   # YES
                            if entries[last_addr] == "exist":
                                cause = miss_cause.PF_TOO_EARLY
                            elif entries[last_addr] == "CAPACITY":
                                cause = miss_cause.EVICTED_MD_CAPACITY
                            elif entries[last_addr] == "CONFLICT":
                                cause = miss_cause.EVICTED_MD_CONFLICT
                        else:
                            cause = miss_cause.NO_TRIGGER
                    else:
                        if addr in last_addr_is_0:
                            cause = miss_cause.NO_MD_LAST_ADDR_0
                        elif addr in last_addr_is_addr:
                            cause = miss_cause.NO_MD_LAST_ADDR_REPEAT
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
                ####
                #logs.append(miss_log(int(lst[0]), lst[2], int(lst[3], 16), int(lst[4], 16), int(lst[5],16), [int(x, 16) for x in lst[6:]]))
            elif lst[1] == "ADD":
                add+=1
                target = int(lst[3],16)
                trigger = int(lst[2],16)
                entries = md_targets.setdefault(target, {})
                entries[trigger] = "exist"
                #logs.append(add_log(int(lst[0]), int(lst[2],16), int(lst[3],16)))
            elif lst[1] == "EVICT":
                evict += 1
                trigger = int(lst[3],16)
                target = int(lst[4],16)
                reason = lst[2]
                entries = md_targets.get(target)
                entries[trigger] = reason
                #logs.append(evict_log(int(lst[0]), lst[2], int(lst[3],16), int(lst[4],16)))
            elif lst[0] == "WARMUP":
                warmed = True
            # elif lst[1] == "ISSUE":
            #     issue += 1
            #     logs.append(issue_log(int(lst[0]), lst[2], int(lst[3],16), int(lst[4],16), int(lst[5],16)))
    

    print(f"{CYAN}{t}{END}: Miss: {miss}, Add: {add}, Evict: {evict}")

    
    # ####
    # for l in tqdm(logs):
    #     match l:
    #         case miss_log():
    #             if l.addr not in misses:
    #                 misses.add(l.addr)
    #                 cause = miss_cause.TARGET_FIRST_APPEAR
    #             elif l.late != "NO":
    #                 cause = miss_cause.PF_TOO_LATE
    #             else:
    #                 entries = md_targets.get(l.addr)
    #                 if entries:
    #                     if l.last_addr in entries.keys():   # YES
    #                         if entries[l.last_addr] == "exist":
    #                             cause = miss_cause.PF_TOO_EARLY
    #                         elif entries[l.last_addr] == "CAPACITY":
    #                             cause = miss_cause.EVICTED_MD_CAPACITY
    #                         elif entries[l.last_addr] == "CONFLICT":
    #                             cause = miss_cause.EVICTED_MD_CONFLICT
    #                     else:
    #                         cause = miss_cause.NO_TRIGGER
    #                 else:
    #                     if l.addr in last_addr_is_0:
    #                         cause = miss_cause.NO_MD_LAST_ADDR_0
    #                     elif l.addr in last_addr_is_addr:
    #                         cause = miss_cause.NO_MD_LAST_ADDR_REPEAT
    #             l.mcause = cause
    #             if warmed:
    #                 counters[cause.name] += 1

    #             ####
    #             if l.last_addr == 0:
    #                 last_addr_is_0.add(l.addr)
    #             else:
    #                 last_addr_is_0.discard(l.addr)
    #             if l.last_addr == l.addr:
    #                 last_addr_is_addr.add(l.addr)
    #             else:
    #                 last_addr_is_addr.discard(l.addr)
    #             ####
    #         case hit_log():
    #             if l.last_addr == 0:
    #                 last_addr_is_0.add(l.addr)
    #             else:
    #                 last_addr_is_0.discard(l.addr)
    #             if l.last_addr == l.addr:
    #                 last_addr_is_addr.add(l.addr)
    #             else:
    #                 last_addr_is_addr.discard(l.addr)

    #         case add_log():
    #             entries = md_targets.setdefault(l.target, {})
    #             entries[l.trigger] = "exist"
                        
    #         case evict_log():
    #             entries = md_targets.get(l.target)
    #             entries[l.trigger] = l.reason

    #         case warmup_done_log():
    #             warmed = True
    s = sum(counters.values())
    # Build rows and sort by percentage descending
    rows = []
    for u, v in counters.items():
        pct = (v / s * 100.0) if s > 0 else 0.0
        rows.append((u, v, pct))
    rows.sort(key=lambda x: x[2], reverse=True)

    # Prepare percentage strings with two decimals, then align decimal points
    pct_strs = [f"{r[2]:.2f}" for r in rows]
    # find max integer part length for alignment
    int_parts = [s_.split(".")[0].lstrip("-") for s_ in pct_strs]
    max_int_len = max((len(x) for x in int_parts), default=1)
    print(f"\nResult of {CYAN}{t}{END}: ")
    for (name, count, pct), pct_s in zip(rows, pct_strs):
        int_part, frac_part = pct_s.split('.')
        int_part_padded = int_part.rjust(max_int_len)
        pct_display = f"{int_part_padded}.{frac_part}%"
        print(f"{name:<30}: {pct_display}")


    # if args.print:
    #     with open(f"{RESULT_PATH}/data/{args.prefetcher}/{t}_full.txt", "w") as f:
    #         for l in logs:
    #             f.write(l.__str__())
    #             f.write("\n")
    with open(f"{RESULT_PATH}/data/{args.prefetcher}/{t}_breif.txt", "w") as f:
        for k,v in counters.items():
            f.write(f"{k} {v}\n")
    all_counters.update({t: counters})
    print(f"\n{CYAN}{t}{END}: Done.")

result = {}
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
            data[key] = int(value)
    n+=1
    result[name] = data

with open(f"./result/data/{args.prefetcher}/result.json", "w") as f:
    json.dump(result, f, indent=2)

print("Generated result.json, total analyzed traces:", n)
# if args.json:
#     json_result = json.dumps(all_counters)
#     with open(f"{RESULT_PATH}/data/{args.prefetcher}/result.json", "w") as f:
#         f.write(json_result)

