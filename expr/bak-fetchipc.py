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
import glob
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
args = parser.parse_args()

from enum import Enum


ipc_results = []
for log_file in glob.glob(os.path.join(LOG_PATH, args.prefetcher, '*.log')):
    log_file_name = os.path.splitext(os.path.basename(log_file))[0]
    base_file = os.path.join(LOG_PATH, "no", log_file_name+".log")

    print(f"\n> Calculating IPC for {CYAN}{log_file_name}{END}")
    ipc = get_ipc(log_file)
    base_ipc = get_ipc(base_file)
    ipc_results.append((log_file_name, ipc/base_ipc))
    print(f"{CYAN}{log_file_name}{END}: IPC = {GREEN}{ipc:.4f}{END}, Base IPC = {GREEN}{base_ipc:.4f}{END}, IPC improvement = {GREEN}{ipc/base_ipc:.4f}{END}")

print(f"\nWriting IPC Speedup results to {YELLOW}{RESULT_PATH}/data/{args.prefetcher}/ipc{END}, total {len(ipc_results)} entries.")
os.makedirs(f"{RESULT_PATH}/data/{args.prefetcher}", exist_ok=True)
with open(f"{RESULT_PATH}/data/{args.prefetcher}/ipc.txt", "w") as f:
    for log_file, ipc in ipc_results:
        f.write(f"{log_file} {ipc}\n")