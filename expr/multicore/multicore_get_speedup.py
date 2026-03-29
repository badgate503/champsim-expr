import os
import re
import math
import csv
from collections import defaultdict

BASE_DIR = "/mnt/data/lyq/exprlog"
CORES = ["core2", "core4", "core8"]

# 匹配 IPC
IPC_PATTERN = re.compile(r"CPU \d+ cumulative IPC:\s*([0-9.]+)")

def geometric_mean(nums):
    if len(nums) == 0:
        return None
    product = 1.0
    for x in nums:
        product *= x
    return product ** (1.0 / len(nums))

def parse_log(filepath):
    ipcs = []
    with open(filepath, "r") as f:
        for line in f:
            m = IPC_PATTERN.search(line)
            if m:
                ipcs.append(float(m.group(1)))
    return geometric_mean(ipcs)

# 数据结构：
# data[core][prefetcher][trace] = ipc
data = defaultdict(lambda: defaultdict(dict))

for core in CORES:
    core_path = os.path.join(BASE_DIR, core)

    if not os.path.isdir(core_path):
        continue

    for pf_dir in os.listdir(core_path):
        pf_path = os.path.join(core_path, pf_dir)
        if not os.path.isdir(pf_path):
            continue

        # pf_dir 形如 xxx.2c / xxx.4c
        prefetcher = pf_dir.split(".")[0]

        for file in os.listdir(pf_path):
            if not file.endswith(".log"):
                continue

            trace_set = file.replace(".log", "")
            log_path = os.path.join(pf_path, file)

            ipc = parse_log(log_path)
            if ipc is not None:
                data[core][prefetcher][trace_set] = ipc

# 输出 CSV
output_file = "speedup.csv"

with open(output_file, "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["core_number", "prefetcher", "trace_set", "speedup"])

    for core in data:
        if "baseline" not in data[core]:
            continue

        baseline_data = data[core]["baseline"]

        for pf in data[core]:
            if pf == "baseline":
                continue

            for trace in data[core][pf]:
                if trace not in baseline_data:
                    continue

                ipc_pf = data[core][pf][trace]
                ipc_base = baseline_data[trace]

                if ipc_base == 0:
                    continue

                speedup = ipc_pf / ipc_base

                core_num = core.replace("core", "")

                writer.writerow([core_num, pf, trace, speedup])

print(f"Done! Output -> {output_file}")