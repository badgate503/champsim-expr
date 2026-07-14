#!/usr/bin/env python3
"""Plot Accuracy, Timeliness and Metadata Traffic for each dataset's prefetchers.

Assumptions:
- Accuracy -> `MT_accuracy`
- Timeliness -> `L2C_Timeliness`
- Metadata Traffic -> `MT_lookups`

Produces a grid with one row per dataset (ligra, spec17, ml, google, gap)
and three columns (Accuracy, Timeliness, Metadata Traffic). Uses 9pt Times New Roman.
"""
import argparse
import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt


def plot_metrics_grouped(csv_path, out_dir):
    df = pd.read_csv(csv_path)

    # order of Sets to show
    sets = ["ligra", "gap", "spec17","ml",
            "google",  "Average"]
    set_alias ={
        "ligra": "Ligra",
        "spec17": "SPEC",
        "ml": "ML",
        "google": "Google",
        "gap": "GAP",
        "Average": "Avg."
    }
    pf_alias = {
        "triangel": "Triangel",
        "prophet": "Prophet",
        "prism":"PRISM"
    }
    pf_alias_rev = {
        "Triangel": "triangel",
        "Prophet": "prophet",
        "PRISM": "prism"
    }


    # Use union of all prefetchers so each Set group has same bars
    prefetchers = list(df["Prefetcher"].astype(str).unique())
    prefetchers = [pf_alias.get(pf, pf) for pf in prefetchers]
    metrics = [
        ("Accuracy", "L2C_Accuracy"),
        ("Timeliness", "L2C_Timeliness"),
        ("DRAM Traffic", "DRAM_Traffic"),
    ]

    # Font: Times New Roman, 9pt
    plt.rcParams.update({
        "font.family": "serif",
        "font.serif": ["Times New Roman"],
        "font.size": 9,
    })

    os.makedirs(out_dir, exist_ok=True)
    from matplotlib.colors import ListedColormap
    # Create single figure with 1 row x 3 cols: left, middle, right
    cmap = ListedColormap(["#CCDA80", "#59A3A4", "#345470"])

    fig, axes = plt.subplots(1, len(metrics), figsize=(8, 1.7))

    n_sets = len(sets)
    n_pf = len(prefetchers)
    x = np.arange(n_sets)
    bar_width = 0.8 / max(1, n_pf)
    offsets = (np.arange(n_pf) - (n_pf - 1) / 2.0) * bar_width

    for ax, (mname, col) in zip(axes, metrics):
        for k, pf in enumerate(prefetchers):
            vals = []
            for s in sets:
                row = df[(df["Set"] == s) & (df["Prefetcher"] == pf_alias_rev[pf])]
                if row.empty:
                    vals.append(np.nan)
                else:
                    vals.append(pd.to_numeric(row.iloc[0].get(col, np.nan), errors="coerce"))
            vals = np.array(vals, dtype=float)
            bars = ax.bar(x + offsets[k], vals, width=bar_width, label=pf if k == 0 or True else None, color=cmap(k % 10), edgecolor="black", linewidth=0.2)
            # Annotate values exceeding 1.2 in DRAM Traffic with arrow pointing to bar top
            if mname == "DRAM Traffic":
                for xpos, val in zip(x + offsets[k], vals):
                    if val > 1.2:
                        ax.annotate(
                            f"{val:.2f}",
                            xy=(xpos, 1.194),
                            xytext=(xpos, 1.21),
                            ha="center",
                            va="bottom",
                            fontsize=8,
                            #arrowprops=dict(arrowstyle="-", lw=0.5, color="black"),
                        )

        ax.set_xticks(x)
        ax.set_xticklabels([set_alias.get(s, s) for s in sets])
        ax.axvline(x=4.5, color="#CCCCCC", linestyle="--", linewidth=0.6)
        if mname == "Accuracy":
            ax.set_xlabel("(a) Accuracy")
            ax.set_ylim(0,1.0)
            ax.set_yticks([0,0.25,0.5,0.75,1.0])
            ax.set_yticklabels(["0","0.25","0.50","0.75","1.00"])
        elif mname == "Timeliness":
            ax.set_xlabel("(b) Timeliness")
            ax.set_ylim(0,1.0)
            ax.set_yticks([0,0.25,0.5,0.75,1.0])
            ax.set_yticklabels(["0","0.25","0.50","0.75","1.00"])
        elif mname == "DRAM Traffic":
            ax.set_xlabel("(c) DRAM Traffic")
        if mname == "DRAM Traffic":
            ax.set_ylim(0.8,1.2)
            ax.set_yticks([0.8,0.9,1.0,1.1,1.2])
            ax.set_yticklabels(["0.80","0.90","1.00","1.10","1.20"])
            ax.axhline(y=1.0, color="gray", linestyle="--", linewidth=0.6)
        ax.margins(y=0.1)

    # Put legend below the subplots
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, ncol=min(6, n_pf), fontsize=8, loc="upper center", bbox_to_anchor=(0.5, 1), frameon=False)
    plt.tight_layout(rect=[0, 0.03, 1, 1])

    out_pdf = os.path.join(out_dir, "acc_time_traffic.pdf")
    fig.savefig(out_pdf, bbox_inches="tight", pad_inches=0.01)
    print(f"Saved: {out_pdf}")



def main():
    parser = argparse.ArgumentParser(description="Plot grouped prefetcher metrics from average.csv")
    parser.add_argument("csv", nargs="?", default="../results/basic/average.csv", help="Path to average.csv")
    parser.add_argument("--out", default="../figure-out", help="Output directory")
    args = parser.parse_args()
    plot_metrics_grouped(args.csv, args.out)


if __name__ == "__main__":
    main()
