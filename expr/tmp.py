#!/usr/bin/env python3
import os

BASELINE_FILE = 'ipc_no.txt'
EXPERIMENT_FILE = 'ipc.txt'
OUTPUT_FILE = 'speedup.txt'

def read_ipc(file_path):
    ipc_map = {}
    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line: 
                continue
            try:
                trace, val = line.split(':')
                trace = trace.strip()
                val = val.strip()
                if val.upper() == 'NA':
                    ipc_map[trace] = None
                else:
                    ipc_map[trace] = float(val)
            except ValueError:
                print(f"Skipping malformed line: {line}")
    return ipc_map

baseline_ipc = read_ipc(BASELINE_FILE)
experiment_ipc = read_ipc(EXPERIMENT_FILE)

speedup_map = {}
for trace, base_val in baseline_ipc.items():
    exp_val = experiment_ipc.get(trace)
    if base_val is None or exp_val is None:
        speedup_map[trace] = 0.0
    else:
        speedup_map[trace] = exp_val / base_val

# 保存到文件
with open(OUTPUT_FILE, 'w') as f:
    for trace, sp in speedup_map.items():
        f.write(f"{trace}: {sp:.3f}\n")

print(f"Speedup calculated for {len(speedup_map)} traces, saved to {OUTPUT_FILE}")