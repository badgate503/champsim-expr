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

parser.add_argument("--config", "-c", help="指定 champsim_config.json 文件路径，默认为 ../champsim_config.json", default="config")
parser.add_argument("--ncore", "-n", help="numcore", default="1")
parser.add_argument("--mode", "-m", choices=["ipc", "missclass"], required=True, help="选择是否输出 miss cause classification 日志")
parser.add_argument("--debug", "-d", choices=["default", "asan"], help="选择编译模式，Default 为开启 -g -O0; Asan 为开启 AddressSanitizer")
parser.add_argument("--prefetcher", "-p" , required=True, help="选用的预取器名称")
parser.add_argument("--exename", "-e", help="指定编译输出的可执行文件名称")
parser.add_argument("--flag","-f", nargs="*")
args = parser.parse_args()

extra_cflags=[]
extra_ldflags=[]
import json

def diff_json(j1, j2, path=""):
    diffs = []

    # key 集合
    keys = set(j1.keys()) | set(j2.keys())

    for k in keys:
        p = f"{path}.{k}" if path else k

        if k not in j1:
            diffs.append(f"[ONLY IN j2] {p}: {j2[k]}")
        elif k not in j2:
            diffs.append(f"[ONLY IN j1] {p}: {j1[k]}")
        else:
            v1, v2 = j1[k], j2[k]

            # 如果都是 dict → 递归
            if isinstance(v1, dict) and isinstance(v2, dict):
                diffs.extend(diff_json(v1, v2, p))

            # 如果都是 list → 简单比较（也可扩展）
            elif isinstance(v1, list) and isinstance(v2, list):
                if v1 != v2:
                    diffs.append(f"[DIFF LIST] {p}: {v1} != {v2}")

            # 普通值
            else:
                if v1 != v2:
                    if p != "executable_name" and p != "L2C.prefetcher":
                        diffs.append(f"[DIFF] {GREEN}{p}: {v1} != {v2}{END}")

    return diffs

if args.config:
    with open(f"../champsim_{args.config}.json") as f1, open("../champsim_config.json") as f2:
        j1, j2 = json.load(f1), json.load(f2)
        differences = diff_json(j1, j2)
        if differences:
            print(f"{YELLOW}Warning: The following differences were found between champsim_{args.config}.json and champsim_config.json:{END}")
            for diff in differences:

                print(diff)
      
                   

if args.debug != None:
    if args.debug == "default":
        extra_cflags.extend(["-g", "-O2"])
    elif args.debug == "asan":
        extra_cflags.extend(["-fsanitize=address", "-g", "-O0"])
        extra_ldflags.append("-fsanitize=address")

if args.mode == "missclass":
    extra_cflags.append("-DMISS_CLASS_LOG")

if args.prefetcher == "prophet":
    if args.exename is not None and "profile" in args.exename:
        extra_cflags.append("-DPROFILE")

if args.flag:
    for f in args.flag:
        extra_cflags.append(f"-D{f}")

with open(f"../champsim_{args.config}.json", "r", encoding="utf-8") as f:
    data = json.load(f)
    data["L2C"]["prefetcher"] = args.prefetcher
    if args.prefetcher == "mjtp":
        args.prefetcher += f".{args.mj}"
    if args.mode == "missclass":
        data["executable_name"] = f"{args.prefetcher}.mc"
    else:
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
with open(f"../champsim_{args.config}.json", "w", encoding="utf-8") as f:
    json.dump(data, f, ensure_ascii=False, indent=4)

print("=============== Compiling ================")
print(f"Executable: {data['executable_name']}")
print(f"CXXFLAGS: {' '.join(extra_cflags)}")
print(f"LDFLAGS: {' '.join(extra_ldflags)}")
print("Compiling...")

os.system("cd .. && make clean")
ret = os.system(f"cd .. && ./config.sh champsim_{args.config}.json")

import subprocess
import sys

try:
    result = subprocess.run(
         ["make",
        f"CXXFLAGS={' '.join(extra_cflags)}",
        f"LDFLAGS={' '.join(extra_ldflags)}",
        "-j256"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd="..",
        text=True,
        check=True
    )
    os.system(f"cp ../champsim_{args.config}.json ../bin/champsim_config_{data['executable_name']}.json")
    print(f"{GREEN}编译完成{END}")

except subprocess.CalledProcessError as e:
    print(f"{RED}编译错误，输出到：error_{args.prefetcher}.log{END}")

    with open(f"error_{args.prefetcher}.log", "w") as f:
        f.write("Return code: {}\n".format(e.returncode))
        f.write("\n=== STDOUT ===\n")
        f.write(e.stdout or "")
        f.write("\n=== STDERR ===\n")
        f.write(e.stderr or "")
    with open(f"error_{args.prefetcher}.log", "r") as f:
        for lineno, line in enumerate(f, 1):
            if "error:" in line:
                print(f"{line.rstrip()}")

    sys.exit(e.returncode)