#!/usr/bin/env python3
import os
import sys 
import argparse
from itertools import product
import json
import time
import random
import subprocess

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

parser.add_argument("--config", "-c", help="Specify the champsim config file, e.g., config_dram2400 -> './champsim_config/champsim_config_dram2400.json'", default="config")
parser.add_argument("--prefetcher", "-p" , required=True, help="Specify the prefetcher to use in L2 Cache, must be implemented in PRISM/prefetcher")
parser.add_argument("--exename", "-e", help="Specify the name of the compiled executable file, default to the prefetcher name")
parser.add_argument("--ncore", "-n", help="Specify the numcore of the simulator", default="1")
parser.add_argument("--flag","-f", nargs="*", help="Extra compile flags")
args = parser.parse_args()

extra_cflags=[]
extra_ldflags=[]
import json

def diff_json(j1, j2, path=""):
    diffs = []
    keys = set(j1.keys()) | set(j2.keys())
    for k in keys:
        p = f"{path}.{k}" if path else k
        if k not in j1:
            diffs.append(f"[ONLY IN j2] {p}: {j2[k]}")
        elif k not in j2:
            diffs.append(f"[ONLY IN j1] {p}: {j1[k]}")
        else:
            v1, v2 = j1[k], j2[k]
            if isinstance(v1, dict) and isinstance(v2, dict):
                diffs.extend(diff_json(v1, v2, p))
            elif isinstance(v1, list) and isinstance(v2, list):
                if v1 != v2:
                    diffs.append(f"[DIFF LIST] {p}: {v1} != {v2}")
            else:
                if v1 != v2:
                    if p != "executable_name" and p != "L2C.prefetcher":
                        diffs.append(f"[DIFF] {GREEN}{p}: {v1} != {v2}{END}")
                        
    return diffs

if args.config:
    with open(f"./champsim_config/champsim_{args.config}.json") as f1, open("./champsim_config/champsim_config.json") as f2:
        j1, j2 = json.load(f1), json.load(f2)
        differences = diff_json(j1, j2)
        if differences:
            print(f"{YELLOW}Warning: The following differences were found between champsim_{args.config}.json and champsim_config.json:{END}")
            for diff in differences:
                print(diff)

if args.prefetcher == "prophet":
    if args.exename is not None and "profile" in args.exename:
        extra_cflags.append("-DPROFILE")

if args.flag:
    for f in args.flag:
        extra_cflags.append(f"-D{f}")

with open(f"./champsim_config/champsim_{args.config}.json", "r", encoding="utf-8") as f:
    data = json.load(f)
    data["L2C"]["prefetcher"] = args.prefetcher
    data["executable_name"] = f"{args.prefetcher}"
    if args.exename is not None:
        data["executable_name"] = args.exename
    data["num_cores"] = int(args.ncore)
    if int(args.ncore) == 1:
        data["physical_memory"]["channels"] = 1
        data["physical_memory"]["ranks"] = 1
    elif int(args.ncore) == 2:
        data["physical_memory"]["channels"] = 2
        data["physical_memory"]["ranks"] = 1
    elif int(args.ncore) == 4:
        data["physical_memory"]["channels"] = 2
        data["physical_memory"]["ranks"] = 2
    elif int(args.ncore) == 8:
        data["physical_memory"]["channels"] = 4
        data["physical_memory"]["ranks"] = 2
with open(f"./champsim_config/champsim_{args.config}.json", "w", encoding="utf-8") as f:
    json.dump(data, f, ensure_ascii=False, indent=4)

print("=============== Compiling ================")
print(f"Executable: {data['executable_name']}")
print(f"CXXFLAGS: {' '.join(extra_cflags)}")
print(f"LDFLAGS: {' '.join(extra_ldflags)}")
print("Compiling...")

ret = os.system(f"cd .. && ./config.sh ./scripts/champsim_config/champsim_{args.config}.json")
os.system("cd .. && make clean")

try:
    result = subprocess.run(
         ["make",
        f"CXXFLAGS={' '.join(extra_cflags)}",
        f"LDFLAGS={' '.join(extra_ldflags)}",
        "-j32"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd="..",
        text=True,
        check=True
    )
    os.system(f"cp ./champsim_config/champsim_{args.config}.json ../bin/champsim_config_{data['executable_name']}.json")
    print(f"{GREEN}Compile successful{END}\n")

except subprocess.CalledProcessError as e:
    print(f"{RED}Compile error, output to: error_{args.prefetcher}.log{END}")
    os.system("mkdir -p comp_errors")
    with open(f"comp_errors/error_{args.prefetcher}.log", "w") as f:
        f.write("Return code: {}\n".format(e.returncode))
        f.write("\n=== STDOUT ===\n")
        f.write(e.stdout or "")
        f.write("\n=== STDERR ===\n")
        f.write(e.stderr or "")
    with open(f"comp_errors/error_{args.prefetcher}.log", "r") as f:
        for lineno, line in enumerate(f, 1):
            if "error:" in line:
                print(f"{line.rstrip()}")

    sys.exit(e.returncode)