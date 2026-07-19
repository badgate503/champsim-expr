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

parser.add_argument("--figure", "-f", choices=["Fig10", "Fig11", "Fig12", "Fig13","Fig14", "Fig16", "Fig18"] , help="Assign which figure to generate (Only Figure 10, 11, 12, 13, 16 and 18 are available)")
parser.add_argument("--phase", "-p", choices=["Compile", "Run", "Draw", "All"], help="Assign which phase to run, use 'All' to run all phases")

args = parser.parse_args()

executable_list = {
    "Fig10": [
        "baseline",
        "triangel",
        "prophet",
        "prism"
    ],
    "Fig11": [
        "baseline",
        "triangel",
        "prophet",
        "prism"
    ],
    "Fig12": [
        "baseline",
        "triangel",
        "prophet",
        "prism"
    ],
    "Fig13": [
        "baseline",
        "triangel",
        "prophet",
        "prism"
    ],
    "Fig14": [
        "baseline",
        "pcq-1",
        "pcq-2",
        "pcq-4",
        "pcq-8",
        "pcq-12",
        "pcq-16",
        "pat-12k",
        "pat-24k",
        "pat-48k",
        "pat-96k",
        "pat-144k",
        "pat-192k"
    ],
    "Fig16": [
        "l1ipcp.baseline",
        "l1ipcp.triangel",
        "l1ipcp.prophet",
        "l1ipcp.prism",
        "l1berti.baseline",
        "l1berti.triangel",
        "l1berti.prophet",
        "l1berti.prism",
        "dram1200.baseline",
        "dram1200.triangel",
        "dram1200.prophet",
        "dram1200.prism",
        "dram2400.baseline",
        "dram2400.triangel",
        "dram2400.prophet",
        "dram2400.prism",
        "dram3600.baseline",
        "dram3600.triangel",
        "dram3600.prophet",
        "dram3600.prism",
        "dram6000.baseline",
        "dram6000.triangel",
        "dram6000.prophet",
        "dram6000.prism",
        "cache1_2.baseline",
        "cache1_2.triangel",
        "cache1_2.prophet",
        "cache1_2.prism",
        "cache1_4.baseline",
        "cache1_4.triangel",
        "cache1_4.prophet",
        "cache1_4.prism",
        "baseline",
        "triangel",
        "prophet",
        "prism"
    ],
    "Fig18": [
        "baseline",
        "prism-ol-pctp",
        "prism-ol-tgp",
        "prism-ol-bmp",
        "prism-ol-irp",
        "prism-wo-pctp",
        "prism-wo-tgp",
        "prism-wo-bmp",
        "prism-wo-irp",
        "prism"
    ]
}

compile_command = {
    "baseline": f"python3 compiler.py -p baseline",
    "triangel": f"python3 compiler.py -p triangel",
    "prophet": f"python3 compiler.py -p prophet",
    "prism": f"python3 compiler.py -p prism",

    "prism-ol-pctp": f"python3 compiler.py -p prism -f ABLATION_STUDY PC_TRIGGER_PREFETCHING -e prism-ol-pctp",
    "prism-ol-tgp": f"python3 compiler.py -p prism -f ABLATION_STUDY TG_PREFETCHING -e prism-ol-tgp",
    "prism-ol-bmp": f"python3 compiler.py -p prism -f ABLATION_STUDY BMP_RESIZE -e prism-ol-bmp",
    "prism-ol-irp": f"python3 compiler.py -p prism -f ABLATION_STUDY INSERTION_POLICY REPLACEMENT_POLICY -e prism-ol-irp",
    "prism-wo-pctp": f"python3 compiler.py -p prism -f ABLATION_STUDY TG_PREFETCHING BMP_RESIZE INSERTION_POLICY REPLACEMENT_POLICY -e prism-wo-pctp",
    "prism-wo-tgp": f"python3 compiler.py -p prism -f ABLATION_STUDY PC_TRIGGER_PREFETCHING BMP_RESIZE INSERTION_POLICY REPLACEMENT_POLICY -e prism-wo-tgp",
    "prism-wo-bmp": f"python3 compiler.py -p prism -f ABLATION_STUDY PC_TRIGGER_PREFETCHING TG_PREFETCHING INSERTION_POLICY REPLACEMENT_POLICY -e prism-wo-bmp",
    "prism-wo-irp": f"python3 compiler.py -p prism -f ABLATION_STUDY PC_TRIGGER_PREFETCHING TG_PREFETCHING BMP_RESIZE -e prism-wo-irp",

    "l1ipcp.baseline": f"python3 compiler.py -p baseline -c config_ipcp -e l1ipcp.baseline",
    "l1ipcp.triangel": f"python3 compiler.py -p triangel -c config_ipcp -e l1ipcp.triangel",
    "l1ipcp.prophet": f"python3 compiler.py -p prophet -c config_ipcp -e l1ipcp.prophet",
    "l1ipcp.prism": f"python3 compiler.py -p prism -c config_ipcp -e l1ipcp.prism",

    "l1berti.baseline": f"python3 compiler.py -p baseline -c config_berti -e l1berti.baseline",
    "l1berti.triangel": f"python3 compiler.py -p triangel -c config_berti -e l1berti.triangel",
    "l1berti.prophet": f"python3 compiler.py -p prophet -c config_berti -e l1berti.prophet",
    "l1berti.prism": f"python3 compiler.py -p prism -c config_berti -e l1berti.prism",

    "dram1200.baseline": f"python3 compiler.py -p baseline -c config_dram1200 -e dram1200.baseline",
    "dram1200.triangel": f"python3 compiler.py -p triangel -c config_dram1200 -e dram1200.triangel",
    "dram1200.prophet": f"python3 compiler.py -p prophet -c config_dram1200 -e dram1200.prophet",
    "dram1200.prism": f"python3 compiler.py -p prism -c config_dram1200 -e dram1200.prism",

    "dram2400.baseline": f"python3 compiler.py -p baseline -c config_dram2400 -e dram2400.baseline",
    "dram2400.triangel": f"python3 compiler.py -p triangel -c config_dram2400 -e dram2400.triangel",
    "dram2400.prophet": f"python3 compiler.py -p prophet -c config_dram2400 -e dram2400.prophet",
    "dram2400.prism": f"python3 compiler.py -p prism -c config_dram2400 -e dram2400.prism",

    "dram3600.baseline": f"python3 compiler.py -p baseline -c config_dram3600 -e dram3600.baseline",
    "dram3600.triangel": f"python3 compiler.py -p triangel -c config_dram3600 -e dram3600.triangel",
    "dram3600.prophet": f"python3 compiler.py -p prophet -c config_dram3600 -e dram3600.prophet",
    "dram3600.prism": f"python3 compiler.py -p prism -c config_dram3600 -e dram3600.prism",

    "dram6000.baseline": f"python3 compiler.py -p baseline -c config_dram6000 -e dram6000.baseline",
    "dram6000.triangel": f"python3 compiler.py -p triangel -c config_dram6000 -e dram6000.triangel",
    "dram6000.prophet": f"python3 compiler.py -p prophet -c config_dram6000 -e dram6000.prophet",
    "dram6000.prism": f"python3 compiler.py -p prism -c config_dram6000 -e dram6000.prism",

    "cache1_4.baseline": f"python3 compiler.py -p baseline -c config_cache1_4 -e cache1_4.baseline",
    "cache1_4.triangel": f"python3 compiler.py -p triangel -c config_cache1_4 -e cache1_4.triangel",
    "cache1_4.prophet": f"python3 compiler.py -p prophet -c config_cache1_4 -e cache1_4.prophet",
    "cache1_4.prism": f"python3 compiler.py -p prism -c config_cache1_4 -e cache1_4.prism",

    "cache1_2.baseline": f"python3 compiler.py -p baseline -c config_cache1_2 -f N_LLC_SET=2048 -e cache1_2.baseline",
    "cache1_2.triangel": f"python3 compiler.py -p triangel -c config_cache1_2 -f N_LLC_SET=2048 -e cache1_2.triangel",
    "cache1_2.prophet": f"python3 compiler.py -p prophet -c config_cache1_2 -f N_LLC_SET=2048 -e cache1_2.prophet",
    "cache1_2.prism": f"python3 compiler.py -p prism -c config_cache1_2 -f N_LLC_SET=2048 -e cache1_2.prism",

    "pcq-1": f"python3 compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=1 -e pcq-1",
    "pcq-2": f"python3 compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=2 -e pcq-2",
    "pcq-4": f"python3 compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=4 -e pcq-4",
    "pcq-8": f"python3 compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=8 -e pcq-8",
    "pcq-12": f"python3 compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=12 -e pcq-12",
    "pcq-16": f"python3 compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=16 -e pcq-16",

    "pat-12k": f"python3 compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=12*1024 -e pat-12k",
    "pat-24k": f"python3 compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=24*1024 -e pat-24k",
    "pat-48k": f"python3 compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=48*1024 -e pat-48k",
    "pat-96k": f"python3 compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=96*1024 -e pat-96k",
    "pat-144k": f"python3 compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=144*1024 -e pat-144k",
    "pat-192k": f"python3 compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=192*1024 -e pat-192k"
}

run_trace_list = {
    "Fig10": ["ligra", "gap", "spec17", "ml", "google"],
    "Fig11": ["ligra", "gap", "spec17", "ml", "google"],
    "Fig12": ["ligra", "gap", "spec17", "ml", "google"],
    "Fig13": ["ligra", "gap", "spec17", "ml", "google"],
    "Fig14": ["google"],
    "Fig15": ["ligra", "gap", "spec17", "ml", "google"],
    "Fig16": ["ligra", "gap", "spec17", "ml", "google"],
    "Fig18": ["ligra", "gap", "spec17", "ml", "google"]
}

collect_command = {
    "Fig10": [f"python3 get_result.py -p triangel prophet prism -o {RESULT_PATH}/basic -m IPCI L2C_Accuracy L2C_Timeliness DRAM_Traffic"],
    "Fig11": [f"python3 get_result.py -p triangel prophet prism -o {RESULT_PATH}/basic -m IPCI L2C_Accuracy L2C_Timeliness DRAM_Traffic"],
    "Fig12": [f"python3 energy/energy.py"],
    "Fig13": [f"python3 get_result.py -p triangel prophet prism -o {RESULT_PATH}/metadata -m MT_lookups MT_inserts MT_hits L2C_USEFUL"],
    "Fig14": [f"python3 get_result.py -p pcq-1 pcq-2 pcq-4 pcq-8 pcq-12 pcq-16 -o {RESULT_PATH}/sens_pctp/pcq -m IPCI PCM_accuracy PCM_laterate",
              f"python3 get_result.py -p pat-12k pat-24k pat-48k pat-96k pat-144k pat-192k -o {RESULT_PATH}/sens_pctp/pat -m IPCI PCM_accuracy PCM_laterate"],
    "Fig16": [f"python3 get_result.py -p cache0.5_2.baseline cache1_2.baseline cache1_4.baseline baseline \
                         cache0.5_2.triangel cache1_2.triangel cache1_4.triangel triangel      \
                         cache0.5_2.prophet cache1_2.prophet  cache1_4.prophet  prophet       \
                         cache0.5_2.prism cache1_2.prism    cache1_4.prism    prism         \
                      -a cache0.5_2.baseline cache1_2.baseline cache1_4.baseline cache2_4.baseline \
                         cache0.5_2.triangel cache1_2.triangel cache1_4.triangel cache2_4.triangel \
                         cache0.5_2.prophet cache1_2.prophet  cache1_4.prophet  cache2_4.prophet  \
                         cache0.5_2.prism cache1_2.prism    cache1_4.prism    cache2_4.prism    -o {RESULT_PATH}/sens_configurations/cache -m IPCI",
              f"python3 get_result.py -p dram1200.baseline dram1200.triangel dram1200.prophet dram1200.prism \
                         dram2400.baseline dram2400.triangel dram2400.prophet dram2400.prism \
                         dram3600.baseline dram3600.triangel dram3600.prophet dram3600.prism \
                         baseline     triangel          prophet          prism          \
                         dram6000.baseline dram6000.triangel dram6000.prophet dram6000.prism \
                      -a 1200.baseline 1200.triangel 1200.prophet 1200.prism \
                         2400.baseline 2400.triangel 2400.prophet 2400.prism \
                         3600.baseline 3600.triangel 3600.prophet 3600.prism \
                         4800.baseline 4800.triangel 4800.prophet 4800.prism \
                         6000.baseline 6000.triangel 6000.prophet 6000.prism -o {RESULT_PATH}/sens_configurations/dram -m IPCI",
              f"python3 get_result.py -p l1ipcp.baseline   l1ipcp.triangel   l1ipcp.prophet   l1ipcp.prism  \
                         l1berti.baseline  l1berti.triangel  l1berti.prophet  l1berti.prism \
                         baseline     triangel          prophet          prism   \
                      -a IPCP.baseline   IPCP.triangel   IPCP.prophet   IPCP.prism  \
                         Berti.baseline  Berti.triangel  Berti.prophet  Berti.prism \
                         Stride.baseline Stride.triangel Stride.prophet Stride.prism  -o {RESULT_PATH}/sens_configurations/l1spatial -m IPCI"],
    "Fig18": [f"python3 get_result.py -p baseline prism-ol-pctp prism-ol-tgp prism-ol-bmp prism-ol-irp prism prism-wo-pctp prism-wo-tgp prism-wo-bmp prism-wo-irp \
                      -a prism-none prism-ol-pctp prism-ol-tgp prism-ol-bmp prism-ol-irp prism prism-wo-pctp prism-wo-tgp prism-wo-bmp prism-wo-irp -o {RESULT_PATH}/ablation -m IPCI"]
}

required_executables = executable_list[args.figure]

os.makedirs(f"{CHAMPSIM_PATH}/bin", exist_ok=True)


if args.phase == "Compile" or args.phase == "All":
    for exe in required_executables:
        if exe not in os.listdir(f"{CHAMPSIM_PATH}/bin"):
            print(f"Executable {UNDERLINE}{exe}{END}: Compiling with {YELLOW}{compile_command[exe]}{END} ...")
            subprocess.run(compile_command[exe], shell=True)

    for exe in required_executables:
        if exe not in os.listdir(f"{CHAMPSIM_PATH}/bin"):
            print(f"")
            print(f"Aborted: {RED}Executable {exe} not found, possibly due to a compilation failure{END}")
            exit(0)
        else:
            print(f"Executable {UNDERLINE}{exe}{END}: Found at {CHAMPSIM_PATH}/bin/{exe}")

if args.phase == "Compile":
    print("Goodbye")
    exit(0)

trace_name_set = set()

with open("./utils/tracelist", "r") as f:
    lines = f.readlines()
for prefix in run_trace_list[args.figure]:
    for line in lines:
        if line.startswith(prefix + ":"):
            traces = line.split(":", 1)[1].strip().split()
            trace_name_set.update(traces)
print(trace_name_set)

from pathlib import Path
os.makedirs(LOG_PATH, exist_ok=True)

def check_results(log_path, list_exe, set_trace):
    success = []
    failed = []
    # print(set_trace)
    # print(list_exe)
    for exe in list_exe:
        exe_name = exe     # 如果 list_exe 是路径，取文件名；如果本来就是名字也没问题
        exe_dir = log_path / exe_name

        if not exe_dir.exists():
            # print(f"[Missing Directory] {exe_dir}")
            for trace in set_trace:
                failed.append((exe_name, trace, "directory missing"))
            continue

        for trace in set_trace:
            trace_name = trace
            log_file = exe_dir / f"{trace_name}.log"
            if not log_file.exists():
                failed.append((exe_name, trace_name, "log missing"))
                continue

            try:
                with open(log_file, "r", encoding="utf-8", errors="ignore") as f:
                    completed = any(
                        "ChampSim completed all CPUs" in line
                        for line in f
                    )
            except Exception as e:
                failed.append((exe_name, trace_name, f"read error: {e}"))
                continue

            if completed:
                success.append((exe_name, trace_name))
            else:
                failed.append((exe_name, trace_name, "not completed"))

    # 输出统计
    total = len(list_exe) * len(set_trace)
    print("=" * 60)
    print(f"Total experiments : {total}")
    print(f"Completed         : {len(success)}")
    print(f"Incomplete        : {len(failed)}")

    if failed:
        print("\nFailed experiments:")
        for exe, trace, reason in failed:
            print(f"  {exe:20} {trace:50} {reason}")

    return success, failed

if args.phase == "Run" or args.phase == "All":
    success, failed = check_results(LOG_PATH, required_executables, trace_name_set)
    print(required_executables,trace_name_set)
    if failed:
        running_command = [f"python3","runner.py","-p"]+required_executables+["-l"]+run_trace_list[args.figure] + ["-s"]
        print(f"Results not ready, running ChampSim Task with {YELLOW}{' '.join(running_command)}{END}")
        subprocess.run(running_command)
        print("ChampSim task completed, checking result integrity...")
        success, failed = check_results(LOG_PATH, required_executables, trace_name_set)
        if failed:
            print(f"{RED}Aborted: Experimental results are incomplete, possibly due to an interrupted experiment. Try running the experiment again.{END}")
            exit(0)
        else:
            print(f"{GREEN}All experiments completed successfully.{END}")
    else:
        print(f"{GREEN}All experiments completed successfully.{END}")

if args.phase == "Run":
    print("Goodbye")
    exit(0)

collect_cmd = collect_command[args.figure]

print(f"Collecting results with {YELLOW}{' '.join(collect_cmd)}{END}")

for c in collect_cmd:
    subprocess.run(c,shell=True)

os.makedirs(FIGURE_PATH, exist_ok=True)

lst = os.listdir("figure-scripts")

for l in lst:
    start = l.split("-")[0]
    if start == args.figure:
        print(f"Generating {args.figure} with {YELLOW}python3 {l}{END}")
        subprocess.run([f"python3", l], cwd="figure-scripts")
        break
lst = os.listdir(FIGURE_PATH)
find = False
for l in lst:
    start = l.split("-")[0]
    if start == args.figure:
        print(f"{GREEN}{args.figure} generated successfully: {END}{FIGURE_PATH}/{l}")
        find = True

if not find:
    print(f"{RED}Failed to generate {args.figure}.{END}")