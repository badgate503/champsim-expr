#!/usr/bin/env python3


import os
import sys 
import argparse
from itertools import product
import json
import time
import random
import subprocess

WARM_UP = 0
INTERVAL = 250_000_000
TRACE_PATH =  "../../champtraces/"
parser = argparse.ArgumentParser()

parser.add_argument("--compile", "-c", action="store_true", help="do compile champsim")
parser.add_argument("--traces", "-t", nargs="+")
parser.add_argument("--interval", "-i")


args = parser.parse_args()
if args.interval != None:
    INTERVAL = int(args.interval)
all_trace_list = []

for root, dirs, files in os.walk(TRACE_PATH):
    for f in files:
        if f.endswith(".xz") or f.endswith(".gz"):
            if args.traces != None:
                if f.replace('.champsimtrace.xz',"").replace('.champsimtrace.gz',"") in args.traces:
                    p = os.path.join(root, f)
                    all_trace_list.append(p)


if args.compile:
    with open("../champsim_config.json", "r", encoding="utf-8") as f:
        data = json.load(f)
        data["L1D"]["prefetcher"] = "no"
        data["L2C"]["prefetcher"] = "prophet_profile"
        data["LLC"]["prefetcher"] = "no"
        data["executable_name"] = f"champsim.expr"
        

    with open("../champsim_config.json", "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=4)
    os.system("cd .. && ./config.sh champsim_config.json && make -j32")

for w in all_trace_list:
    work = [f"../bin/champsim.expr", "--warmup-instructions", f"{WARM_UP}", "--simulation-instructions", f"{INTERVAL}", w]
    os.system(" ".join(work))