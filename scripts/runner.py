#!/usr/bin/env python3

import os
import sys 
import argparse
from itertools import product
import json
import time
import random
import multiprocessing as mp
import subprocess
from multiprocessing import Pool
from tqdm import tqdm

from utils.defs import *
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
WARM_UP = 50_000_000
INTERVAL = 200_000_000
parser = argparse.ArgumentParser()


parser.add_argument("--prefetcher", "-p",nargs="+", required=True, help="Name(s) of the prefetcher(s) to run (e.g., baseline triangel prophet prism).")
parser.add_argument("--traces", "-t", nargs="+", help="Name(s) of the trace(s) to run (e.g., bc-0 bc-12).")
parser.add_argument("--tracelist", "-l", nargs="*", type=str, help="Name(s) of the trace Set(s) to run (e.g., ligra gap spec17 ml google).")
parser.add_argument("--output", "-o", help="Directory to redirect experiment results to")
parser.add_argument("--skip", "-s", action="store_true", help="Skip re-running those whose output files already exist; otherwise, overwrite previous results")
parser.add_argument("--nproc", "-j", type=int, default=None, help="Number of parallel jobs to use (default: use all available cores)")
parser.add_argument("--ncore","-n", type=int, default=1, help="Number of cores to use for multicore experiments (default: 8)")

args = parser.parse_args()

trace_path_map = dict()
for root, dirs, files in os.walk(TRACE_PATH):
    for f in files:
        if f.endswith(".xz") or f.endswith(".gz"):
            trace_name = f.replace('.champsimtrace.xz',"").replace('.champsimtrace.gz',"").replace('.champsim.gz',"")
            trace_path_map[trace_name] = os.path.join(root, f)


if not args.traces and not args.tracelist:

    print(f"{RED}No trace assigned. Job done{END}")
    sys.exit(1)

trace_name_set = set()

if args.traces is not None:
    trace_name_set.update(args.traces)
multicore_trace_map = dict()
if args.tracelist is not None and len(args.tracelist) > 0:
    with open("./utils/tracelist", "r") as f:
        lines = f.readlines()
    for prefix in args.tracelist:
        for line in lines:
            if line.startswith(prefix + ":"):
                traces = line.split(":", 1)[1].strip().split()
                trace_name_set.update(traces)
               # print(f"Include {len(traces)} traces from {CYAN}{prefix}{END}: {RED}{' '.join(traces)}{END}\n")

os.makedirs(LOG_PATH, exist_ok=True)
if args.ncore == 1:
    log_path = LOG_PATH
    if args.output is not None:
        log_path = os.path.abspath(args.output)
    task_set = {(p,t,log_path) for p in args.prefetcher for t in trace_name_set}
    #print(f"Prefetchers: {YELLOW}{' '.join(args.prefetcher)}{END}")
    for pp in args.prefetcher:
        pp_basename = pp    
        if os.path.isdir(log_path / f"{pp_basename}"):
            for fname in os.listdir(log_path / f"{pp_basename}"):
                if fname.endswith(".log"):
                    trace_name = fname[:-4]  # remove .log
                    if trace_name in trace_name_set:
                        with open(log_path / f"{pp_basename}" / f"{fname}", "r") as f:
                            if "ChampSim completed all CPUs" in f.read():
                                if args.skip:
                                    task_set.remove((pp, trace_name, log_path))
else:
    print(f"ncore = {args.ncore}")
    trace_mix_set = set()
    with open(f"./utils/sample_{args.ncore}core.csv", "r") as f:
        lines = f.readlines()
        for line in lines:
            parts = line.strip().split(",")
            trace_mix_set.add(parts[0])
            multicore_trace_map[parts[0]] = parts[1:]
    print(f"Included multi-core trace sets from sample_{args.ncore}core.csv: {YELLOW}{' '.join(trace_mix_set)}{END}")
    log_path = LOG_PATH
    if args.output is not None:
        log_path = os.path.abspath(args.output)
    task_set = {(p,t,log_path) for p in args.prefetcher for t in trace_mix_set}

    for pp in args.prefetcher:
        pp_basename = pp    
        if os.path.isdir(log_path / f"core{args.ncore}" / pp_basename):
            for fname in os.listdir(log_path / f"core{args.ncore}" / pp_basename):
                if fname.endswith(".log"):
                    trace_name = fname[:-4]  # remove .log
                    if trace_name in trace_mix_set:
                        
                        with open(log_path / f"core{args.ncore}" / pp_basename / fname, "r") as f:
                            if "ChampSim completed all CPUs" in f.read():
                                if args.skip:
                                    task_set.remove((pp, trace_name, log_path))
                                    print(f"Exclude {YELLOW}{pp}{END}@{RED}{trace_name}{END}")
                            else:
                                print(f"Overwrite {YELLOW}{pp}{END}@{RED}{trace_name}{END}")


task_map = {}

for t in task_set:
    key = t[0]
    task_map.setdefault(key, []).append(t[1])

for pp in args.prefetcher:
    if pp in task_map:
        mode_is_mc = pp.endswith(".mc")
        out_path = f"{LOG_PATH}/{pp}"
        if args.output is not None:
            out_path = os.path.abspath(args.output+f"/{pp}")

        if f"{pp}" not in os.listdir("../bin/"):
            print(f"> {RED}Error: Executable {pp} not found in ../bin/. Please compile first with --compile flag.{END}")
            sys.exit(1)

task_lines = ', '.join([f'{YELLOW}{p}{END}@{RED}{t}{END}' for p, t, log_path in task_set])
nproc = args.nproc if args.nproc is not None else os.cpu_count()
print(f"\nNumber of Champsim tasks: {len(task_set)}. Number of processes: {nproc}")

def launch_task(task):
    p, w, log = task
    if args.ncore == 1:
        w = [w]
    else:
        w = multicore_trace_map[w]
    
    trace_all = [trace_path_map[v] for v in w]
    work = [f"{CHAMPSIM_PATH}/bin/{p}", "--warmup-instructions", f"{WARM_UP}", "--simulation-instructions", f"{INTERVAL}"] + trace_all
    name = f"{w[0]}.log"
    if args.ncore == 1:
        out_path = f"{log}/{p}"
    else:
        out_path = f"{log}/core{args.ncore}/{p}"
    os.makedirs(f"{out_path}/", exist_ok=True)
    with open(f"{out_path}/{name}", "w") as f:
        f.write(" ".join(work))
        f.write("\n\n")
        data = None
        if os.path.exists(f"../bin/champsim_config_{p}.json"):
            with open(f"../bin/champsim_config_{p}.json", "r", encoding="utf-8") as rf:
                data = json.load(rf)
        if data is not None:
            for conf in data['L1D'].items():
                f.write(f"L1D_{conf[0]}:{conf[1]}\n")
            for conf in data['L2C'].items():
                f.write(f"L2C_{conf[0]}:{conf[1]}\n")
            for conf in data['LLC'].items():
                f.write(f"LLC_{conf[0]}:{conf[1]}\n")
            for conf in data['physical_memory'].items():
                f.write(f"DRAM_{conf[0]}:{conf[1]}\n")
        f.write("\n")
        f.flush()
        subprocess.run(
            work,
            stdout=f,
            stderr=f,
            cwd=out_path
        )
    if len(w) == 1:
        return f"{YELLOW}{p}{END}@{RED}{w[0]}{END}"
    else:
        return f"{YELLOW}{p}{END}@{RED}{len(w)}{args.ncore}core-{' '.join(w)}{END}"

if __name__ == '__main__':

    with Pool(processes=nproc) as pool:
        results = []
        pbar = tqdm(
            pool.imap_unordered(launch_task, task_set), 
            total=len(task_set), 
            desc="Running Champsim tasks"
        )
        
        results = []
        for message in pbar:
            pbar.write("Finished: "+message)

    print(f"{len(task_set)} Champsim task completed.")