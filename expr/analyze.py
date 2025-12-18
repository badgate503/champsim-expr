#!/usr/bin/env python3.11


import os
import sys 
import argparse
from itertools import product
import json
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
args = parser.parse_args()

from enum import Enum

class miss_cause(Enum):
    FIRST_ACCESS = 1
    PF_TOO_LATE = 2
    PF_TOO_EARLY = 3
    MD_EVICTED_CAPACITY = 4
    MD_EVICTED_CONFLICT = 5
    NO_METADATA = 6
    NO_TRIGGER = 7
    OTHER = 8


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
        return f"{RED}MISS{END}({self.timestamp}: PC {self.ip:#x} access {self.addr:#x}, exist triggers {[f'{x:#x}' for x in self.triggers]})"

class add_log:
    def __init__(self, timestamp, trigger, target):
        self.timestamp = timestamp
        self.trigger = trigger 
        self.target = target 
    def __str__(self):
        return f"{CYAN}ADD{END}({self.timestamp}: {self.trigger:#x} -> {self.target:#x})"

class evict_log:
    def __init__(self, timestamp, reason, trigger, target):
        self.timestamp = timestamp
        self.reason = reason
        self.trigger = trigger 
        self.target = target 
    def __str__(self):
        return f"{YELLOW}EVICT{END}({self.timestamp}: {self.trigger:#x} -> {self.target:#x}, {self.reason})"



logs = []
add = 0
evict = 0
miss_mshr = 0
miss_l2pq = 0
miss_l3pq = 0
miss_nolate = 0
for t in args.traces:
    with open(LOG_PATH+t+".txt") as f:
        for line in f:
            lst = line.strip().split(" ")
            if lst[1] == "MISS":
                if lst[2] == "MSHR":
                    miss_mshr +=1
                elif lst[2] == "L2PQ":
                    miss_l2pq += 1
                elif lst[2] == "L3PQ":
                    miss_l3pq += 1
                else:
                    miss_nolate += 1
                logs.append(miss_log(int(lst[0]), lst[2] != "NO", int(lst[3], 16), int(lst[4], 16), int(lst[5],16), [int(x, 16) for x in lst[6:]]))
            elif lst[1] == "ADD":
                add+=1
                logs.append(add_log(int(lst[0]), int(lst[2],16), int(lst[3],16)))
            elif lst[1] == "EVICT":
                evict += 1
                logs.append(evict_log(int(lst[0]), lst[2], int(lst[3],16), int(lst[4],16)))
    print(f"Miss(No late prefetch): {miss_nolate}, Miss(PR send to MSHR): {miss_mshr}, Miss(PR send to L2 PQ): {miss_l2pq}, Miss(PR send to L3 PQ): {miss_l3pq}, Add: {add}, Evict: {evict}")

    misses = set()
    md_targets = {}

    metadata = {}
    i = 0
    counters = {cause:0 for cause in miss_cause}
    for l in logs:
        match l:
            case miss_log():
                if l.addr not in misses:
                    misses.add(l.addr)
                    l.mcause = miss_cause.FIRST_ACCESS
                    counters[miss_cause.FIRST_ACCESS] += 1
                elif l.late:
                    l.mcause = miss_cause.PF_TOO_LATE
                    counters[miss_cause.PF_TOO_LATE] += 1
                elif l.triggers:
                    l.mcause = miss_cause.NO_TRIGGER
                    counters[miss_cause.NO_TRIGGER] += 1
                elif l.addr not in md_targets.keys():
                    l.mcause = miss_cause.NO_METADATA
                    counters[miss_cause.NO_METADATA] += 1
                elif l.last_addr not in [x[0] for x in md_targets[l.addr]]:
                    l.mcause = miss_cause.NO_METADATA
                    counters[miss_cause.NO_METADATA] += 1
                else:
                    for uuu in md_targets[l.addr]:
                        if l.last_addr == uuu[0]:
                            if uuu[1] == "CAPACITY":
                                l.mcause = miss_cause.MD_EVICTED_CAPACITY
                                counters[miss_cause.MD_EVICTED_CAPACITY] += 1
                            elif uuu[1] == "CONFLICT":
                                l.mcause = miss_cause.MD_EVICTED_CONFLICT
                                counters[miss_cause.MD_EVICTED_CONFLICT] += 1
                    
            case add_log():
                metadata[l.trigger] = l.target
                md_targets.setdefault(l.target, []).append([l.trigger, "exist"])
            case evict_log():
                del metadata[l.trigger]
                entries = md_targets.get(l.target)
                if entries:
                    for rec in entries:
                        if rec[0] == l.trigger and rec[1] == "exist":
                            rec[1] = l.reason
    print(counters)