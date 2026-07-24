#!/usr/bin/env python3

from utils.defs import *
import argparse
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

parser = argparse.ArgumentParser()

parser.add_argument("--output", "-o", default=RESULT_PATH, help="Specify the output directory for the results")
parser.add_argument("--pflist", "-p", nargs="+", help="Specify the list of prefetchers")
parser.add_argument("--alias", "-a", nargs="+", help="Specify aliases for the prefetchers in the output file, in the same order as the prefetcher list")
parser.add_argument("--measure", "-m", nargs="+", help="Specify the metrics to extract")
args = parser.parse_args()

PF_LIST = []
pf_alias_map = dict()
if args.pflist is not None:
    PF_LIST = args.pflist
    if args.alias is not None:
        pf_alias_map = {pf: alias for pf,alias in zip(PF_LIST, args.alias)}

if args.output is not None:
    os.makedirs(args.output, exist_ok=True)

METRICS = [
    'IPC',
    'IPCI',
    'L2C_Coverage',
    'L2C_Accuracy',
    'L2C_Overprediction',
    'L2C_Timeliness',
    'L2C_PrefetchHit',
    # 'L2C_PFfill',
    # 'L2C_PFhit',
    # 'L2C_DemandHit',
    # 'L2C_UselessPF',
    # 'L2C_Demand_miss',
    'L2C_Relative_Useful', # relative to baseline
    'L2C_UsefulPF',
    'LLC_DemandHit',
    'DRAM_Traffic',
    # 'L1D_average_miss_latency',
    # 'L2C_average_miss_latency',
    # 'LLC_average_miss_latency',
    # 'L1-MPKI',
    # 'L2-MPKI',
    'MPKI',
    'PCM_useful_prefetches',
    'PCM_late_prefetches',
    'PCM_accuracy',
    'PCM_laterate',
    'PCM_useful_coverage',
    'MT_inserts',
    'MT_lookups',
    'MT_hits',
    'MT_usefuls', # l2 useful prefetches, normalized to baseline LLC traffic
    'MT_hits2lookups_rate',
    'MT_usefuls2hits_rate',
    'MT_accuratepf',
    'LLC_Traffic',
    'MT_acc_find_rate',
    'Fail_Entry_Init',
    'Insert_PC',
    'Init_L3Hit_rate',
    'Init_UPF_rate',
    "L2C_PF_Issue",
    "L2C_PF_Fill",
]
BASELINE = "baseline"
def get_measure(path, baseline_result = None):
    if not os.path.exists(path):
        print(f"{RED}Error: Log file {path} not found.{END}")
        return {m:"0" for m in METRICS}
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
                    if (result := re.search(rf'{m} \s*([0-9.eE+-]+)', line)) is not None:
                        counters[m] = result.group(1)

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
            

            if line.startswith("cpu0->LLC TOTAL"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                total_llc = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->LLC LOAD"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                load_llc = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->LLC RFO"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                rfo_llc = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->LLC TRANSLATION"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                trans_llc = {k: int(v) for k, v in pairs}

            if line.startswith("cpu0->LLC PREFETCH REQUESTED"):
                pairs = re.findall(r'(\w+):\s+(\d+)', line)
                data_llcpf = {k: int(v) for k, v in pairs}

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
        
        counters['L2C_PF_Issue'] = f"{data_l2pf['ISSUED']}"
        counters['L2C_PF_Fill'] = f"{data_l2pf['USEFUL'] + data_l2pf['LATE'] + data_l2pf['USELESS']}"
        # counters['L2C_DemandHit'] = f"{load_l2c['HIT'] / load_l2c['ACCESS']}"
        counters['L2C_UsefulPF'] = f"{data_l2pf['USEFUL'] / (data_l2pf['ISSUED']) if data_l2pf['ISSUED'] > 0 else 0.0} "
        # counters['L2C_UselessPF'] = f"{data_l2pf['USELESS'] / (data_l2pf['ISSUED']) if data_l2pf['ISSUED'] > 0 else 0.0} "
        counters['LLC_DemandHit'] = f"{load_llc['HIT'] / load_llc['ACCESS']}"
        counters['MT_usefuls'] = f"{data_l2pf['USEFUL']}"
        if (int(counters['MT_lookups']) > 0):
            counters['MT_acc_find_rate'] = f"{float(counters['MT_accuratepf']) / float(counters['MT_lookups'])}"

        if baseline_result is not None:
            counters['MT_inserts'] = f"{int(counters['MT_inserts']) / int(baseline_result['LLC_Traffic']) if int(baseline_result['LLC_Traffic']) > 0 else 0.0}"
            counters['MT_lookups'] = f"{int(counters['MT_lookups']) / int(baseline_result['LLC_Traffic']) if int(baseline_result['LLC_Traffic']) > 0 else 0.0}"
            counters['MT_hits'] = f"{int(counters['MT_hits']) / int(baseline_result['LLC_Traffic']) if int(baseline_result['LLC_Traffic']) > 0 else 0.0}"
            counters['MT_usefuls'] = f"{int(counters['MT_usefuls']) / int(baseline_result['LLC_Traffic']) if int(baseline_result['LLC_Traffic']) > 0 else 0.0}"

            
        else:
            counters['LLC_Traffic'] = f"{int(total_llc['ACCESS']) - int(trans_llc['ACCESS']) + int(counters['MT_lookups']) + int(counters['MT_inserts'])}"
        if baseline_result is not None:
            if baseline_result['L2C_Demand_miss'] > 0:
                counters['L2C_Coverage'] = f"{(baseline_result['L2C_Demand_miss'] - (load_l2c['MISS'] + rfo_l2c['MISS'])) / baseline_result['L2C_Demand_miss']}"
                #counters['L2C_Coverage'] = f"{data_l2pf['USEFUL']/(data_l2pf['USEFUL']+load_l2c['MISS']+rfo_l2c['MISS'])}"
                # counters['L2C_Overprediction']
            else:
                counters['L2C_Coverage'] = f"{0.0}"
        else:
            counters['L2C_Coverage'] = f"{0.0}"
        counters['MT_hits2lookups_rate'] = f"{float(counters['MT_hits']) / float(counters['MT_lookups']) if float(counters['MT_lookups']) > 0 else 0.0}"
        counters['MT_usefuls2hits_rate'] = f"{float(counters['MT_usefuls']) / float(counters['MT_hits']) if float(counters['MT_hits']) > 0 else 0.0}"
        # if baseline_result is not None:
        #     if int(baseline_result['PCM_useful_prefetches']) > 0:
        #         counters['PCM_useful_coverage'] = f"{(int(counters['PCM_useful_prefetches']) / int(baseline_result['PCM_useful_prefetches'])) if int(baseline_result['PCM_useful_prefetches']) > 0 else 0.0}"
        # else:
        #     counters['PCM_useful_coverage'] = counters['PCM_useful_prefetches']
        
        if baseline_result is not None:
            if int(baseline_result['L2C_Relative_Useful']) > 0:
                counters['L2C_Relative_Useful'] = f"{(data_l2pf['USEFUL'] / float(baseline_result['L2C_Relative_Useful']))}"
        else:
            counters['L2C_Relative_Useful'] = f"{(data_l2pf['USEFUL'])}"

        if baseline_result is not None:
            if int(baseline_result['L2C_PrefetchHit']) > 0:
                counters['L2C_PrefetchHit'] = f"{(data_l2pf_access['HIT'] / data_l2pf['ISSUED']) if data_l2pf['ISSUED'] > 0 else 0.0}"
        else:
            counters['L2C_PrefetchHit'] = f"{(data_l2pf_access['HIT'])}"
        
        if (data_l2pf['USEFUL'] + data_l2pf['LATE'] + data_l2pf['USELESS']) > 10:
            counters['L2C_Accuracy'] = f"{((data_l2pf['USEFUL'] + data_l2pf['LATE']) / (data_l2pf['USEFUL'] + data_l2pf['LATE'] + data_l2pf['USELESS']))}"
        # if (data_l2pf['ISSUED']) > 0:
        #     counters['L2C_Accuracy'] = f"{((data_l2pf['USEFUL'] + data_l2pf['LATE']) / (data_l2pf['ISSUED']))}"
        if data_l2pf['USEFUL'] > 0:
            counters['L2C_Timeliness'] = f"{(data_l2pf['USEFUL'] / (data_l2pf['USEFUL'] + data_l2pf['LATE']))}"

        if baseline_result is not None:
            baseline_dram_traffic = int(baseline_result['DRAM_Traffic'])
            current_dram_traffic = rq_rbh + rq_rbm + wq_rbh + wq_rbm
            if baseline_dram_traffic > 0:
                counters['DRAM_Traffic'] = str(1.0*current_dram_traffic / baseline_dram_traffic)
        else:
            counters['DRAM_Traffic'] = str(rq_rbh + rq_rbm + wq_rbh + wq_rbm)
        #print(counters)
        return counters



if __name__ == "__main__":


    with open(SCRIPTS_PATH/"utils"/"tracelist", "r") as f:
        for line in f:
            if (line.split(":")[0] == "gap" or line.split(":")[0] == "ligra" or line.split(":")[0] == "ml" or line.split(":")[0] == "google" or line.split(":")[0] == "spec17"):
                TRACE_LIST[line.split(":")[0]] =  sorted(line.split(":")[1].strip().split(" "))

    with open(f"{args.output}/average.csv","w") as ff:
        if args.measure is not None:
            met = args.measure
        else:
            met = METRICS
        ff.write("Set,Prefetcher," + ",".join(met) + "\n")
        pass
    for set_name in TRACE_LIST.keys():
        with open(f"{args.output}/average.csv","a") as ff:
            with open(f"{args.output}/{set_name}.csv", "w") as f:
                if args.measure is not None:
                    met = args.measure
                else:
                    met = METRICS
                f.write("Trace,Prefetcher," + ",".join(met) + "\n")
                baseline_result = {}
                average = {pf:[] for pf in PF_LIST}
                for trace in TRACE_LIST[set_name]:
                    baseline_result[trace] = get_measure(LOG_PATH / BASELINE / f"{trace}.log")
                    
                    for pf in PF_LIST:
                        if os.path.exists(LOG_PATH / pf / f"{trace}.log"):
                            #print("Reading from: " + str(LOG_PATH / pf / f"{trace}.log"))
                            result = get_measure(LOG_PATH / pf / f"{trace}.log", baseline_result[trace])
                            if args.alias is not None:
                                f.write(trace + "," + pf_alias_map[pf] + "," + ",".join([result[m] for m in met]) + "\n")
                            else:
                                f.write(trace + "," + pf + "," + ",".join([result[m] for m in met]) + "\n")
                            average[pf].append(list(result.values()))
                        else:
                            #print(f"{RED}Warning: Log file for trace {trace} with prefetcher {pf} not found.{END}")
                            pass
                average_line = {pf: {m: "0" for m in METRICS} for pf in PF_LIST}
                for m in METRICS:
                    if m == "IPC" or m == "IPCI":
                        for pf in PF_LIST:
                            total = 1.0
                            count = 0
                            for res in average[pf]:
                                try:
                                    total *= float(res[METRICS.index(m)])
                                    count += 1
                                except:
                                    print("Bad format:", res[METRICS.index(m)])
                            if count > 0:
                                average_line[pf][m] = f"{(pow(total, 1.0/count)):.4f}"
                            else:
                                average_line[pf][m] = "0.0000"
                    else:
                        for pf in PF_LIST:
                            total = 0.0
                            count = 0
                            for res in average[pf]:
                                try:
                                    total += float(res[METRICS.index(m)])
                                    count += 1
                                except:
                                    print("Bad format:", res[METRICS.index(m)])
                                
                            if count > 0:
                                average_line[pf][m] = f"{(total / count):.4f}"
                            else:
                                average_line[pf][m] = "0.0000"
                for pf in PF_LIST:
                    if args.measure is not None:
                        line_l = [average_line[pf][m] for m in args.measure]
                    else:
                        line_l = average_line[pf].values()
                    
                    if args.alias is not None:
                        f.write("Average," + pf_alias_map[pf] + "," + ",".join(line_l) + "\n")
                        ff.write(f"{set_name}," + pf_alias_map[pf] + "," + ",".join(line_l) + "\n")
                    else:
                        f.write("Average," + pf + "," + ",".join(line_l) + "\n")
                        ff.write(f"{set_name}," + pf + "," + ",".join(line_l) + "\n")

    import pandas as pd
    import numpy as np

    df = pd.read_csv(f"{args.output}/average.csv")

    geo_cols = ["IPC", "IPCI"]
    measure_cols = [col for col in df.columns if col not in ["Set", "Prefetcher"]]
    mean_cols = [col for col in measure_cols if col not in geo_cols]

    def geomean(x):
        x = x[x > 0]
        return np.exp(np.mean(np.log(x))) if len(x) > 0 else np.nan

    avg_rows = []

    for pf, group in df.groupby("Prefetcher"):
        row = {
            "Set": "Average",
            "Prefetcher": pf
        }
        
        # 几何平均
        for col in geo_cols:
            if col in group:
                row[col] = geomean(group[col])
        
        # 算术平均
        for col in mean_cols:
            row[col] = group[col].mean()
        
        avg_rows.append(row)

    avg_df = pd.DataFrame(avg_rows)

    # 保证列顺序一致
    avg_df = avg_df[df.columns]

    # 拼接（原数据 + 平均行）
    out_df = pd.concat([df, avg_df], ignore_index=True)

    out_df.to_csv(f"{args.output}/average.csv", index=False)

    print(out_df)