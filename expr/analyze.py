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
    def __init__(self, timestamp, addr, ip, triggers):
        self.timestamp = timestamp
        self.ip = ip 
        self.addr = addr 
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
miss = 0
for t in args.traces:
    with open(LOG_PATH+t+".txt") as f:
        for line in f:
            lst = line.strip().split(" ")
            if lst[1] == "MISS":
                miss += 1
                logs.append(miss_log(int(lst[0]), int(lst[2], 16), int(lst[3], 16), [int(x, 16) for x in lst[4:]]))
            elif lst[1] == "ADD":
                add+=1
                logs.append(add_log(int(lst[0]), int(lst[2],16), int(lst[3],16)))
            elif lst[1] == "EVICT":
                evict += 1
                logs.append(evict_log(int(lst[0]), lst[2], int(lst[3],16), int(lst[4],16)))
    print(miss, add, evict)

    misses = set()
    # create 16384 independent dicts (avoid using [{}] * n which reuses the same dict)
    metadata = [dict() for _ in range(16384)]
    i = 0
    counters = {cause:0 for cause in miss_cause}
    for l in logs:
        match l:
            case miss_log():
                if l.addr not in misses:
                    misses.add(l.addr)
                    l.mcause = miss_cause.FIRST_ACCESS
                    counters[miss_cause.FIRST_ACCESS] += 1
                    continue  # categorized as first_access
                
            case add_log():
                metadata[l.trigger & 0x3FFF][l.trigger] = l.target
                if len(metadata[l.trigger & 0x3FFF].keys()) > 12:
                    print(list(metadata[l.trigger & 0x3FFF].keys()))
                    assert False
            case evict_log():
                del metadata[l.trigger & 0x3FFF][l.trigger]
            
    print(counters)