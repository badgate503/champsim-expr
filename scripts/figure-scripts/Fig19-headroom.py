#!/usr/bin/env python3
"""Plot IPCI (Speedup) for all Prefetchers across Sets from opt.csv.

Copied structure from ACC-TIME-MDT/spd.py but defaults to `opt.csv` and
outputs into `results/Rebuttal`.
"""
import argparse
import os
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

from pathlib import Path

sys.path.append(str(Path(__file__).parent.parent))
from utils.defs import *


BASE_DIR = Path(__file__).resolve().parent

def plot_ipci(csv_path, out_dir):
    df = pd.read_csv(csv_path)

    # order of Sets to show (same as plot_prefetchers_metrics.py)
    sets = ["ligra", "gap", "spec17","ml", "google",  "Average"]
    set_alias = {
        "ligra": "Ligra",
        "spec17": "SPEC",
        "ml": "ML",
        "google": "Google",
        "gap": "GAP",
        "Average": "Avg.",
    }

    pf_alias = {
        "triangel": "Triangel",
        "prophet": "Prophet",
        "prism": "PRISM",
        "baseline-inf": "Baseline-Inf",
        "ideal-tp": "Ideal-TP",
    }

    # Prepare prefetchers (original names from CSV, with display names)
    orig_prefetchers = ["prism","baseline-inf","ideal-tp"]
    # Ensure PRISM comes before ideal-tp (place ideal-tp to the right of PRISM)
    if "prism" in orig_prefetchers and "ideal-tp" in orig_prefetchers:
        pi = orig_prefetchers.index("prism")
        oi = orig_prefetchers.index("ideal-tp")
        if oi < pi:
            # move ideal-tp to just after prism
            orig_prefetchers.pop(oi)
            pi = orig_prefetchers.index("prism")
            orig_prefetchers.insert(pi + 1, "ideal-tp")

    disp_prefetchers = [pf_alias.get(pf, pf) for pf in orig_prefetchers]

    # Plot styling: 9pt Times New Roman
    plt.rcParams.update({
        "font.family": "serif",
        "font.serif": ["Times New Roman"],
        "font.size": 9,
    })

    os.makedirs(out_dir, exist_ok=True)
    from matplotlib.colors import ListedColormap
    cmap = ListedColormap(["#345470","#5C8FB1", "#B2DAD8", ])
    colors = list(cmap.colors)

    # Ensure display name for 'ideal-tp' is 'Optimal'
    pf_alias.update({"ideal-tp": "Optimal"})

    n_sets = len(sets)
    n_pf = len(orig_prefetchers)
    x = np.arange(n_sets)
    bar_width = 0.8 / max(1, n_pf)
    offsets = (np.arange(n_pf) - (n_pf - 1) / 2.0) * bar_width

    fig, ax = plt.subplots(1, 1, figsize=(3.5, 1.2))

    # Collect values for all prefetchers first to compute y-limits
    all_vals = []
    prefetcher_vals = []
    for orig_pf in orig_prefetchers:
        vals = []
        for s in sets:
            row = df[(df.get("Set") == s) & (df["Prefetcher"] == orig_pf)]
            if row.empty:
                vals.append(np.nan)
            else:
                vals.append(pd.to_numeric(row.iloc[0].get("IPCI", np.nan), errors="coerce"))
        vals = np.array(vals, dtype=float)
        prefetcher_vals.append(vals)
        all_vals.append(vals)

    # Flatten to compute global min/max
    if len(all_vals) > 0:
        stacked = np.vstack(all_vals)
        global_min = np.nanmin(stacked)
        global_max = np.nanmax(stacked)
    else:
        global_min, global_max = 0.0, 1.0

    # Set nice margin for ylim
    span = global_max - global_min
    if span == 0 or np.isnan(span):
        margin = 0.01 * max(1.0, abs(global_max))
    else:
        margin = span * 0.08
    y_bottom = max(0.0, global_min - margin)
    y_top = global_max + margin

    # Now draw bars with consistent colors
    for k, (orig_pf, disp_pf, vals) in enumerate(zip(orig_prefetchers, disp_prefetchers, prefetcher_vals)):
        color = colors[k % len(colors)]
        ax.bar(x + offsets[k], vals, width=bar_width, edgecolor="#000000", linewidth=0.2, label=disp_pf, color=color)

        if orig_pf == "ideal-tp" and "ml" in sets:
            ml_idx = sets.index("ml")
            ml_value = vals[ml_idx]
            if not np.isnan(ml_value):
                ax.text(
                    x[ml_idx] + offsets[k],
                    1.41,
                    f"{ml_value:.2f}",
                    ha="center",
                    va="bottom",
                    fontsize=7,
                    fontfamily="Times New Roman",
                    color="#222222",
                    clip_on=False,
                )
            gap_idx = sets.index("gap")
            gap_value = vals[gap_idx]
            if not np.isnan(gap_value):
                ax.text(
                    x[gap_idx] + offsets[k],
                    1.41,
                    f"{gap_value:.2f}",
                    ha="center",
                    va="bottom",
                    fontsize=7,
                    fontfamily="Times New Roman",
                    color="#222222",
                    clip_on=False,
                )
            avg_idx = sets.index("Average")
            avg_value = vals[avg_idx]
            if not np.isnan(avg_value):
                ax.text(
                    x[avg_idx] + offsets[k],
                    1.41,
                    f"{avg_value:.2f}",
                    ha="center",
                    va="bottom",
                    fontsize=7,
                    fontfamily="Times New Roman",
                    color="#222222",
                    clip_on=False,
                )

    ax.set_xticks(x)
    ax.set_xticklabels([set_alias.get(s, s) for s in sets])
    ax.set_ylabel("Speedup")
    ax.set_xlabel("")
    ax.margins(y=0.02)
    ax.set_ylim(bottom=1, top=1.4)
    # Choose 4 y-ticks between bottom and top
    yticks = [1.00, 1.2, 1.4]
    ax.set_yticks(yticks)
    ax.set_yticklabels([f"{t:.2f}" for t in yticks])
    #ax.axhline(y=1.0, color="gray", linestyle="--", linewidth=1)

    # Draw dashed separator between the main groups and 'Average' (placed at the far right)
    if 'Average' in sets:
        sep_idx = sets.index('Average')
        if sep_idx > 0:
            x_line = (sep_idx - 1 + sep_idx) / 2.0
            ax.axvline(x=x_line,ymin=0, ymax=1.1, color='#CCCCCC', linestyle='--', linewidth=1)

    # Legend at top, no frame
    handles, labels = ax.get_legend_handles_labels()
    fig.legend(handles, labels, ncol=min(6, n_pf), fontsize=8, loc='upper center', bbox_to_anchor=(0.55, 1.01), frameon=False)

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    out_pdf = os.path.join(out_dir, "Fig19-headroom.pdf")
    fig.savefig(out_pdf, bbox_inches="tight", pad_inches=0.01)
    print(f"Saved: {out_pdf}")


def main():
    parser = argparse.ArgumentParser(description="Plot IPCI (Speedup) grouped by Set from opt.csv")
    parser.add_argument("csv", nargs="?", default=f"{RESULT_PATH}/Fig19/average.csv", help="Path to opt.csv")
    parser.add_argument("--out", default=f"{FIGURE_PATH}", help="Output directory")
    args = parser.parse_args()
    plot_ipci(args.csv, args.out)


if __name__ == '__main__':
    main()
