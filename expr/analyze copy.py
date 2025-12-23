#!/usr/bin/env python3.11


import os
import sys 
import argparse
from itertools import product
import json
from tqdm import tqdm
import time
import random
import subprocess
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
LOG_PATH="./log/"
parser = argparse.ArgumentParser()
parser.add_argument("--traces", "-t", nargs="+")
parser.add_argument("--print", "-p", action="store_true")
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


class miss_log:
    def __init__(self, timestamp,late,  addr, ip,last_addr, triggers):
        self.timestamp = timestamp
        self.late = late
        self.ip = ip 
        self.addr = addr 
        self.last_addr = last_addr
        self.triggers = triggers
        self.mcause = miss_cause.OTHER
    def __str__(self):
        return f"[{self.timestamp:^12}] MISS ({self.mcause.name}, PC = {self.ip:#x}, access = {self.addr:#x}, last access = {self.last_addr:#x}, late PF: {self.late}, exist triggers: {[f'{x:#x}' for x in self.triggers]})"

class add_log:
    def __init__(self, timestamp, trigger, target):
        self.timestamp = timestamp
        self.trigger = trigger 
        self.target = target 
    def __str__(self):
        return f"[{self.timestamp:^12}] ADD ({self.trigger:#x} -> {self.target:#x})"

class evict_log:
    def __init__(self, timestamp, reason, trigger, target):
        self.timestamp = timestamp
        self.reason = reason
        self.trigger = trigger 
        self.target = target 
    def __str__(self):
        return f"[{self.timestamp:^12}] EVICT ({self.trigger:#x} -> {self.target:#x}, due to {self.reason})"

class issue_log:
    def __init__(self, timestamp, where, ip, trigger, addr):
        self.timestamp = timestamp
        self.where = where
        self.ip = ip
        self.trigger = trigger 
        self.addr = addr 
    def __str__(self):
        return f"[{self.timestamp:^12}] ISSUE ({self.where}, PC = {self.ip:#x}, trigger = {self.trigger:#x}: issue {self.addr:#x})"

class hit_log:
    def __init__(self, timestamp,  addr, ip,last_addr, triggers):
        self.timestamp = timestamp
        self.ip = ip 
        self.addr = addr 
        self.last_addr = last_addr
        self.triggers = triggers
    def __str__(self):
        return f"[{self.timestamp:^12}] HIT (PC = {self.ip:#x}, access = {self.addr:#x}, last access = {self.last_addr:#x}, exist triggers: {[f'{x:#x}' for x in self.triggers]})"








for t in args.traces:
    logs = []
    add = 0
    evict = 0
    issue = 0
    miss_mshr = 0
    miss_l2pq = 0
    miss_l3pq = 0
    miss_nolate = 0
    with open(LOG_PATH+t+".txt") as f:
        for line in tqdm(f):
            lst = line.strip().split(" ")
            if lst[1] == "HIT":
                logs.append(hit_log(int(lst[0]), int(lst[2], 16), int(lst[3], 16), int(lst[4],16), [int(x, 16) for x in lst[5:]]))
            if lst[1] == "MISS":
                if lst[2] == "MSHR":
                    miss_mshr +=1
                elif lst[2] == "L2PQ":
                    miss_l2pq += 1
                elif lst[2] == "L3PQ":
                    miss_l3pq += 1
                else:
                    miss_nolate += 1
                logs.append(miss_log(int(lst[0]), lst[2], int(lst[3], 16), int(lst[4], 16), int(lst[5],16), [int(x, 16) for x in lst[6:]]))
            elif lst[1] == "ADD":
                add+=1
                logs.append(add_log(int(lst[0]), int(lst[2],16), int(lst[3],16)))
            elif lst[1] == "EVICT":
                evict += 1
                logs.append(evict_log(int(lst[0]), lst[2], int(lst[3],16), int(lst[4],16)))
            # elif lst[1] == "ISSUE":
            #     issue += 1
            #     logs.append(issue_log(int(lst[0]), lst[2], int(lst[3],16), int(lst[4],16), int(lst[5],16)))
    

    print(f"Miss(No late prefetch): {miss_nolate}, Miss(PR send to MSHR): {miss_mshr}, Miss(PR send to L2 PQ): {miss_l2pq}, Miss(PR send to L3 PQ): {miss_l3pq}, Add: {add}, Evict: {evict}")
    print(len(logs))
    misses = set()
    md_targets = {}

    #metadata = {}
    i = 0
    counters = {cause.name:0 for cause in miss_cause}

    #####
    last_addr_is_0 = set()
    last_addr_is_addr = set()
    ####
    for l in tqdm(logs):
        match l:
            case miss_log():
                if l.addr not in misses:
                    misses.add(l.addr)
                    cause = miss_cause.TARGET_FIRST_APPEAR
                elif l.late != "NO":
                    cause = miss_cause.PF_TOO_LATE
                elif l.triggers:
                    if l.last_addr in l.triggers:
                        cause = miss_cause.PF_TOO_EARLY
                    else:
                        cause = miss_cause.NO_TRIGGER
                elif l.addr not in md_targets.keys():

                    ####
                    if l.addr in last_addr_is_0:
                        cause = miss_cause.NO_MD_LAST_ADDR_0
                    elif l.addr in last_addr_is_addr:
                        cause = miss_cause.NO_MD_LAST_ADDR_REPEAT
                    ####
                elif l.last_addr not in md_targets[l.addr].keys():
                    cause = miss_cause.EVICTED_NO_TRIGGER
                else:
                    cause = miss_cause.EVICTED_MD_CAPACITY if md_targets[l.addr][l.last_addr] == "CAPACITY" else miss_cause.EVICTED_MD_CONFLICT
                l.mcause = cause
                counters[cause.name] += 1

                ####
                if l.last_addr == 0:
                    last_addr_is_0.add(l.addr)
                else:
                    last_addr_is_0.discard(l.addr)
                if l.last_addr == l.addr:
                    last_addr_is_addr.add(l.addr)
                else:
                    last_addr_is_addr.discard(l.addr)
                ####
            case hit_log():
                if l.last_addr == 0:
                    last_addr_is_0.add(l.addr)
                else:
                    last_addr_is_0.discard(l.addr)
                if l.last_addr == l.addr:
                    last_addr_is_addr.add(l.addr)
                else:
                    last_addr_is_addr.discard(l.addr)

            case add_log():
                entries = md_targets.setdefault(l.target, {})
                entries[l.trigger] = "exist"
                        
            case evict_log():
                # On eviction, the (trigger,target) should exist; set its
                # status to the eviction reason (e.g., 'CAPACITY' or 'CONFLICT').
                entries = md_targets.get(l.target)
                entries[l.trigger] = l.reason
    print(counters)
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

    for (name, count, pct), pct_s in zip(rows, pct_strs):
        int_part, frac_part = pct_s.split('.')
        int_part_padded = int_part.rjust(max_int_len)
        pct_display = f"{int_part_padded}.{frac_part}%"
        print(f"{name:<30}: {pct_display}")


    print(len(logs))
    if args.print:
        with open(f"./result/{t}_full.txt", "w") as f:
            for l in logs:
                f.write(l.__str__())
                f.write("\n")
    with open(f"./result/{t}_breif.txt", "w") as f:
        for k,v in counters.items():
            f.write(f"{k} {v}\n")