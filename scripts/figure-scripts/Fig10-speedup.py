
#!/usr/bin/env python3
"""Plot IPCI (Speedup) for all Prefetchers across Sets from average.csv.

Each Set is a group of bars; each group contains one bar per Prefetcher.
- Uses the same `set_alias` and `pf_alias` mappings as `plot_prefetchers_metrics.py`.
- Legend is placed at the top without a frame. Colors use the required cmap.
- Font: Times New Roman, 9pt.
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
        "prism": "PRISM"
    }

    # Prepare prefetchers (original names from CSV, with display names)
    orig_prefetchers = list(df["Prefetcher"].astype(str).unique())
    disp_prefetchers = [pf_alias.get(pf, pf) for pf in orig_prefetchers]

    # Plot styling: 9pt Times New Roman
    plt.rcParams.update({
        "font.family": "serif",
        "font.serif": ["Times New Roman"],
        "font.size": 9,
    })

    os.makedirs(out_dir, exist_ok=True)
    from matplotlib.colors import ListedColormap
    cmap = ListedColormap(["#CCDA80", "#59A3A4", "#345470"])
    colors = list(cmap.colors)

    n_sets = len(sets)
    n_pf = len(orig_prefetchers)
    x = np.arange(n_sets)
    bar_width = 0.8 / max(1, n_pf)
    offsets = (np.arange(n_pf) - (n_pf - 1) / 2.0) * bar_width

    fig, ax = plt.subplots(1, 1, figsize=(3.5, 1.25))

    for k, (orig_pf, disp_pf) in enumerate(zip(orig_prefetchers, disp_prefetchers)):
        vals = []
        for s in sets:
            row = df[(df["Set"] == s) & (df["Prefetcher"] == orig_pf)]
            if row.empty:
                vals.append(np.nan)
            else:
                vals.append(pd.to_numeric(row.iloc[0].get("IPCI", np.nan), errors="coerce"))
        vals = np.array(vals, dtype=float)
        color = colors[k % len(colors)]
        ax.bar(x + offsets[k], vals, width=bar_width, label=disp_pf, color=color, edgecolor="black", linewidth=0.2)

    ax.set_xticks(x)
    ax.set_xticklabels([set_alias.get(s, s) for s in sets])
    ax.set_ylabel("Speedup")
    ax.set_xlabel("")
    ax.margins(y=0.1)
    ax.set_ylim(bottom=0.95,top=1.11)
    ax.set_yticks([0.95, 1.0, 1.05, 1.10])
    ax.set_yticklabels(["0.95", "1.00", "1.05", "1.10"])
    ax.axhline(y=1.0, color="gray", linestyle="--", linewidth=1)

    # Draw dashed separator between the main groups and 'Others' (placed at the far right)
    if 'Average' in sets:
        sep_idx = sets.index('Average')
        if sep_idx > 0:
            x_line = (sep_idx - 1 + sep_idx) / 2.0
            ax.axvline(x=x_line,ymin=0, ymax=1.1, color='#CCCCCC', linestyle='--', linewidth=1)
    # Legend at top, no frame
    handles, labels = ax.get_legend_handles_labels()
    fig.legend(handles, labels, ncol=min(6, n_pf), fontsize=8, loc='upper center', bbox_to_anchor=(0.55, 1.02), frameon=False)

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    out_pdf = os.path.join(out_dir, "Fig10-speedup.pdf")
    fig.savefig(out_pdf, bbox_inches="tight", pad_inches=0.01)
    print(f"Saved: {out_pdf}")


def main():
    parser = argparse.ArgumentParser(description="Plot IPCI (Speedup) grouped by Set from average.csv")
    parser.add_argument("csv", nargs="?", default=f"{RESULT_PATH}/basic/average.csv", help="Path to average.csv")
    parser.add_argument("--out", default=f"{FIGURE_PATH}", help="Output directory")
    args = parser.parse_args()
    plot_ipci(args.csv, args.out)


if __name__ == '__main__':
    main()
