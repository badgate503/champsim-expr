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
parser.add_argument("--traces", "-t")
parser.add_argument("--fr" )
parser.add_argument("--to")
args = parser.parse_args()

from enum import Enum

class miss_cause(Enum):
    HIT=1
    
    PF_TOO_LATE = 2
    PF_TOO_EARLY = 3
    EVICTED_MD_CAPACITY = 4
    EVICTED_MD_CONFLICT = 5
    NO_MD_LAST_ADDR_0 = 6
    
    NO_TRIGGER = 7

    NO_MD_LAST_ADDR_REPEAT = 8
    TARGET_FIRST_APPEAR = 9


#all_counters = {}
print(LOG_PATH)
NPROC = 89
working_queue = []

from_path = os.path.join(LOG_PATH, args.fr, f"{args.traces}.txt")
to_path = os.path.join(LOG_PATH, args.to, f"{args.traces}.txt")

def analyze_one(t, log_file, is_from):
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

    #print(f"{CYAN}{t}{END}: Reading logs from {YELLOW}{log_file}{END}")
    with open(log_file) as f:
        with open("from" if is_from else "to", "w") as fout:
            for line in f:
                lst = line.strip().split(" ")
                if lst[1] == "HIT":
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
                    real_last[ip] = addr
                    if warmed:
                        fout.write(f"{addr:#x} HIT\n")
                if lst[1] == "MISS":
                    miss += 1
                    late = lst[2]
                    addr = int(lst[3], 16)
                    ip = int(lst[4], 16)
                    #last_addr = int(lst[5], 16) # from pcTable
                    last_addr = real_last.get(ip) if real_last.get(ip) is not None else 0 # real last addr
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
                        fout.write(f"{addr:#x} {cause.name}\n")
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
                    
                elif lst[1] == "ADD":
                    add+=1
                    target = int(lst[3],16)
                    trigger = int(lst[2],16)
                    entries = md_targets.setdefault(target, {})
                    entries[trigger] = "exist"
        
                elif lst[1] == "EVICT":
                    evict += 1
                    trigger = int(lst[3],16)
                    target = int(lst[4],16)
                    reason = lst[2]
                    entries = md_targets.get(target)
                    entries[trigger] = reason
                
                elif lst[0] == "WARMUP":
                    warmed = True

def sorter(a):
    os.system(f"sort -k1,1 -s {a} > {a}.sorted")

with ProcessPoolExecutor(max_workers = 2) as executor:
    executor.submit(analyze_one, args.traces, from_path, True)
    executor.submit(analyze_one, args.traces, to_path, False)


with ProcessPoolExecutor(max_workers = 2) as executor:
    executor.submit(sorter, "from")
    executor.submit(sorter, "to")



def parse(line):
    addr, cause = line.strip().split(maxsplit=1)
    return addr, cause

from collections import defaultdict
flow = defaultdict(lambda: defaultdict(int))

fa = open("from.sorted")
fb = open("to.sorted")

line_a = fa.readline()
line_b = fb.readline()

only_A = 0
only_B = 0
both   = 0

while line_a or line_b:
    if line_a:
        addr_a, cause_a = parse(line_a)
    if line_b:
        addr_b, cause_b = parse(line_b)

    if line_a and line_b:
        if addr_a == addr_b:
            # 对齐成功（一次）
            both += 1
            line_a = fa.readline()
            line_b = fb.readline()
            flow[cause_a][cause_b] += 1

        elif addr_a < addr_b:
            # A 中多出来的
            only_A += 1
            line_a = fa.readline()
            flow[cause_a]["NOACC"] += 1

        else:  # addr_a > addr_b
            # B 中多出来的
            only_B += 1
            line_b = fb.readline()
            flow["NOACC"][cause_b] += 1
    elif line_a:
        only_A += 1
        line_a = fa.readline()
        flow[cause_a]["NOACC"] += 1
    else:
        only_B += 1
        line_b = fb.readline()
        flow["NOACC"][cause_b] += 1
fa.close()
fb.close()


print(only_A, only_B, both)




flow["HIT"]["HIT"] = 0

import plotly.graph_objects as go

left_nodes  = sorted(flow.keys())
right_nodes = sorted({b for a in flow for b in flow[a]})

labels = [f"{c}_{args.fr}" for c in left_nodes] + [f"{c}_{args.to}" for c in right_nodes]

idx = {label: i for i, label in enumerate(labels)}

sources = []
targets = []
values  = []

for a, targets_dict in flow.items():
    for b, cnt in targets_dict.items():
        sources.append(idx[f"{a}_{args.fr}"])
        targets.append(idx[f"{b}_{args.to}"])
        values.append(cnt)

fig = go.Figure(go.Sankey(

    node=dict(label=labels),
    link=dict(
        source=sources,
        target=targets,
        value=values
    )
))
fig.update_layout(
    font=dict(size=20)
)
fig.update_layout(
    title=f"{args.fr} -> {args.to} : {args.traces}"
)
fig.write_image(f"./result/sankey/{args.traces}_from_{args.fr}_to_{args.to}.png", width=1440, height=2000)
fig.write_html(f"./result/sankey/{args.traces}_from_{args.fr}_to_{args.to}.html")
# os.system("rm from")
# os.system("rm to")
# os.system("rm from.sorted")
# os.system("rm to.sorted")