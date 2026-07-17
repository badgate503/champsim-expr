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

parser.add_argument("--output", "-o", default="./result", help="指定输出文件")
parser.add_argument("--pflist", "-p", nargs="+", help="指定仿真预热长度（指令数）")
parser.add_argument("--alias", "-a", nargs="+", help="指定预取器别名，顺序与预取器列表一致")
parser.add_argument("--measure", "-m", nargs="+", help="指定仿真区间长度（指令数）")
args = parser.parse_args()

PF_LIST = [
    # "no",
    # "optimal",
    # "inftable8",
    # "inftable12",
    # "inftable16",
    # "baseline.1way",
    # "baseline.2way",
    # "baseline.3way",
    # "baseline.4way",
    # "baseline.5way",
    # "baseline.6way",
    # "baseline.7way",
    # "baseline.8way",
    # "acc_stat",
    # "triangel",
    # "triangel_8way",
    # "prophet",
    # "kairos",
    # "croage",
    "prism",
    # "prism-wo-pctp",
    # "prism_final",
    # "prism33",
    # "prism_l1",
    # "prism_l2",
    # "prism_l3",
    # "prism_l4",
    # "prism_fixlevel",
    # "prism_rs",
    # "prism_runtimers",
    # "prism_dy",
    # "prism_dy2",
    # "prism_l2pf",
    # "prism_l3pf",
    # "prism_filte",
    # "prism_coverage_05",
    # "prism_coverage_15",
    # "prism_coverage_25",
    # "prism_coverage_35",
    # "prism_coverage_50",
    # "prism_accuracy_rrpv",
    # "prism_8to1",
    # "prism_pctp_filter",
    # "prism_pctphit",
    # "prism_8to2",
    # "prism_1to8",
    # "prism_2to8",
    # "prism_init1",
    # "prism_init2",
    # "prism_init8",
    # "prism_warminit1",
    # "prism_warminit2",
    # "prism_warminit8",

    # "l1ipcp.triangel",
    # "l1ipcp.prophet",
    # "l1ipcp.prism",
    # "l1berti.triangel",
    # "l1berti.prophet",
    # "l1berti.prism",

    # "pctpinf.a1",
    # "pctpinf.a2",
    # "pctpinf.a4",
    # "pctpinf.a8",
    # "pctpinf.a12",
    # "pctpinf.a16",
    # "pctp6k",
    # "pctp12k",
    # "pctp24k",
    # "pctp48k",
    # "pctp96k",
    # "pctp144k",
    # "pctp192k",

    # "pctp_0way",
    # "pctp_1way",
    # "pctp_2way",
    "prism_ae",
    "prism_ae_0.1",

    # "conftp-inf",
    # "conftp1way",
    # "conftp2way",
    # "conftp3way",
    # "conftp4way",
    # "conftp44",
    # "conftp11",

    # "tri2way2",

    # "resize_tr",
    # "resize_pr",
    # "resize_kr",
    # "resize_prism",
    # "resize1w",
    # "resize10w",
    # "resize1m",
    # "resize10m",

    # "resize_regular8",
    # "resize_regular2",
    # "resize1to8",
    # "resize8to1",
    # "resize2to8",
    # "resize8to2",
    
    # "earlytp",
    # "filtetp",
    # "filtetp2",

    # "lfutp",
    # "shtp",
    # "shtp-trigger",
    # "srtp",
    # "srtp-trigger",
    # "brtp",
    # "brtp-trigger",
    # "drtp",
    # "drtp-trigger",
    # "rndtp",

    # "ltpl2dyn",
    # "ltpl3dyn",
    # "ltpl4dyn",
    # "ltpl2dynamic",
    # "localdg",
    # "localdg42",
    # "globaldg42",


    # "ltp4wayl0d1",
    # "ltp4wayl0d2",
    # "ltp4wayl0d4",
    # "ltp4wayl0d6",
    # "ltp4wayl0d8",
    # "ltp4wayl1d1",
    # "ltp4wayl1d2",
    # "ltp4wayl1d4",
    # "ltp4wayl1d6",
    # "ltp4wayl1d8",
    # "ltp4wayl2d1",
    # "ltp4wayl2d2",
    # "ltp4wayl2d4",
    # "ltp4wayl2d6",
    # "ltp4wayl2d8",
    # "ltp4wayl3d1",
    # "ltp4wayl3d2",
    # "ltp4wayl3d4",
    # "ltp4wayl3d6",
    # "ltp4wayl3d8",
    # "ltp4wayl4d1",
    # "ltp4wayl4d2",
    # "ltp4wayl4d4",
    # "ltp4wayl4d6",
    # "ltp4wayl4d8",

    # "prism_degree_1",
    # "prism_degree_2",
    # "prism_degree_3",
    # "prism_degree_4",
    # "prism_degree_5",
    # "prism_degree_6",


    # "acc_low00_high55",
    # "acc_low00_high65",
    # "acc_low00_high75",
    # "acc_low00_high85",
    # "acc_low00_high95",
    # "acc_low00_high100",

    # "acc_low05_high55",
    # "acc_low05_high65",
    # "acc_low05_high75",
    # "acc_low05_high85",
    # "acc_low05_high95",
    # "acc_low05_high100",

    # "acc_low10_high55",
    # "acc_low10_high65",
    # "acc_low10_high75",
    # "acc_low10_high85",
    # "acc_low10_high95",
    # "acc_low10_high100",

    # "acc_low15_high55",
    # "acc_low15_high65",
    # "acc_low15_high75",
    # "acc_low15_high85",
    # "acc_low15_high95",
    # "acc_low15_high100",

    # "acc_low20_high55",
    # "acc_low20_high65",
    # "acc_low20_high75",
    # "acc_low20_high85",
    # "acc_low20_high95",
    # "acc_low20_high100",

    # "acc_low25_high55",
    # "acc_low25_high65",
    # "acc_low25_high75",
    # "acc_low25_high85",
    # "acc_low25_high95",
    # "acc_low25_high100",

    # "acc_low30_high55",
    # "acc_low30_high65",
    # "acc_low30_high75",
    # "acc_low30_high85",
    # "acc_low30_high95",
    # "acc_low30_high100",

    # "acc_late05",
    # "acc_late10", 
    # "acc_late15",
    # "acc_late20", 
    # "acc_late25",

    # "bw_mid6_high11",
    # "bw_mid6_high12",
    # "bw_mid6_high13",
    # "bw_mid6_high14",
    # "bw_mid6_high15",
    # "bw_mid7_high11",
    # "bw_mid7_high12",
    # "bw_mid7_high13",
    # "bw_mid7_high14",
    # "bw_mid7_high15",
    # "bw_mid8_high11",
    # "bw_mid8_high12",
    # "bw_mid8_high13",
    # "bw_mid8_high14",
    # "bw_mid8_high15",
    # "bw_mid9_high11",
    # "bw_mid9_high12",
    # "bw_mid9_high13",
    # "bw_mid9_high14",
    # "bw_mid9_high15",
    # "bw_mid10_high11",
    # "bw_mid10_high12",
    # "bw_mid10_high13",
    # "bw_mid10_high14",
    # "bw_mid10_high15",

    # "init1_1.5_1",
    # "init1_1.5_1.25",
    # "init1_1.5_1.5",
    # "init1_1.5_1.75",
    # "init1_1.5_2",

    # "init1_1.75_1",
    # "init1_1.75_1.25",
    # "init1_1.75_1.5",
    # "init1_1.75_1.75",
    # "init1_1.75_2",

    # "k_1_0.25",
    # "k_1_0.5",
    # "k_1_0.75",
    # "k_1_1",
    # "k_1_1.25",
    # "k_1_1.5",
    # #"k_1_1.75",

    # "k_1.25_0.25",
    # "k_1.25_0.5",
    # "k_1.25_0.75",
    # "k_1.25_1",
    # "k_1.25_1.25",
    # "k_1.25_1.5",
    # #"k_1.25_1.75",

    # "k_1.5_0.25",
    # "k_1.5_0.5",
    # "k_1.5_0.75",
    # "k_1.5_1",
    # "k_1.5_1.25",
    # "k_1.5_1.5",
    # #"k_1.5_1.75",

    # "k_1.75_0.25",
    # "k_1.75_0.5",
    # "k_1.75_0.75",
    # "k_1.75_1",
    # "k_1.75_1.25",
    # "k_1.75_1.5",
    # #"k_1.75_1.75",

    # "k_2_0.25",
    # "k_2_0.5",
    # "k_2_0.75",
    # "k_2_1",
    # "k_2_1.25",
    # "k_2_1.5",
    # #"k_2_1.75",

    # "k_2.25_0.25",
    # "k_2.25_0.5",
    # "k_2.25_0.75",
    # "k_2.25_1",
    # "k_2.25_1.25",
    # "k_2.25_1.5",
    #"k_2.25_1.75",

]
pf_alias_map = dict()
if args.pflist is not None:
    PF_LIST = args.pflist
    if args.alias is not None:
        pf_alias_map = {pf: alias for pf,alias in zip(PF_LIST, args.alias)}
if args.output is not None:
    if not os.path.exists(args.output):
        os.makedirs(args.output)

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
    'L2C_USEFUL',
    'L2C_Relative_Useful', # relative to baseline
    'L2C_UsefulPF',
    "LLC_DemandHit",
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
    'MT_hitrate',
    'MT_accuracy',
    'MT_accuratepf',
    'LLC_Traffic',
    'MT_acc_find_rate',
    # 'CT_hitrate',
    # 'CT_accuracy',
    # 'CT_useful_prefetches',
    'Fail_Entry_Init',
    'Insert_PC',
    'Init_L3Hit_rate',
    'Init_UPF_rate',
    "L2C_PF_Issue",
    "L2C_PF_Fill",
]
BASELINE = "baseline.4way"
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
        counters['L2C_USEFUL'] = f"{data_l2pf['USEFUL']}"
        if (int(counters['MT_lookups']) > 0):
            counters['MT_acc_find_rate'] = f"{float(counters['MT_accuratepf']) / float(counters['MT_lookups'])}"

        if baseline_result is not None:
            counters['MT_inserts'] = f"{int(counters['MT_inserts']) / int(baseline_result['LLC_Traffic']) if int(baseline_result['LLC_Traffic']) > 0 else 0.0}"
            counters['MT_lookups'] = f"{int(counters['MT_lookups']) / int(baseline_result['LLC_Traffic']) if int(baseline_result['LLC_Traffic']) > 0 else 0.0}"
            counters['MT_hits'] = f"{int(counters['MT_hits']) / int(baseline_result['LLC_Traffic']) if int(baseline_result['LLC_Traffic']) > 0 else 0.0}"
            counters['L2C_USEFUL'] = f"{int(counters['L2C_USEFUL']) / int(baseline_result['LLC_Traffic']) if int(baseline_result['LLC_Traffic']) > 0 else 0.0}"
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


    with open("utils/tracelist", "r") as f:
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
                    baseline_result[trace] = get_measure(LOG_PATH +"/"+ BASELINE + "/" + (trace+".log"))
                    
                    for pf in PF_LIST:
                        if os.path.exists(LOG_PATH +"/"+ pf + "/" + (trace+".log")):
                            #print("Reading from: " + LOG_PATH +"/"+ pf + "/" + (trace+".log"))
                            result = get_measure(LOG_PATH +"/"+ pf + "/" + (trace+".log"), baseline_result[trace])
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