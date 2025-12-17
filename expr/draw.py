#!/usr/bin/env python3

import argparse
import re
from utils.get_measure import *

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

import os
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument("--baseline", "-b", help="use baseline")
parser.add_argument("--tracelist", "-t", help="use predefined list in list.txt")
parser.add_argument("--pflist", "-p", help="use predefined list in list.txt")
parser.add_argument("--measure", "-m", choices=['ipc', 'pfissue','pfuseful','pfaccuracy'])

args = parser.parse_args()

def sanitize_filename(name):
    """把 measure 名转成安全的文件名（替换空格和特殊字符）"""
    return "".join(c if c.isalnum() or c in "-_." else "_" for c in name)

def plot_measures(df,bottom, out_dir="plots", measures=None, figsize=(25, 10),
                  dpi=150, show=False):
    required_cols = {"measure", "workload", "config", "value"}
    if not required_cols.issubset(set(df.columns)):
        raise ValueError(f"df 必须包含列: {required_cols}")

    os.makedirs(out_dir, exist_ok=True)

    if measures is None:
        measures = sorted(df['measure'].unique())

    for measure in measures:
        sub = df[df['measure'] == measure]
        if sub.empty:
            continue

        # pivot 成矩阵：index=workload, columns=config, values=value
        config_order = sub['config'].drop_duplicates()
        pivot = sub.pivot_table(index='workload', columns='config', values='value', aggfunc='mean')
        pivot = pivot[config_order]
        workloads = list(pivot.index)
        configs = list(pivot.columns)
        n_workloads = len(workloads)
        n_configs = len(configs)

        # x 坐标和宽度设置
        x = np.arange(n_workloads)
        total_width = 0.8  # 每组总宽度（不占满 x tick）
        bar_width = total_width / n_configs

        fig, ax = plt.subplots(figsize=figsize, dpi=dpi)
        #BOTTOM = 0.9
        # 绘制每个 config 的柱子（颜色由 matplotlib 自动分配）
        for i, cfg in enumerate(configs):
            # 每个 config 的偏移
            offset = (i - (n_configs - 1) / 2) * bar_width
            heights = pivot[cfg].values
            ax.bar(x + offset, heights-bottom, width=bar_width, label=str(cfg),bottom=bottom)

        # 格式化轴与标签
        ax.set_xticks(x)
        ax.set_xticklabels(workloads, rotation=30, ha='right')
        ax.set_xlabel("Workload")
        ax.set_ylabel(measure)
        ax.set_title(measure)
        ax.legend(title="Config", bbox_to_anchor=(1.02, 1), loc="upper left")
        ax.grid(axis='y', linestyle='--', linewidth=0.5, alpha=0.7)

        plt.tight_layout()

        out_path = os.path.join(out_dir, f"{sanitize_filename(measure)}.png")
        plt.savefig(out_path)
        if show:
            plt.show()
        plt.close(fig)


data = []

class PltSet:
    def __init__(self, measure_alias, workloads, configs, baseline_config, use_baseline, bottom, get):  # 构造函数，初始化对象
        self.measure_alias = measure_alias
        self.workloads = workloads
        self.configs = configs
        self.baseline_config = baseline_config
        self.use_baseline = use_baseline
        self.bottom = bottom
        self.get = get
    
    def plot(self):
        rows = []
        for w in self.workloads:
            if self.use_baseline:
                baseline_val = self.get(w,self.baseline_config)
                for c in self.configs:
                    if c == self.baseline_config:
                        continue
                    val = self.get(w,c)
                    print(f"{GREEN}{w:<30}{END}|{GREEN}{c:<10}{END}|{CYAN}{self.measure_alias:<5}{END} = {val}")
                    rows.append({"measure": self.measure_alias, "workload": w, "config": c, "value": val/baseline_val})
            else:
                for c in self.configs:
                    val = self.get(w,c)
                    print(f"{GREEN}{w:<30}{END}|{GREEN}{c:<10}{END}|{CYAN}{self.measure_alias:<5}{END} = {val}")
                    rows.append({"measure": self.measure_alias, "workload": w, "config": c, "value": val})

        df = pd.DataFrame(rows)

        # 生成图表并保存在 ./plots
        plot_measures(df,self.bottom, out_dir="plots", show=False)
        print("Done. Plots saved to ./plots/")


if __name__ == "__main__":
    name = f"{args.measure}_{args.tracelist}_{args.pflist}"
    if args.baseline != None:
        name += f"_baseline-{args.baseline}"
    if args.tracelist:
        result = {}
        with open("./tracelist", "r") as f:
            for line in f:
                line = line.strip()
                if not line or ":" not in line:
                    continue

                key, values = line.split(":", 1)
                key = key.strip()
                value_list = values.strip().split()

                result[key] = value_list
        if args.tracelist in result.keys():
            traces = result[args.tracelist]
    else:
        print("Please input trace list.")
        sys.exit(0)
    if args.pflist:
        result = {}
        with open("./pflist", "r") as f:
            for line in f:
                line = line.strip()
                if not line or ":" not in line:
                    continue

                key, values = line.split(":", 1)
                key = key.strip()
                value_list = values.strip().split()

                result[key] = value_list
        if args.pflist in result.keys():
            prefetchers = result[args.pflist]
    else:
        print("Please input prefetcher list.")
        sys.exit(0)
    get_func    = {"ipc":get_ipc, "pfissue":get_l2cpf_issue, "pfuseful":get_l2cpf_useful, "pfaccuracy":get_l2cpf_accuracy}[args.measure]

    PltSet(
           measure_alias=   name,
           workloads=       traces,
           configs=         prefetchers,
           baseline_config= args.baseline,
           use_baseline=    (args.baseline != None), 
           bottom=          0,
           get = get_func).plot()