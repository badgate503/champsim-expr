import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import sys
from pathlib import Path
from typing import Optional

sys.path.append(str(Path(__file__).resolve().parent.parent))
from get_data import get_expr_result


BASE_DIR = Path(__file__).resolve().parent
ALL_CSV = BASE_DIR / "all.csv"
OUT_PATH = BASE_DIR.parent / "results" / "SPD-TML-ACC" / "speedup-timeliness-accuracy.pdf"


def parse_pat_size(prefetcher: str) -> Optional[int]:
    if not isinstance(prefetcher, str):
        return None
    if not prefetcher.startswith("pctp") or not prefetcher.endswith("k"):
        return None
    try:
        return int(prefetcher.removeprefix("pctp").removesuffix("k"))
    except ValueError:
        return None


def load_left_series() -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, list[str]]:
    prefix = "pctpinf.a"
    x_labels = [f"{prefix}{i}" for i in [1, 2, 4, 8, 12, 16]]
    accuracy = []
    timeliness = []
    speedup = []
    for x_label in x_labels:
        result = get_expr_result(x_label, "google")
        print(result)
        accuracy.append(result["PCM_accuracy"])
        timeliness.append(1 - result["PCM_laterate"])
        speedup.append(result["IPCI"])

    accuracy = np.array(accuracy)
    timeliness = np.array(timeliness)
    speedup = np.array(speedup)

    n = min(len(accuracy), len(timeliness), len(speedup))
    accuracy = accuracy[:n]
    timeliness = timeliness[:n]
    speedup = speedup[:n]
    labels = [f"{i}" for i in [1, 2, 4, 8, 12, 16]]
    if len(labels) != n:
        labels = [str(i) for i in range(n)]
    x = np.arange(n)
    return x, accuracy, timeliness, speedup, labels


def load_google_all_data(csv_path: Path) -> pd.DataFrame:
    df = pd.read_csv(csv_path)
    df.columns = df.columns.str.strip()
    df["Set"] = df["Set"].astype(str).str.strip()
    df["Prefetcher"] = df["Prefetcher"].astype(str).str.strip()
    df["IPCI"] = pd.to_numeric(df["IPCI"], errors="coerce")
    df["PCM_useful_prefetches"] = pd.to_numeric(df["PCM_useful_prefetches"], errors="coerce")

    return df[df["Set"].str.lower() == "google"].copy()


def style_right_axis(ax: plt.Axes) -> None:

    ax.yaxis.tick_left()
    ax.yaxis.set_label_position("left")


def main() -> None:
    plt.rcParams.update(
        {
            "font.family": "Times New Roman",
            "font.size": 9,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
        }
    )

    x, accuracy, timeliness, speedup, labels = load_left_series()
    google_df = load_google_all_data(ALL_CSV)
    if google_df.empty:
        raise ValueError("No Google data found in SPD-TML-ACC/all.csv")

    base_row = google_df[google_df["Prefetcher"] == "pctpinf.a8"]
    if base_row.empty:
        raise ValueError("Missing pctpinf.a8 baseline in SPD-TML-ACC/all.csv")
    base_useful = float(base_row["PCM_useful_prefetches"].iloc[0])
    if base_useful == 0.0:
        raise ValueError("pctpinf.a8 PCM_useful_prefetches is zero; cannot compute coverage")

    pat_order = [12, 24, 48, 96, 144, 192]
    plot_df = google_df.copy()
    plot_df["PAT"] = plot_df["Prefetcher"].map(parse_pat_size)
    plot_df = plot_df[plot_df["PAT"].isin(pat_order)].copy()
    plot_df["PAT"] = plot_df["PAT"].astype(int)
    plot_df["PAT"] = pd.Categorical(plot_df["PAT"], categories=pat_order, ordered=True)
    plot_df = plot_df.sort_values("PAT")

    pat_x = np.arange(len(plot_df), dtype=float)
    pat_labels = [f"{int(v)}" for v in plot_df["PAT"].to_list()]
    useful_coverage = plot_df["PCM_useful_prefetches"].to_numpy(dtype=float) / base_useful
    pat_speedup = plot_df["IPCI"].to_numpy(dtype=float)

    COLOR_ACC = "#2F8AC4"
    COLOR_TIME = "#196553"
    COLOR_SPEED = "#5D69B1"
    COLOR_PAT = "#345470"

    fig = plt.figure(figsize=(3.5, 2))
    outer = fig.add_gridspec(1, 2, width_ratios=[1.1, 1.1], wspace=0.28)
    left = outer[0].subgridspec(10, 1, hspace=1.8)
    right = outer[1].subgridspec(10, 1, hspace=1.8)

    ax_top = fig.add_subplot(left[:6])
    ax_bottom = fig.add_subplot(left[6:], sharex=ax_top)
    ax_right_top = fig.add_subplot(right[:6])
    ax_right_bottom = fig.add_subplot(right[6:], sharex=ax_right_top)

    ax_top.plot(x, accuracy, marker="^", linewidth=1.5, markersize=3.0,markeredgecolor="black", markeredgewidth=0.2, color=COLOR_ACC, label="Accuracy")
    ax_top.plot(x, timeliness, marker="v", linewidth=1.5, markersize=3.0,markeredgecolor="black", markeredgewidth=0.2, color=COLOR_TIME, label="Timeliness")
    ax_top.set_ylim(0.5, 1.0)
    ax_top.set_yticks([0.5,0.75,1.0])
    ax_top.set_yticklabels(["0.50", "0.75", "1.00"])
    ax_top.set_ylabel("Metrics")
    ax_top.grid(ls="--", alpha=0.35)
    
    ax_top.legend(loc="lower right", frameon=False, fontsize=7, handlelength=1.1, handletextpad=0.1, columnspacing=0.4,ncol=2)
    ax_top.tick_params(axis="x", bottom=False, labelbottom=False)
    ax_bottom.plot(x, speedup, marker="o",markeredgecolor="black", markeredgewidth=0.2, linewidth=1.5, markersize=3.0, color=COLOR_SPEED)
    ax_bottom.set_ylabel("Speedup")

    ax_bottom.set_ylim(1.03, 1.08)
    ax_bottom.set_yticks([1.03, 1.08])
    ax_bottom.set_yticklabels(["1.03", "1.08"])
    ax_bottom.grid(ls="--", alpha=0.35)
    ax_bottom.set_xticks(x)
    ax_bottom.set_xticklabels(labels)
    plt.setp(ax_top.get_xticklabels(), visible=False)

    ax_right_top.plot(pat_x, useful_coverage, marker="^",markeredgecolor="black", markeredgewidth=0.2, linewidth=1.5, markersize=3.0, color=COLOR_ACC, label="Useful Prefetch Coverage")
    ax_right_top.set_ylim(0.0, 1.05)
    ax_right_top.set_yticks([0.0, 0.5, 1.0])
    ax_right_top.set_yticklabels(["0.00", "0.50", "1.00"])
    
    ax_right_top.grid(ls="--", alpha=0.35)
    style_right_axis(ax_right_top)
    ax_right_top.tick_params(axis="x", bottom=False, labelbottom=False)
    ax_right_top.legend(loc="lower right", frameon=False, fontsize=7, handlelength=1.1, handletextpad=0.1, columnspacing=0.4,ncol=2)
    ax_right_bottom.plot(pat_x, pat_speedup, marker="o",markeredgecolor="black", markeredgewidth=0.2, linewidth=1.5, markersize=3.0, color=COLOR_SPEED)

    ax_right_bottom.set_ylim(1.00, 1.10)
    ax_right_bottom.set_yticks([1.0,1.1])
    ax_right_bottom.set_yticklabels(["1.00", "1.10"])
    ax_right_bottom.grid(ls="--", alpha=0.35)
    ax_right_bottom.set_xticks(pat_x)
    ax_right_bottom.set_xticklabels(pat_labels)
    style_right_axis(ax_right_bottom)

    fig.text(0.28, 0.01, "(a) PCQ Depth", ha="center", va="bottom", fontsize=9)
    fig.text(0.8, 0.01, "(b) PAT Size (K)", ha="center", va="bottom", fontsize=9)

    fig.subplots_adjust(left=0.10, right=0.99, top=0.98, bottom=0.18)
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUT_PATH, dpi=1000, bbox_inches="tight", pad_inches=0.01)


if __name__ == "__main__":
    main()
