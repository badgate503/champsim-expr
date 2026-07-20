#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

import matplotlib as mpl

mpl.use("Agg")

import matplotlib.font_manager as fm
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from pathlib import Path
import sys

sys.path.append(str(Path(__file__).parent.parent))
from utils.defs import *

ACC_CSV_PATH = RESULT_PATH / "Fig15" / "sens_acc" / "average.csv"
K_CSV_PATH = RESULT_PATH / "Fig15" / "sens_k" / "average.csv"
OUTPUT_PATH = FIGURE_PATH / "Fig15-sensitivity-tgp-bmp.pdf"
FONT_SIZE_PT = 9
FIG_SIZE = (3.5, 0.9)

ACC_HIGH_VALUES = [55.0, 65.0, 75.0, 85.0, 95.0]
ACC_LOW_VALUES = [5.0, 10.0, 15.0, 20.0, 25.0]
K_VALUES = [1.00, 1.25, 1.50, 1.75, 2.00]


def _parse_acc_prefetcher(name: str) -> tuple[float, float]:
	parts = str(name).split("_")
	if len(parts) != 3 or not parts[0].startswith("acc"):
		raise ValueError(f"Unexpected Prefetcher value: {name}")
	low = parts[1]
	high = parts[2]
	if not low.startswith("low") or not high.startswith("high"):
		raise ValueError(f"Unexpected Prefetcher value: {name}")
	return float(low.replace("low", "")), float(high.replace("high", ""))


def _parse_k_prefetcher(name: str) -> float:
	parts = str(name).split("_")
	if len(parts) < 2 or parts[0] != "k":
		raise ValueError(f"Unexpected Prefetcher value: {name}")
	return float(parts[1])


def _load_dataframe(csv_path: Path) -> pd.DataFrame:
	df = pd.read_csv(csv_path)
	if "Prefetcher" not in df.columns or "IPCI" not in df.columns:
		raise ValueError(f"{csv_path.name} must contain Prefetcher and IPCI columns")
	return df.copy()


def load_acc_average(csv_path: Path) -> pd.DataFrame:
	df = _load_dataframe(csv_path)
	if "Set" in df.columns:
		df = df[df["Set"] == "Average"].copy()
	parsed = df["Prefetcher"].apply(_parse_acc_prefetcher)
	df["low"] = parsed.apply(lambda item: item[0])
	df["high"] = parsed.apply(lambda item: item[1])
	return df


def load_k_average(csv_path: Path) -> pd.DataFrame:
	df = _load_dataframe(csv_path)
	if "Set" in df.columns:
		df = df[df["Set"] == "Average"].copy()
	df["k"] = df["Prefetcher"].apply(_parse_k_prefetcher)
	return df.groupby("k", as_index=False)["IPCI"].mean()


def extract_series(df: pd.DataFrame, field: str, values: list[float], filters: dict[str, float]) -> list[float]:
	subset = df
	for column, expected in filters.items():
		subset = subset[subset[column] == expected]
	indexed = subset.set_index(field)["IPCI"]
	missing = [value for value in values if value not in indexed.index]
	if missing:
		raise ValueError(f"Missing values for {field}: {missing}")
	return [float(indexed.loc[value]) for value in values]


def style_axis(ax: plt.Axes) -> None:
	ax.tick_params(axis="both", labelsize=FONT_SIZE_PT, width=0.8, length=2.5)
	for spine in ax.spines.values():
		spine.set_linewidth(0.8)


def plot_line(ax: plt.Axes, x_values: list[float], y_values: list[float], *, color: str, marker: str) -> None:
	ax.plot(
		x_values,
		y_values,
		color=color,
		linewidth=1.5,
		marker=marker,
		markersize=3,
		markerfacecolor=color,
		markeredgecolor="black",
		markeredgewidth=0.2,
        
		zorder=5,
		clip_on=False,
	)


def main() -> None:
	plt.rcParams.update(
		{
			"font.family": "Times New Roman",
            "font.size": 9,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
			"mathtext.fontset": "custom",
			"mathtext.rm": "Times New Roman",
			"mathtext.it": "Times New Roman:italic",
			"mathtext.bf": "Times New Roman:bold",
			"axes.unicode_minus": False,
			"xtick.direction": "out",
			"ytick.direction": "out",
		}
	)


	font_prop = fm.FontProperties(family="Times New Roman", size=FONT_SIZE_PT)

	acc_df = load_acc_average(ACC_CSV_PATH)
	k_df = load_k_average(K_CSV_PATH)

	acc_high_y = extract_series(acc_df, "high", ACC_HIGH_VALUES, {"low": 15.0})
	acc_low_y = extract_series(acc_df, "low", ACC_LOW_VALUES, {"high": 75.0})
	k_y_values = [float(k_df.set_index("k").loc[value, "IPCI"]) for value in K_VALUES]

	y_min = min(min(acc_high_y), min(acc_low_y), min(k_y_values))
	y_max = max(max(acc_high_y), max(acc_low_y), max(k_y_values))
	y_min = 1.06
	y_max = 1.08

	fig = plt.figure(figsize=FIG_SIZE)
	gs = fig.add_gridspec(2, 2, width_ratios=[1.08, 1.08], wspace=0.25, hspace=0.50)

	ax_acc_high = fig.add_subplot(gs[0, 0])
	ax_acc_low = fig.add_subplot(gs[1, 0])
	ax_k = fig.add_subplot(gs[:, 1])
	ax_acc_high.set_ylim(1.07,1.08)
	ax_acc_high.set_yticks([1.07, 1.08])
	ax_acc_high.set_yticklabels(["1.07", "1.08"])
	ax_acc_low.set_ylim(1.07,1.08)
	ax_acc_low.set_yticks([1.07, 1.08])
	ax_acc_low.set_yticklabels(["1.07", "1.08"])
	ax_k.set_ylim(1.06,1.08)
	ax_k.set_yticks([1.06, 1.08])
	ax_k.set_yticklabels(["1.06", "1.08"])

	plot_line(ax_acc_high, ACC_HIGH_VALUES, acc_high_y, color="#2F8AC4", marker="v")
	plot_line(ax_acc_low, ACC_LOW_VALUES, acc_low_y, color="#196553", marker="o")
	plot_line(ax_k, K_VALUES, k_y_values, color="#5D69B1", marker="^")

	for ax in (ax_acc_high, ax_acc_low, ax_k):
		style_axis(ax)
		ax.grid(ls="--", alpha=0.35)
	


	ax_acc_high.set_xticks(ACC_HIGH_VALUES)
	ax_acc_high.xaxis.tick_top()
	ax_acc_high.xaxis.set_label_position("top")
	ax_acc_high.tick_params(axis="x", top=True, labeltop=True, bottom=False, labelbottom=False, pad=1)
	ax_acc_high.set_xlabel(r"$T_{high}$ (%)", fontproperties=font_prop, labelpad=4)
	ax_acc_high.spines["bottom"].set_visible(False)
	

	ax_acc_low.set_xticks(ACC_LOW_VALUES)
	ax_acc_low.tick_params(axis="x", top=False, labeltop=False, bottom=True, labelbottom=True, pad=1)
	ax_acc_low.set_xlabel(r"$T_{low}$ (%)", fontproperties=font_prop, labelpad=0.5)
	ax_acc_low.spines["top"].set_visible(False)
	ax_acc_low.set_ylabel("Speedup", fontproperties=font_prop)
	ax_acc_low.yaxis.set_label_coords(-0.23, 1.2)
	

	ax_k.set_xticks(K_VALUES)
	ax_k.tick_params(axis="x", top=False, labeltop=False, bottom=True, labelbottom=True, pad=1)
	ax_k.set_xlabel(r"$\kappa$", fontproperties=font_prop, labelpad=0.5)
	
	fig.text(0.32, -0.3, "(a) Accuracy Thresholds", ha="center", va="bottom", fontproperties=font_prop)
	fig.text(0.77, -0.3, "(b) Partitioning Parameters", ha="center", va="bottom", fontproperties=font_prop)

	fig.subplots_adjust(left=0.11, right=0.99, top=0.90, bottom=0.20)
	OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
	fig.savefig(OUTPUT_PATH, dpi=300, bbox_inches="tight", pad_inches=0.01)


if __name__ == "__main__":
	main()
