#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import sys
sys.path.append(str(Path(__file__).parent.parent))
from utils.defs import *

BASE_DIR = Path(__file__).resolve().parent
CSV_PATH = RESULT_PATH / "energy" / "energy_result.csv"

SET_ORDER = ["ligra", "gap", "spec17", "ml", "google", "Average"]
SET_LABELS = {
    "ligra": "Ligra",
    "gap": "GAP",
    "spec17": "SPEC",
    "ml": "ML",
    "google": "Google",
    "Average": "Avg.",
}

PF_LABELS = {
    "triangel": "Triangel",
    "prophet": "Prophet",
    
    "prism": "PRISM",
}

PF_COLORS = {
    "triangel": "#59A3A4",
    "prophet": "#CCDA80",
    
    "prism": "#345470",
}


def plot_energy(csv_path: Path, out_dir: Path) -> None:
    df = pd.read_csv(csv_path)
    required = {"Prefetcher", "Set", "Energy"}
    missing = required.difference(df.columns)
    if missing:
        raise ValueError(f"CSV is missing required columns: {sorted(missing)}")

    df["Prefetcher"] = df["Prefetcher"].astype(str).str.strip()
    df["Set"] = df["Set"].astype(str).str.strip()
    df["Energy"] = pd.to_numeric(df["Energy"], errors="coerce")

    orig_prefetchers = ['Triangel','Prophet',  'PRISM']
    print(orig_prefetchers)
    disp_prefetchers = [PF_LABELS.get(pf, pf) for pf in orig_prefetchers]

    plt.rcParams.update(
        {
            "font.family": "serif",
            "font.serif": ["Times New Roman"],
            "font.size": 9,
        }
    )

    os.makedirs(out_dir, exist_ok=True)

    n_sets = len(SET_ORDER)
    n_pf = len(orig_prefetchers)
    x = np.arange(n_sets)
    bar_width = 0.8 / max(1, n_pf)
    offsets = (np.arange(n_pf) - (n_pf - 1) / 2.0) * bar_width

    # Use a broken y-axis with two stacked panels: low (0-2) and high (7-13)
    fig, (ax_high, ax_low) = plt.subplots(
        2,
        1,
        sharex=True,
        figsize=(3.5, 1.0),
        gridspec_kw={"height_ratios": [1, 1], "hspace": 0.04},
    )

    all_vals: list[np.ndarray] = []
    prefetcher_vals: list[np.ndarray] = []
    for orig_pf in orig_prefetchers:
        vals = []
        for set_name in SET_ORDER:
            row = df[(df["Set"] == set_name) & (df["Prefetcher"] == orig_pf)]
            if row.empty:
                vals.append(np.nan)
            else:
                vals.append(pd.to_numeric(row.iloc[0].get("Energy", np.nan), errors="coerce"))
        arr = np.array(vals, dtype=float)
        prefetcher_vals.append(arr)
        all_vals.append(arr)

    # draw bars on both panels so matplotlib will clip appropriately
    for k, (orig_pf, disp_pf, vals) in enumerate(zip(orig_prefetchers, disp_prefetchers, prefetcher_vals)):
        color = PF_COLORS.get(orig_pf, ["#CCDA80", "#59A3A4", "#345470"][k % 3])
        ax_high.bar(x + offsets[k], vals, width=bar_width, edgecolor="#000000", linewidth=0.2, label=disp_pf, color=color)
        ax_low.bar(x + offsets[k], vals, width=bar_width, edgecolor="#000000", linewidth=0.2, label=disp_pf, color=color)

    # requested fixed panel limits
    low_lim = (0, 2.5)
    high_lim = (5.5, 13)
    ax_low.set_ylim(low_lim)
    ax_high.set_ylim(high_lim)
    

    # format x/y labels and ticks
    ax_high.set_xticks([])
    ax_high.tick_params(axis='x', which='both', bottom=False, labelbottom=False)
    ax_low.set_xticks(x)
    ax_low.set_xticklabels([SET_LABELS.get(s, s) for s in SET_ORDER])
    ax_high.set_ylabel("Normalized Energy")
    ax_high.yaxis.set_label_coords(-0.1 , 0.1)
    ax_low.margins(y=0.02)

    ax_low.yaxis.set_major_locator(plt.MaxNLocator(nbins=3))
    ax_high.yaxis.set_major_locator(plt.MaxNLocator(nbins=4))
    ax_high.set_yticks([7, 10, 13])
    ax_high.set_yticklabels(["7", "10", "13"])
    ax_low.set_yticks([0, 1, 2])
    ax_low.set_yticklabels(["0", "1","2"])
    
    # vertical separator before Average on both panels
    if "Average" in SET_ORDER:
        sep_idx = SET_ORDER.index("Average")
        if sep_idx > 0:
            x_line = (sep_idx - 1 + sep_idx) / 2.0
            for a in (ax_low, ax_high):
                a.axvline(x=x_line, ymin=0, ymax=1.1, color="#CCCCCC", linestyle="--", linewidth=1)

    # Legend from top axis
    handles, labels = ax_high.get_legend_handles_labels()
    fig.legend(
        handles,
        labels,
        ncol=min(6, n_pf),
        fontsize=8,
        loc="upper center",
        bbox_to_anchor=(0.5, 1.12),
        frameon=False,
    )

    # remove the horizontal spine between the two panels so they appear flush
    ax_high.spines["bottom"].set_visible(False)
    ax_low.spines["top"].set_visible(False)

    # draw diagonal break marks at the y-axis break (both left and right)
    d = 0.015  # size of diagonal slashes in axes coordinates
    # top axis: draw slashes at bottom
    kwargs_top = dict(transform=ax_high.transAxes, color="black", clip_on=False, linewidth=0.6)
    ax_high.plot((-d, +d), (-d, +d), **kwargs_top)
    ax_high.plot((1 - d, 1 + d), (-d, +d), **kwargs_top)
    # bottom axis: draw slashes at top
    kwargs_bot = dict(transform=ax_low.transAxes, color="black", clip_on=False, linewidth=0.6)
    ax_low.plot((-d, +d), (1 - d, 1 + d), **kwargs_bot)
    ax_low.plot((1 - d, 1 + d), (1 - d, 1 + d), **kwargs_bot)

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    out_pdf = os.path.join(out_dir, "Fig12-energy.pdf")
    fig.savefig(out_pdf, bbox_inches="tight", pad_inches=0.01)
    print(f"Saved: {out_pdf}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot Energy grouped by Set from energy.csv")
    parser.add_argument("csv", nargs="?", default=f"{CSV_PATH}", help="Path to energy.csv")
    parser.add_argument("--out", default=f"{EXPR_PATH}/figure_out", help="Output directory")
    args = parser.parse_args()
    plot_energy(Path(args.csv), Path(args.out))


if __name__ == "__main__":
    main()
