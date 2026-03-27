from multiprocessing.managers import BaseManager
import re
METRICS = [
    'IPC',
    'IPCI',
    'L2C_PFIssue',
    'L2C_Coverage',
    'L2C_Accuracy',
    'L2C_Overprediction',
    'L2C_Timeliness',
    'DRAM_Traffic',
    'L1D_average_miss_latency',
    'L2C_average_miss_latency',
    'LLC_average_miss_latency',
    'L1-MPKI',
    'L2-MPKI',
    'MPKI',
    'PCM_useful_prefetches',
    'PCM_late_prefetches',
    'PCM_coverage',
    'PCM_accuracy',
    'PCM_laterate',
    "L2C_Hit",
    "L2C_Total",
]
BASELINE = "no"
TRACE_LIST = dict()
with open("tracelist", "r") as f:
    for line in f:
        TRACE_LIST[line.split(":")[0]] =  sorted(line.split(":")[1].strip().split(" "))
def get_prefetcher_list():
    lst = []
    for f in os.listdir("/mnt/data/lyq/exprlog/"):
        lst.append(f)
    return lst

def get_trace_list(prefetcher):
    lst = []
    for f in os.listdir(f"/mnt/data/lyq/exprlog/{prefetcher}"):
        if f.endswith(".log"):
            lst.append(f.replace(".log", ""))
    return lst 

def get_expr_result(prefetcher, trace):
    paths: dict[str,tuple[str,str]] = {} # trace -> 

    for f in os.listdir(f"/mnt/data/lyq/exprlog/{BASELINE}"):
        if f.endswith(".log"):
            if trace == "all" or trace == "average" or trace == f.replace(".log", "") or (trace in TRACE_LIST.keys() and f.replace(".log", "") in TRACE_LIST[trace]):
                paths[f.replace(".log", "")]=(f"/mnt/data/lyq/exprlog/{BASELINE}/{f}",)
    for p in list(paths.keys()):
        if os.path.exists(f"/mnt/data/lyq/exprlog/{prefetcher}/{p}.log"):
            paths[p]=(paths[p][0],f"/mnt/data/lyq/exprlog/{prefetcher}/{p}.log")
        else:
            del paths[p]
    
    measures = dict()
    for p in paths.keys():
        print(p)
        baseline_measure = get_m(paths[p][0], None)
        res = get_m(paths[p][1], baseline_measure)
        measures[p] = {k: float(v) for k, v in res.items()}
    if trace == "all":
        return measures
    elif trace == "average" or (trace in TRACE_LIST.keys()):
        average = dict()
        for m in METRICS:
            if m == "IPC" or m == "IPCI":
                prod = 1
                cnt = 0
                for q in measures.keys():
                    prod *= measures[q][m]
                    cnt += 1
                if cnt != 0:
                    average[m] = pow(prod, 1.0/cnt)
                else:
                    average[m] = 0
            else:
                sum = 0
                cnt = 0
                for q in measures.keys():
                    sum += measures[q][m]
                    cnt += 1
                if cnt != 0:
                    average[m] = sum / cnt
                else:
                    average[m] = 0
        return average
    else:
        for k in measures.keys():
            return measures[k]




def get_m(path, baseline_result = None):
    with open(path, "r") as f:
        lines = f.readlines()
        counters = {m:"0" for m in METRICS}
        find = False
        
        for idx, line in enumerate(lines):
            if line.startswith("=== Simulation ==="):
                find = True
            if not find:
                continue

            for m in METRICS:
                if line.startswith(m):
                    counters[m] = re.search(rf'{m} \s*([0-9.]+)', line).group(1)
                # if line.startswith("GPM_laterate"):
                #     counters['GPM_laterate'] = re.search(r'GPM_laterate \s*([0-9.]+)', line).group(1)

            if line.startswith("CPU 0 cumulative IPC:"):
                ipc = re.search(r'CPU 0 cumulative IPC:\s*([0-9.]+)', line).group(1)

            if line.startswith("cpu0->cpu0_L1D AVERAGE MISS LATENCY:"):
                l1daml = re.search(r'cpu0->cpu0_L1D AVERAGE MISS LATENCY:\s*([0-9.]+)', line).group(1)

            if line.startswith("cpu0->cpu0_L2C AVERAGE MISS LATENCY:"):
                l2caml = re.search(r'cpu0->cpu0_L2C AVERAGE MISS LATENCY:\s*([0-9.]+)', line).group(1)

            if line.startswith("cpu0->LLC AVERAGE MISS LATENCY:"):
                llcaml = re.search(r'cpu0->LLC AVERAGE MISS LATENCY:\s*([0-9.]+)', line).group(1)

            if line.startswith("Channel 0 RQ ROW_BUFFER_HIT:"):
                rq_rbh = int(re.search(r'Channel 0 RQ ROW_BUFFER_HIT:\s*(\d+)', line).group(1))
                rq_rbm = int(re.search(r'ROW_BUFFER_MISS:\s*(\d+)', lines[idx+1]).group(1))

            if line.startswith("Channel 0 WQ ROW_BUFFER_HIT:"):
                wq_rbh = int(re.search(r'Channel 0 WQ ROW_BUFFER_HIT:\s*(\d+)', line).group(1))
                wq_rbm = int(re.search(r'ROW_BUFFER_MISS:\s*(\d+)', lines[idx+1]).group(1))

            if line.startswith("cpu0->cpu0_L2C PREFETCH"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                data_l2pf = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->cpu0_L1D TOTAL"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                total_l1d = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->cpu0_L1D LOAD"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                load_l1d = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->cpu0_L1D RFO"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                rfo_l1d = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->cpu0_L1D PREFETCH"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                prefetch_l1d = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->cpu0_L2C TOTAL"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                total_l2c = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->cpu0_L2C LOAD"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                load_l2c = {k: int(v) for k, v in pairs}
            
            if line.startswith("cpu0->cpu0_L2C RFO"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                rfo_l2c = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->cpu0_L2C PREFETCH"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                prefetch_l2c = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->LLC TOTAL"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                total_llc = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->LLC LOAD"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                load_llc = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->LLC RFO"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                rfo_llc = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->LLC PREFETCH"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                prefetch_llc = {k: int(v) for k, v in pairs}

        if not find:
            print(f"{RED}Error: No simulation result found in {path}{END}")
            return counters

        if baseline_result is not None:
            baseline_ipc = float(baseline_result['IPC'])
            if baseline_ipc > 0:
                counters['IPCI'] = f"{(float(ipc) / baseline_ipc)}"
        else:
            counters['IPCI'] = "1.0"
            counters['L2C_Demand_miss'] = load_l2c['MISS'] + rfo_l2c['MISS']

        counters['IPC'] = str(ipc)
        counters['L1D_average_miss_latency'] = str(l1daml)
        counters['L2C_average_miss_latency'] = str(l2caml)
        counters['LLC_average_miss_latency'] = str(llcaml)

        counters['L1-MPKI'] = f"{((load_l1d['MISS'] + rfo_l1d['MISS']) / 200_000)}"
        counters['L2-MPKI'] = f"{((load_l2c['MISS'] + rfo_l2c['MISS']) / 200_000)}"
        counters['MPKI'] = f"{((load_llc['MISS'] + rfo_llc['MISS']) / 200_000)}"
        counters['L2C_Hit'] = f"{load_l2c['HIT']}"
        counters['L2C_Total'] = f"{load_l2c['ACCESS']}"
        if baseline_result is not None:
            if baseline_result['L2C_Demand_miss'] > 0:
                counters['L2C_Coverage'] = f"{(baseline_result['L2C_Demand_miss'] - (load_l2c['MISS'] + rfo_l2c['MISS'])) / baseline_result['L2C_Demand_miss']}"
                # counters['L2C_Overprediction']
            else:
                counters['L2C_Coverage'] = f"{0.0}"
        else:
            counters['L2C_Coverage'] = f"{0.0}"
            
        if data_l2pf['ISSUED'] > 0:
            counters['L2C_Accuracy'] = f"{((data_l2pf['USEFUL'] + data_l2pf['LATE']) / data_l2pf['ISSUED'])}"
            counters['L2C_PFIssue'] = f"{data_l2pf['ISSUED']}"
        if data_l2pf['USEFUL'] > 0:
            counters['L2C_Timeliness'] = f"{(data_l2pf['USEFUL'] / (data_l2pf['USEFUL'] + data_l2pf['LATE']))}"

        if baseline_result is not None:
            baseline_dram_traffic = int(baseline_result['DRAM_Traffic'])
            current_dram_traffic = rq_rbh + rq_rbm + wq_rbh + wq_rbm
            if baseline_dram_traffic > 0:
                counters['DRAM_Traffic'] = str(1.0*current_dram_traffic / baseline_dram_traffic)
        else:
            counters['DRAM_Traffic'] = str(rq_rbh + rq_rbm + wq_rbh + wq_rbm)

        return counters
import os

from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route("/get_prefetcher_list", methods=["POST"])
def get_prefetcher_list_rpc():
    data = request.json
    print("hi")
    result = get_prefetcher_list()
    return jsonify(result)

@app.route("/get_trace_list", methods=["POST"])
def get_trace_list_rpc():
    data = request.json
    print(data["prefetcher"])
    result = get_trace_list(data["prefetcher"])
    
    return jsonify(result)

@app.route("/get_expr_result", methods=["POST"])
def get_expr_result_rpc():
    data = request.json
    result = get_expr_result(data["prefetcher"], data["trace"])
    return jsonify(result)
app.run(host="0.0.0.0", port=8000)