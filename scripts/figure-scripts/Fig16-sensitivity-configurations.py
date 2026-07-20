"""Normalize cache/dram/l1spatial CSVs for plotting.

Keeps only Average rows and splits the Prefetcher column into two parts.
"""

from __future__ import annotations

from pathlib import Path
from typing import Tuple

import pandas as pd
import sys
sys.path.append(str(Path(__file__).parent.parent))
from utils.defs import *

FILES = [f"{RESULT_PATH}/Fig16/cache/average.csv", f"{RESULT_PATH}/Fig16/dram/average.csv", f"{RESULT_PATH}/Fig16/l1spatial/average.csv"]


def split_prefetcher(value: str) -> Tuple[str, str]:
    if isinstance(value, str) and "." in value:
        left, right = value.split(".", 1)
        return left.strip(), right.strip()
    return value, ""


def clean_file(path: Path) -> None:
    df = pd.read_csv(path)
    df.columns = df.columns.str.strip()

    df = df[df["Set"].str.strip().str.lower() == "average"].copy()
    df = df.drop(columns=["Set"], errors="ignore")

    split_values = df["Prefetcher"].apply(split_prefetcher)
    df.insert(0, "Set", split_values.apply(lambda parts: parts[0]))
    df["Prefetcher"] = split_values.apply(lambda parts: parts[1])

    df.to_csv(path, index=False)
    print(f"Cleaned {path}")


for file_name in FILES:
    clean_file(file_name)




"""Plot cache, DRAM, and L1 sensitivity trends on a single canvas."""

from __future__ import annotations

import csv
from pathlib import Path
from typing import Dict, List, Tuple

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.lines import Line2D


FIG_SIZE = (8, 2)
FONT_FAMILY = "Times New Roman"
FONT_SIZE = 9
CONFIG_TYPES = ["cache", "dram", "l1spatial"]
PREFETCHER_MAP: Dict[str, Tuple[str, str]] = {
    "triangel": ("Triangel", "#CCDA80"),
    "prophet": ("Prophet", "#59A3A4"),
    "prism": ("PRISM", "#345470"),
}




def build_config_map() -> Dict[str, Dict[str, Dict[str, float]]]:
    config_map: Dict[str, Dict[str, Dict[str, float]]] = {}

    for config_type in CONFIG_TYPES:
        data: Dict[str, Dict[str, float]] = {}
        csv_path = RESULT_PATH / "sens" / config_type / "average.csv"
        with csv_path.open(newline="") as fh:
            reader = csv.DictReader(fh)
            for row in reader:
                cfg = row.get("Set", "").strip()
                pf = row.get("Prefetcher", "").strip().lower()
                ipci_str = row.get("IPCI", "").strip()
                if not cfg or not pf or not ipci_str:
                    continue
                try:
                    value = float(ipci_str)
                except ValueError:
                    continue
                data.setdefault(cfg, {})[pf] = value

        normalized: Dict[str, Dict[str, float]] = {}
        for cfg, pf_map in data.items():
            baseline = pf_map.get("baseline")
            if baseline is None or baseline == 0:
                continue
            normalized[cfg] = {
                pf: val / baseline
                for pf, val in pf_map.items()
                if pf != "baseline"
            }
        config_map[config_type] = normalized

    return config_map


def format_cache_label(key: str) -> str:
    if key == "cache0.5_2":
        return "(0.5, 2)"
    if not key.startswith("cache"):
        return key
    try:
        rest = key[len("cache") :]
        l1, l2 = rest.split("_", 1)
        return f"({int(l1)}, {int(l2)})"
    except (ValueError, TypeError):
        return key


def cache_order(keys: List[str]) -> List[str]:
    def sort_key(name: str) -> Tuple[int, int, str]:
        if name.startswith("cache"):
            try:
                rest = name[len("cache") :]
                l1, l2 = rest.split("_", 1)
                if name == "cache0.5_2":
                    return (0.5, 2, name)
                return (int(l1), int(l2), name)
            except ValueError:
                pass
        return (9999, 9999, name)

    return sorted(keys, key=sort_key)


def dram_order(keys: List[str]) -> List[str]:
    def sort_key(name: str) -> Tuple[int, str]:
        try:
            return (int(name), name)
        except ValueError:
            return (9999, name)

    return sorted(keys, key=sort_key)


def compute_ylim(values: List[float], padding_ratio: float = 0.05) -> Tuple[float, float]:

    finite_vals = [v for v in values if np.isfinite(v)]
    if not finite_vals:
        return (0.0, 1.0)
    vmin = min(finite_vals)
    vmax = max(finite_vals)
    span = vmax - vmin
    padding = span * padding_ratio if span > 0 else 0.05
    lower = max(0.0, vmin - padding)
    upper = vmax + padding
    return (lower, upper)


def plot_cache(ax: plt.Axes, cache_data: Dict[str, Dict[str, float]]) -> None:
    configs = cache_order(list(cache_data.keys()))
    x = np.arange(len(configs))
    all_values: List[float] = []

    for pf_key, (label, color) in PREFETCHER_MAP.items():
        y = [cache_data.get(cfg, {}).get(pf_key, np.nan) for cfg in configs]
        y_arr = np.array(y, dtype=float)
        if not np.isfinite(y_arr).any():
            continue
        if pf_key == "prism":
            ax.plot(x, y_arr, marker="o", markeredgewidth=0.3, markeredgecolor="black", linewidth=1.5, color=color)
        elif pf_key == "triangel":
            ax.plot(x, y_arr, marker="v", markeredgewidth=0.3, markeredgecolor="black", linewidth=1.5, color=color)
        elif pf_key == "prophet":
            ax.plot(x, y_arr, marker="^", markeredgewidth=0.3, markeredgecolor="black", linewidth=1.5, color=color)
        all_values.extend(y_arr[np.isfinite(y_arr)].tolist())

    ax.set_xticks(x)
    ax.set_xticklabels([format_cache_label(cfg) for cfg in configs], rotation=0)
    ax.set_ylabel("Speedup")
    ax.set_ylim(0.95, 1.11)
    ax.set_yticks([0.95, 1.00, 1.05,1.10])
    ax.set_yticklabels([f"0.95", "1.00", "1.05", "1.10"])
    # ax.set_ylim(compute_ylim(all_values))
    # vmin, vmax = compute_ylim(all_values)

    # vavg = (vmin + vmax) / 2
    # ax.set_yticks([vmin, vavg, vmax])
    # ax.set_yticklabels([f"{vmin:.2f}", f"{vavg:.2f}", f"{vmax:.2f}"])
    ax.axhline(y=1.0, color="#CCCCCC", linestyle="--", linewidth=1)
    ax.text(0.5, -0.3, "(a) (L2, L3) Cache Size, in MB", transform=ax.transAxes, ha="center", va="top", fontsize=FONT_SIZE)


def plot_dram(ax: plt.Axes, dram_data: Dict[str, Dict[str, float]]) -> None:
    configs = dram_order(list(dram_data.keys())) 
    x = np.arange(len(configs))
    all_values: List[float] = []

    for pf_key, (_, color) in PREFETCHER_MAP.items():
        y = [dram_data.get(cfg, {}).get(pf_key, np.nan) for cfg in configs]
        y_arr = np.array(y, dtype=float)
        if not np.isfinite(y_arr).any():
            continue
        if pf_key == "prism":
            ax.plot(x, y_arr, marker="o", linewidth=1.5,markeredgewidth=0.3,markeredgecolor="black", color=color)
        elif pf_key == "triangel":
            ax.plot(x, y_arr, marker="v", linewidth=1.5,markeredgewidth=0.3,markeredgecolor="black", color=color)
        elif pf_key == "prophet":
            ax.plot(x, y_arr, marker="^", linewidth=1.5,markeredgewidth=0.3,markeredgecolor="black", color=color)
        all_values.extend(y_arr[np.isfinite(y_arr)].tolist())

    ax.set_xticks(x)
    ax.set_xticklabels(configs, rotation=0)

    ax.set_ylim(0.95, 1.11)
    ax.set_yticks([])
    ax.set_yticklabels([])
    
    # ax.set_ylim(compute_ylim(all_values))
    # vmin, vmax = compute_ylim(all_values)
    # vavg = (vmin + vmax) / 2
    # ax.set_yticks([vmin, vavg, vmax])
    # ax.set_yticklabels([f"{vmin:.2f}", f"{vavg:.2f}", f"{vmax:.2f}"])
    ax.axhline(y=1.0, color="#CCCCCC", linestyle="--", linewidth=1)
    ax.text(0.5, -0.3, "(b) DRAM Bandwidth (MT/s)", transform=ax.transAxes, ha="center", va="top", fontsize=FONT_SIZE)


def plot_l1(ax: plt.Axes, l1_data: Dict[str, Dict[str, float]]) -> None:
    base_order = ["Stride", "IPCP", "Berti"]
    configs = [cfg for cfg in base_order if cfg in l1_data]
    configs += [cfg for cfg in sorted(l1_data.keys()) if cfg not in configs]
    x = np.arange(len(configs))
    all_values: List[float] = []

    for pf_key, (_, color) in PREFETCHER_MAP.items():
        y = [l1_data.get(cfg, {}).get(pf_key, np.nan) for cfg in configs]
        y_arr = np.array(y, dtype=float)
        if not np.isfinite(y_arr).any():
            continue
        if pf_key == "prism":
            ax.plot(x, y_arr, marker="o", markeredgewidth=0.3, markeredgecolor="black", linewidth=1.5, color=color)
        elif pf_key == "triangel":
            ax.plot(x, y_arr, marker="v", markeredgewidth=0.3, markeredgecolor="black", linewidth=1.5, color=color)
        elif pf_key == "prophet":
            ax.plot(x, y_arr, marker="^", markeredgewidth=0.3, markeredgecolor="black", linewidth=1.5, color=color)
        all_values.extend(y_arr[np.isfinite(y_arr)].tolist())

    ax.set_xticks(x)
    ax.set_xticklabels(configs, rotation=0)
    ax.set_ylim(0.95, 1.11)
    ax.set_yticks([])
    ax.set_yticklabels([])
    
    # ax.set_ylim(compute_ylim(all_values))
    # vmin, vmax = compute_ylim(all_values)
    # vavg = (vmin + vmax) / 2
    # ax.set_yticks([vmin, vavg, vmax])
    # ax.set_yticklabels([f"{vmin:.2f}", f"{vavg:.2f}", f"{vmax:.2f}"])
    ax.axhline(y=1.0, color="#CCCCCC", linestyle="--", linewidth=1)
    ax.text(0.5, -0.3, "(c) L1 Spatial Prefetcher", transform=ax.transAxes, ha="center", va="top", fontsize=FONT_SIZE)


def main() -> None:
    plt.rcParams.update({"font.family": FONT_FAMILY, "font.size": FONT_SIZE})

    config_map = build_config_map()
    cache_data = config_map.get("cache", {})
    dram_data = config_map.get("dram", {})
    l1_data = config_map.get("l1spatial", {})

    fig, axes = plt.subplots(1, 3, figsize=FIG_SIZE)

    plot_cache(axes[0], cache_data)
    plot_dram(axes[1], dram_data)
    plot_l1(axes[2], l1_data)

    legend_handles = [
        Line2D([0], [0], color=color, marker=marker, markeredgewidth=0.3, markeredgecolor="black", linewidth=1.5, label=label)
        for (label, color), marker in zip(PREFETCHER_MAP.values(),["v","^","o"])
    ]
    fig.legend(
        legend_handles,
        [label for label, _ in PREFETCHER_MAP.values()],
        loc="upper center",
        ncol=len(PREFETCHER_MAP),
        frameon=False,
        bbox_to_anchor=(0.5, 0.92),
    )

    fig.subplots_adjust(top=0.78, bottom=0.35, left=0.07, right=0.98, wspace=0.1)
    
    output_path = RESULT_PATH / "figure_out" / "Fig16-sensitivity-configurations.pdf"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=300, bbox_inches="tight", pad_inches=0.01)
    plt.close(fig)
    print(f"Saved figure to {output_path}")


if __name__ == "__main__":
    main()
