"""Plot metadata traffic breakdowns from all.csv."""

from __future__ import annotations

from pathlib import Path
import sys
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import numpy as np
import pandas as pd

sys.path.append(str(Path(__file__).parent.parent))
from utils.defs import *

BASE_DIR = Path(__file__).resolve().parent
OUT_PATH = BASE_DIR.parent / FIGURE_PATH / "Fig13-metadata.pdf"
FIG_SIZE = (3.5, 2.0)
FONT_FAMILY = "Times New Roman"
FONT_SIZE = 8
PREFETCHERS = ["Triangel", "Prophet", "PRISM"]
COLORS = {
	"Triangel": "#CCDA80",
	"Prophet": "#59A3A4",
	"PRISM": "#345470",
}
SET_ORDER = ["ligra", "gap", "spec17", "ml", "google", "Average"]
SET_LABELS = {
	"ligra": "Ligra",
	"gap": "GAP",
	"spec17": "SPEC",
	"ml": "ML",
	"google": "Google",
	"Average": "Avg.",
}


def load_data() -> pd.DataFrame:
	df = pd.read_csv(BASE_DIR.parent / RESULT_PATH / "metadata"  / "average.csv")
	df.columns = df.columns.str.strip()
	df["Set"] = df["Set"].astype(str).str.strip()
	df["Prefetcher"] = df["Prefetcher"].astype(str).str.strip()
	numeric_cols = ["MT_lookups", "MT_inserts", "MT_hits", "L2C_USEFUL"]
	for col in numeric_cols:
		df[col] = pd.to_numeric(df[col], errors="coerce")
	return df


def ordered_sets(df: pd.DataFrame) -> list[str]:
	existing = list(df["Set"].dropna().unique())
	ordered = [item for item in SET_ORDER if item in existing]
	ordered += [item for item in existing if item not in ordered]
	return ordered


def set_positions(n_sets: int) -> np.ndarray:
	return np.arange(n_sets)


def stack_ylim(values: list[float]) -> tuple[float, float]:
	finite = np.array([value for value in values if np.isfinite(value)], dtype=float)
	if finite.size == 0:
		return 0.0, 1.0
	vmax = float(np.max(finite))
	return 0.0, vmax * 1.12 if vmax > 0 else 1.0


def log_ylim(values: list[float]) -> tuple[float, float]:
	finite = np.array([value for value in values if np.isfinite(value) and value > 0], dtype=float)
	if finite.size == 0:
		return 0.1, 1.0
	vmin = float(np.min(finite))
	vmax = float(np.max(finite))
	lower = max(vmin * 0.85, 1e-3)
	upper = vmax * 1.15 if vmax > 0 else 1.0
	return lower, upper


def darker_color(color: str, factor: float = 0.3) -> str:
	r, g, b = mcolors.to_rgb(color)
	return (r * factor, g * factor, b * factor)


def outline_top_bottom(ax: plt.Axes, patch, color: str) -> None:
	x0 = patch.get_x()
	y0 = patch.get_y()
	width = patch.get_width()
	height = patch.get_height()
	if height <= 0:
		return
	ax.hlines(y0, x0, x0 + width, colors=color, linewidth=0.5)
	ax.hlines(y0 + height, x0, x0 + width, colors=color, linewidth=0.5)
    

def panel_label(ax: plt.Axes, text: str) -> None:
	ax.text(0.5, -0.28, text, transform=ax.transAxes, ha="center", va="top", fontsize=FONT_SIZE)


def plot_left(ax: plt.Axes, df: pd.DataFrame, sets: list[str]) -> None:
	x = set_positions(len(sets))
	bar_width = 0.8 / len(PREFETCHERS)
	offsets = (np.arange(len(PREFETCHERS)) - (len(PREFETCHERS) - 1) / 2.0) * bar_width
	all_values: list[float] = []

	for idx, pf in enumerate(PREFETCHERS):
		vals = []
		for set_name in sets:
			row = df[(df["Set"] == set_name) & (df["Prefetcher"] == pf)]
			value = row["MT_hits"].iloc[0] if not row.empty else np.nan
			vals.append(value)
		bars = ax.bar(
			x + offsets[idx],
			vals,
			width=bar_width,
			color=COLORS[pf],
			linewidth=0.2,
			edgecolor="black",
			label=pf,
		)
		# for patch in bars:
		# 	outline_top_bottom(ax, patch, darker_color(COLORS[pf]))
		all_values.extend([value for value in vals if np.isfinite(value)])

	ax.set_xticks(x)
	ax.set_xticklabels([SET_LABELS.get(set_name, set_name) for set_name in sets])
	ax.set_ylabel("MT_hits")
	ax.set_ylim(stack_ylim(all_values))
	ax.spines["top"].set_visible(False)
	ax.spines["right"].set_visible(False)
	panel_label(ax, "(a) MT_hits")


def plot_middle(ax: plt.Axes, df: pd.DataFrame, sets: list[str]) -> None:
	x = set_positions(len(sets))
	bar_width = 0.8 / len(PREFETCHERS)
	offsets = (np.arange(len(PREFETCHERS)) - (len(PREFETCHERS) - 1) / 2.0) * bar_width
	all_values: list[float] = []

	for idx, pf in enumerate(PREFETCHERS):
		lookup_vals = []
		insert_vals = []
		for set_name in sets:
			row = df[(df["Set"] == set_name) & (df["Prefetcher"] == pf)]
			if row.empty:
				lookup_vals.append(np.nan)
				insert_vals.append(np.nan)
				continue
			lookup = float(row["MT_lookups"].iloc[0])
			insert = float(row["MT_inserts"].iloc[0])
			lookup_vals.append(lookup)
			insert_vals.append(insert)

		lookup_bars = ax.bar(
			x + offsets[idx],
			lookup_vals,
			width=bar_width,
			color=COLORS[pf],
			linewidth=0.2,
			edgecolor="black",
			label=pf,
		)
		# for patch in lookup_bars:
		# 	outline_top_bottom(ax, patch, "#555555")
		insert_bars = ax.bar(
			x + offsets[idx],
			insert_vals,
			width=bar_width,
			bottom=lookup_vals,
			color=COLORS[pf],
			linewidth=0.2,
			edgecolor="black",
			hatch="oooooo",
			#edgecolor='#CCCCCC',
		)
		# for patch in insert_bars:
		# 	outline_top_bottom(ax, patch, "#555555")
		stacked = np.array(lookup_vals, dtype=float) + np.array(insert_vals, dtype=float)
		all_values.extend(stacked[np.isfinite(stacked)].tolist())

	ax.set_xticks([])
	ax.set_xticklabels([])
	ax.set_ylim(0,2)
	ax.set_yticks([0,0.5,1.0,1.5,2.0])
	ax.set_yticklabels(["0","0.5","1.0","1.5","2.0"])
	ax.text(0.5, -0.06, "(a) Metadata Insert & Lookup", transform=ax.transAxes, ha="center", va="top", fontsize=FONT_SIZE)


def plot_right(ax: plt.Axes, df: pd.DataFrame, sets: list[str]) -> None:
	x = set_positions(len(sets))
	bar_width = 0.8 / len(PREFETCHERS)
	offsets = (np.arange(len(PREFETCHERS)) - (len(PREFETCHERS) - 1) / 2.0) * bar_width
	all_values: list[float] = []

	for idx, pf in enumerate(PREFETCHERS):
		useful_vals = []
		hit_gap_vals = []
		miss_gap_vals = []
		for set_name in sets:
			row = df[(df["Set"] == set_name) & (df["Prefetcher"] == pf)]
			if row.empty:
				useful_vals.append(np.nan)
				hit_gap_vals.append(np.nan)
				miss_gap_vals.append(np.nan)
				continue
			mt_lookups = float(row["MT_lookups"].iloc[0])
			mt_hits = float(row["MT_hits"].iloc[0])
			l2c_useful = float(row["L2C_USEFUL"].iloc[0])
			useful_vals.append(l2c_useful)
			hit_gap_vals.append(max(mt_hits - l2c_useful, 0.0))
			miss_gap_vals.append(max(mt_lookups - mt_hits, 0.0))

		bottom = np.array(useful_vals, dtype=float)
		middle = np.array(hit_gap_vals, dtype=float)
		top = np.array(miss_gap_vals, dtype=float)
		bottom_bars = ax.bar(
			x + offsets[idx],
			bottom,
			width=bar_width,
			color=COLORS[pf],
			linewidth=0.2,
			label=pf,
			hatch="xxxx",
			edgecolor="black",
			
		)
		# for patch in bottom_bars:
		# 	outline_top_bottom(ax, patch, darker_color(COLORS[pf]))
		middle_bars = ax.bar(
			x + offsets[idx],
			middle,
			width=bar_width,
			bottom=bottom,
			color=COLORS[pf],
			linewidth=0.2,
			hatch="////",
			edgecolor="black",
		)
		# for patch in middle_bars:
		# 	outline_top_bottom(ax, patch, darker_color(COLORS[pf]))
		top_bars = ax.bar(
			x + offsets[idx],
			top,
			width=bar_width,
			bottom=bottom + middle,
			color=COLORS[pf],
			linewidth=0.2,
			edgecolor="black",
			
		)
		# for patch in top_bars:
		# 	outline_top_bottom(ax, patch, darker_color(COLORS[pf]))
		stacked = bottom + middle + top
		all_values.extend(stacked[np.isfinite(stacked)].tolist())

	ax.set_xticks(x)
	ax.set_xticklabels([SET_LABELS.get(set_name, set_name) for set_name in sets])
	ax.set_yscale("log")
	ax.set_ylim(0,1.7)
	ax.text(0.5, -0.33, "(b) Metadata Lookup & Hit & Useful Prefetch", transform=ax.transAxes, ha="center", va="top", fontsize=FONT_SIZE)



def main() -> None:
	plt.rcParams.update({
		"font.family": "serif",
		"font.serif": [FONT_FAMILY],
		"font.size": FONT_SIZE,
		"hatch.linewidth": 0.5,
		"hatch.color": "#555555",
	})

	df = load_data()
	sets = ordered_sets(df)

	fig, axes = plt.subplots(2, 1, figsize=FIG_SIZE)

	plot_middle(axes[0], df, sets)
	plot_right(axes[1], df, sets)

	prefetcher_handles = [
		plt.Rectangle((0, 0), 1, 1, facecolor=COLORS[pf], linewidth=0.2, edgecolor="black", label=pf)
		for pf in PREFETCHERS
	]
	hatch_handles = [
		plt.Rectangle((0, 0), 1, 1, facecolor="white", edgecolor="black",linewidth=0.2, hatch="", label="Lookup"),
		plt.Rectangle((0, 0), 1, 1, facecolor="white", edgecolor="black",linewidth=0.2, hatch="oooo", label="Insert"),
		plt.Rectangle((0, 0), 1, 1, facecolor="white", edgecolor="black", linewidth=0.2, hatch="\\\\\\\\", label="Useful"),
		plt.Rectangle((0, 0), 1, 1, facecolor="white", edgecolor="black",linewidth=0.2, hatch="////", label="Hit"),
	]
	fig.legend(
		prefetcher_handles,
		PREFETCHERS,
		loc="upper center",
		ncol=3,
		frameon=False,

		bbox_to_anchor=(0.57, 1.01),
	)
	fig.legend(
		hatch_handles,
		["Lookup", "Insert", "Useful", "Hit"],
		loc="upper center",
		ncol=4,
		frameon=False,
		bbox_to_anchor=(0.57, 0.95),
		
				handletextpad=0.35,
				handlelength=1.28,
	)

	fig.subplots_adjust(top=0.84, bottom=0.12, left=0.18, right=0.98, hspace=0.25)
	OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
	
	fig.savefig(OUT_PATH, bbox_inches="tight", dpi=1000, pad_inches=0.01)
	plt.close(fig)
	print(f"Saved figure to {OUT_PATH}")


if __name__ == "__main__":
	main()
