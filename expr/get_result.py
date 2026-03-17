#!/usr/bin/env python3.11

from utils.defs import *
import re
import os
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
TRACE_LIST = {}
PF_LIST = [
    # "no",
    # "baseline.1way",
    # "baseline.2way",
    # "baseline.3way",
    # "baseline.4way",
    # "baseline.5way",
    # "baseline.6way",
    # "baseline.7way",
    # "baseline",
    # "triangel",
    # "prophet",
    # "prism",

    # "pctp.a1",
    # "pctp.a2",
    # "pctp.a4",
    # "pctp.a8",
    # "pctp.a12",
    # "pctp.a16",
    # "pctp6ksrp",
    # "pctp12ksrp",
    # "pctp24ksrp",
    # "pctp1waysrp",
    # "pctp2waysrp",
    # "pctp3waysrp",
    # "pctp4waysrp",
    # "pctp1waysrp2",

    # "conftp-inf",
    # "conftp1way",
    # "conftp2way",
    # "conftp3way",
    # "conftp4way",

    # "resize1w",
    # "resize10w",
    # "resize1m",
    # "resize10m",

    "latetpl0d2",
    "latetpl0d3",
    "latetpl0d4",
    "latetpl0d5",
    "latetpl1d1",
    "latetpl1d2",
    "latetpl1d3",
    "latetpl1d4",
    "latetpl1d5",
    "latetpl2d1",
    "latetpl2d2",
    "latetpl2d3",
    "latetpl2d4",
    "latetpl2d5",
    "latetpl3d1",
    "latetpl3d2",
    "latetpl3d3",
    "latetpl3d4",
    "latetpl3d5",
    "latetpl4d1",
    "latetpl4d2",
    "latetpl4d3",
    "latetpl4d4",
    "latetpl4d5",

    # "earlytp",

    # "filtetp",
    # "shtp",
    # "rndtp",
    # "srtp",
    # "drtp",
]
METRICS = [
    'IPC',
    'IPCI',
    'L2C_Coverage',
    'L2C_Accuracy',
    'L2C_Overprediction',
    'L2C_Timeliness',
    # 'L2C_PFfill',
    # 'L2C_PFhit',
    # 'L2C_DemandHit',
    # 'L2C_UselessPF',
    # 'L2C_Demand_miss',
    'L2C_UsefulPF',
    "LLC_DemandHit",
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
    'MT_hitrate',
    'CT_hitrate',
    'CT_accuracy',
    'CT_accuratepf',
]
BASELINE = "no"
def get_measure(path, baseline_result = None):
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
                # if line.startswith("PCM_laterate"):
                #     counters['PCM_laterate'] = re.search(r'PCM_laterate \s*([0-9.]+)', line).group(1)

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

            if line.startswith("cpu0->cpu0_L2C PREFETCH     ACCESS"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                data_l2pf_access = {k: int(v) for k, v in pairs}
            
            if line.startswith("cpu0->cpu0_L2C PREFETCH REQUESTED"):
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
        
        # counters['L2C_PFhit'] = f"{data_l2pf_access['HIT']}"
        # counters['L2C_DemandHit'] = f"{load_l2c['HIT'] / load_l2c['ACCESS']}"
        counters['L2C_UsefulPF'] = f"{data_l2pf['USEFUL'] / (data_l2pf['ISSUED']) if data_l2pf['ISSUED'] > 0 else 0.0} "
        # counters['L2C_UselessPF'] = f"{data_l2pf['USELESS'] / (data_l2pf['ISSUED']) if data_l2pf['ISSUED'] > 0 else 0.0} "
        counters['LLC_DemandHit'] = f"{load_llc['HIT'] / load_llc['ACCESS']}"

        if baseline_result is not None:
            if baseline_result['L2C_Demand_miss'] > 0:
                counters['L2C_Coverage'] = f"{(baseline_result['L2C_Demand_miss'] - (load_l2c['MISS'] + rfo_l2c['MISS'])) / baseline_result['L2C_Demand_miss']}"
                # counters['L2C_Overprediction']
            else:
                counters['L2C_Coverage'] = f"{0.0}"
        else:
            counters['L2C_Coverage'] = f"{0.0}"
            
        
        counters['L2C_PFfill'] = f"{data_l2pf['USEFUL'] + data_l2pf['LATE'] + data_l2pf['USELESS']}"
        if int(counters['L2C_PFfill']) > 0:
            counters['L2C_Accuracy'] = f"{((data_l2pf['USEFUL'] + data_l2pf['LATE']) / int(counters['L2C_PFfill']))}"
        if data_l2pf['USEFUL'] > 0:
            counters['L2C_Timeliness'] = f"{(data_l2pf['USEFUL'] / (data_l2pf['USEFUL'] + data_l2pf['LATE']))}"

        if baseline_result is not None:
            baseline_dram_traffic = int(baseline_result['DRAM_Traffic'])
            current_dram_traffic = rq_rbh + rq_rbm + wq_rbh + wq_rbm
            if baseline_dram_traffic > 0:
                counters['DRAM_Traffic'] = str(1.0*current_dram_traffic / baseline_dram_traffic)
        else:
            counters['DRAM_Traffic'] = str(rq_rbh + rq_rbm + wq_rbh + wq_rbm)
        print(counters)
        return counters



if __name__ == "__main__":


    with open("utils/tracelist", "r") as f:
        for line in f:
            if (line.split(":")[0] != "nontemp"):
                TRACE_LIST[line.split(":")[0]] =  sorted(line.split(":")[1].strip().split(" "))


    for set_name in TRACE_LIST.keys():
        with open(f"result/{set_name}.csv", "w") as f:
            f.write("Trace,Prefetcher," + ",".join(METRICS) + "\n")
            baseline_result = {}
            average = {pf:[] for pf in PF_LIST}
            for trace in TRACE_LIST[set_name]:
                baseline_result[trace] = get_measure(LOG_PATH +"/"+ BASELINE + "/" + (trace+".log"))
                
                for pf in PF_LIST:
                    if os.path.exists(LOG_PATH +"/"+ pf + "/" + (trace+".log")):
                        print("Reading from: " + LOG_PATH +"/"+ pf + "/" + (trace+".log"))
                        result = list(get_measure(LOG_PATH +"/"+ pf + "/" + (trace+".log"), baseline_result[trace]).values())
                        f.write(trace + "," + pf + "," + ",".join(result) + "\n")
                        average[pf].append(result)
                    else:
                        print(f"{RED}Warning: Log file for trace {trace} with prefetcher {pf} not found.{END}")
            average_line = {pf: {m: "0" for m in METRICS} for pf in PF_LIST}
            for m in METRICS:
                if m == "IPC" or m == "IPCI":
                    for pf in PF_LIST:
                        total = 1.0
                        count = 0
                        for res in average[pf]:
                            total *= float(res[METRICS.index(m)])
                            count += 1
                        if count > 0:
                            average_line[pf][m] = f"{(pow(total, 1.0/count)):.4f}"
                        else:
                            average_line[pf][m] = "0.0000"
                else:
                    for pf in PF_LIST:
                        total = 0.0
                        count = 0
                        for res in average[pf]:
                            total += float(res[METRICS.index(m)])
                            count += 1
                        if count > 0:
                            average_line[pf][m] = f"{(total / count):.4f}"
                        else:
                            average_line[pf][m] = "0.0000"
            for pf in PF_LIST:
                result = average_line[pf].values()
                f.write("Average," + pf + "," + ",".join(result) + "\n")

                    