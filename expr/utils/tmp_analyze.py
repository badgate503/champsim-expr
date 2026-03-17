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



print(f"Analyzing: {' '.join([t for t, _ in working_queue])}")


def analyze_one(t, log_file):
    add = 0
    evict = 0
    issue = 0
    miss = 0
    misses = set()
    md_targets = {}
    md_total_used = 0
    md_total_useful = 0

    # (trigger, target) -> (used?, useful?)
    md_insert_list: dict[tuple[int, int], tuple[bool, bool]] = {}
    issue_list: dict[int, tuple[int, int]] = {}
    metadata = {}
    i = 0
    counters = {cause.name:0 for cause in miss_cause}
    warmed = False
    #####
    last_addr_is_0 = set()
    last_addr_is_addr = set()
    real_last = {}
    #print(f"{CYAN}{t}{END}: Reading logs from {YELLOW}{log_file}{END}")
    with open(log_file) as f:
        if args.print:
            full_log = open(f"{RESULT_PATH}/data/{args.prefetcher}/{t}_full.txt", "w")
        for line in f:
            lst = line.strip().split(" ")
            if lst[1] == "HIT":
                addr = int(lst[2], 16)
                ip = int(lst[3], 16)
                #last_addr = int(lst[4], 16) # from pcTable
                last_addr = real_last.get(ip) if real_last.get(ip) is not None else 0 # real last addr
                triggers = [int(x, 16) for x in lst[5:]]

                
                if addr in issue_list:
                    if issue_list[addr] in md_insert_list:
                        md_insert_list[issue_list[addr]] = (True, True)
                    else:
                        md_total_useful += 1
                if addr in issue_list:
                    del issue_list[addr]

            if lst[1] == "MISS":
                miss += 1
                late = lst[2]
                addr = int(lst[3], 16)
                ip = int(lst[4], 16)
                if addr in issue_list:
                    del issue_list[addr]

            elif lst[1] == "ADD":
                
                add+=1
                target = int(lst[3],16)
                trigger = int(lst[2],16)
                # timestamp, trigger, target, evicted?, useful?

                md_insert_list[(trigger,target)] = (False,False)

               
            elif lst[1] == "EVICT":
                evict += 1
                trigger = int(lst[3],16)
                target = int(lst[4],16)
                reason = lst[2]
                
                used, useful =  md_insert_list[(trigger,target)]
                if useful:
                    md_total_useful += 1
                if used:
                    md_total_used += 1
                del md_insert_list[(trigger,target)]
                

                

            elif lst[0] == "WARMUP":
                warmed = True
         
            elif lst[1] == "ISSUE":
                issue_list[int(lst[5],16)] = (int(lst[4],16), int(lst[5],16))
                _, old = md_insert_list[(int(lst[4],16), int(lst[5],16))]
                md_insert_list[(int(lst[4],16), int(lst[5],16))] = (True, old)

    used_ratio = md_total_used / evict 
    useful_ratio = md_total_useful / evict
    return (t,used_ratio,useful_ratio)
    

    #print(f"\n{CYAN}{t}{END}: Done.")
results = []
max_workers = min(90,len(working_queue))
import csv
with open("ratio_data.csv", "w") as f:
    f.write("workload,used_ratio,useful_ratio\n")
    
if max_workers != 0:
    with ProcessPoolExecutor(max_workers = max_workers) as executor:
        futures = [executor.submit(analyze_one, x, y) for x, y in working_queue]
        for f1 in tqdm(as_completed(futures), total=len(futures)):
            t, used,useful = f1.result()
            with open("ratio_data.csv", "a") as f:
                f.write(f"{t},{used},{useful}\n")

    print("Done.")



    
    
