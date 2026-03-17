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
parser.add_argument("--tuplenum", "-n", type=int, default=3)
args = parser.parse_args()
tuple_num = args.tuplenum
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


#all_counters = {}
print(LOG_PATH)
NPROC = 89
working_queue = []
for log_file in glob.glob(os.path.join(LOG_PATH, args.prefetcher, '*.txt')):
    
    t = os.path.splitext(os.path.basename(log_file))[0]
    
    if args.traces and t in args.traces:
        working_queue.append((t,log_file))
    elif not args.traces:
        working_queue.append((t,log_file))

from collections import defaultdict, deque



print(f"Analyzing: {' '.join([t for t, _ in working_queue])}")


def analyze_one(t, log_file):
    add = 0
    evict = 0
    issue = 0
    miss = 0
    misses = set()

    history = defaultdict(lambda: deque(maxlen=tuple_num))
    seen = defaultdict(lambda: defaultdict(int))

    reoccurrence = 0

    #####
    last_addr_is_0 = set()
    last_addr_is_addr = set()
    real_last = {}
    #print(f"{CYAN}{t}{END}: Reading logs from {YELLOW}{log_file}{END}")
    with open(log_file) as f:
        for line in f:
            lst = line.strip().split(" ")
            ip = 0
            addr = 0
            if lst[1] == "HIT" or lst[1] == "MISS":
                if lst[1] == "HIT":
                    addr = int(lst[2], 16)
                    ip = int(lst[3], 16)
                
                if lst[1] == "MISS":
                    addr = int(lst[3], 16)
                    ip = int(lst[4], 16)
                
                dq = history[ip]
                dq.append(addr)

                if len(dq) == tuple_num:
                    t = tuple(dq)

                    if seen[ip][t] > 0:
                        reoccurrence += 1

                    seen[ip][t] += 1
    

    return (t,reoccurrence)
    

    #print(f"\n{CYAN}{t}{END}: Done.")
results = []
max_workers = min(90,len(working_queue))
import csv
# with open(f"tuple_reocc.csv", "w") as f:
#     f.write("tuple_num,reoccurrence\n")
    
if max_workers != 0:
    with ProcessPoolExecutor(max_workers = max_workers) as executor:
        futures = [executor.submit(analyze_one, x, y) for x, y in working_queue]
        for f1 in tqdm(as_completed(futures), total=len(futures)):
            t, reocc = f1.result()
            with open(f"tuple_reocc.csv", "a") as f:
                f.write(f"{tuple_num},{reocc}\n")

    print("Done.")



    
    
