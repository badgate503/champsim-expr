#!/usr/bin/env python3
TRACE_PATH = "../../champtraces/"
PREFETCHER_PATH = "../prefetcher/"
LOG_BASE = "./log/"
WARM_UP = 50_000_000
INTERVAL = 200_000_000
MAX_THREAD = 32
running = 0
import os
import sys 
import argparse
from itertools import product
import json
import time
import random
import subprocess
parser = argparse.ArgumentParser()

parser.add_argument("--compile", "-c", action="store_true", help="do compile champsim")
parser.add_argument("--stride","-s",action="store_true",help="l1d stride prefetcher")
parser.add_argument("--list", "-l", action="store_true", help="list trace and prefetcher")
parser.add_argument("--all", "-a", action="store_true", help="run all traces")
parser.add_argument("--traces", "-t", nargs="+")
parser.add_argument("--prefetchers", "-p", nargs="+")

args = parser.parse_args()

# (Trace, Prefetcher, Process, Status)
process_list = []

def issue_work(trace, pref):
    global WARM_UP
    global INTERVAL
    global LOG_BASE
    global process_list
    work = [f"../bin/champsim.{pref}", "--warmup-instructions", f"{WARM_UP}", "--simulation-instructions", f"{INTERVAL}", trace[1]]
    
    #work = ["sleep", f"{random.randint(10, 30)}"]
    with open(f"{LOG_BASE}{trace[0]}/{pref}.log", "w") as f:
        
        f.write(" ".join(work))
        f.write("\n")
        process = subprocess.Popen(
            work,
            stdout=f,
            stderr=f
        )
        process_list.append((trace[0], pref, process, "RUNNING"))
        print(f"NEW Thread: {trace[0]} / {pref}")





all_trace_list = []
all_pf_list  = []
if args.list:
    print("\n- Trace List:")
for root, dirs, files in os.walk(TRACE_PATH):
    for f in files:
        if f.endswith(".xz") or f.endswith(".gz"):
            p = os.path.join(root, f)
            all_trace_list.append((f,p))
            if args.list:
                print(""+f.replace('.champsimtrace.xz',"").replace('.champsimtrace.gz',"")+"", end=" ")
if args.list:
    print("\n- Prefetcher List:")
for f in os.listdir(PREFETCHER_PATH):
    all_pf_list.append(f)
    if args.list:
        print("- "+f)


if args.compile:
    for p in args.prefetchers:
        with open("../champsim_config.json", "r", encoding="utf-8") as f:
            data = json.load(f)
        if args.stride:
            data["L1D"]["prefetcher"] = "ip_stride"
            data["executable_name"] = f"champsim.stride_{p}"
        else:
            data["L1D"]["prefetcher"] = "no"
            data["executable_name"] = f"champsim.{p}"
        data["L2C"]["prefetcher"] = p
        
        with open("../champsim_config.json", "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=4)
        os.system("cd .. && ./config.sh champsim_config.json && make -j32")

trace_paths = []
for trace in args.traces:
    tt = [u for s, u in all_trace_list if s.startswith(trace)]
    trace_paths.append((trace,tt[0]))



total = len(trace_paths) * len(args.prefetchers)


for t in trace_paths:
    #os.system(f"rm -r ./log/{t[0]}")
    os.system(f"mkdir -p ./log/{t[0]}")
    for p in args.prefetchers:
        while True:
            if running < MAX_THREAD:
                na = "stride_"+p if args.stride else p
                issue_work(t,na)
                running += 1
                break
            else:
                print("Sleeping...")
                time.sleep(5)
                for i,l in enumerate(process_list):
                    ret = l[2].poll()
                    if ret is not None and l[3] == "RUNNING":
                        running -= 1
                        process_list[i] = (l[0], l[1], l[2], "TERMINATED")
                
                description = "Trace"
                bb = "PF"
                cc = "Status"
                if not len(process_list) == 0:
                    os.system("clear")
                    print(f"\n|{description:<30}|{bb:<20}|{cc:<10}|")
                for l in process_list:
                    print(f"|{l[0]:<30}|{l[1]:<20}|{l[3]:<10}|")
                print(f"Running {running} / Terminated {total -running} / Total {total}")
                
        
        for i,l in enumerate(process_list):
            ret = l[2].poll()
            if ret is not None and l[3] == "RUNNING":
                running -= 1
                process_list[i] = (l[0], l[1], l[2], "TERMINATED")
        
        description = "Trace"
        bb = "PF"
        cc = "Status"
        if not len(process_list) == 0:
            os.system("clear")
            print(f"\n|{description:<30}|{bb:<20}|{cc:<10}|")
        for l in process_list:
            print(f"|{l[0]:<30}|{l[1]:<20}|{l[3]:<10}|")
        print(f"Running {running} / Terminated {total -running} / Total {total}")
                
    
        



while running > 0:
    for i,l in enumerate(process_list):
        ret = l[2].poll()
        if ret is not None and l[3] == "RUNNING":
            running -= 1
            process_list[i] = (l[0], l[1], l[2], "TERMINATED")
    description = "Trace"
    bb = "PF"
    cc = "Status"
    
    if not len(process_list) == 0:
        os.system("clear")
        print(f"\n|{description:<30}|{bb:<20}|{cc:<10}|")
    for l in process_list:
        print(f"|{l[0]:<30}|{l[1]:<20}|{l[3]:<10}|")
    print(f"Running {running} / Terminated {total -running} / Total {total}")
    print("\nSleeping...")
    time.sleep(10)
