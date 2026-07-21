"""Plot 8-core multicore experiment speedup from speedup.csv."""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

import sys
sys.path.append(str(Path(__file__).parent.parent))
from utils.defs import *

CSV_PATH = RESULT_PATH / "Fig17" / "speedup.csv"
OUT_PATH = FIGURE_PATH / "Fig17-multicore.pdf"

FIG_SIZE = (8, 1.0)
FONT_SIZE = 9
PREFETCHER_COLORS = {
	"triangel": "#CCDA80",
	"prophet": "#59A3A4",
	"prism": "#345470",
}
PREFETCHER_LABELS = {
	"triangel": "Triangel",
	"prophet": "Prophet",
	"prism": "PRISM",
}
TRACE_SET_GROUPS = ["ligra", "gap", "spec17", "ml", "google", "mix"]
TRACE_SET_LABELS = {
	"ligra": "Ligra",
	"gap": "GAP",
	"spec17": "SPEC",
	"ml": "ML",
	"google": "Google",
	"mix": "MIX",
}
Y_AXIS_CONFIG = {
	"ligra": {"ylim": (0.95, 1.15), "yticks": [0.95, 1.05, 1.15], "yticklabels": ["0.95", "1.05", "1.15"]},
	"gap": {"ylim": (0.95, 1.15), "yticks": [0.95, 1.05, 1.15], "yticklabels": ["0.95", "1.05", "1.15"]},
	"spec17": {"ylim": (0.75, 1.25), "yticks": [0.75, 1, 1.25], "yticklabels": ["0.75", "1.00", "1.25"]},
	"ml": {"ylim": (0.9, 1.3), "yticks": [0.9, 1.1, 1.3], "yticklabels": ["0.90", "1.10", "1.30"]},
	"google": {"ylim": (0.95, 1.15), "yticks": [0.95, 1.05, 1.15], "yticklabels": ["0.95", "1.05", "1.15"]},
	"mix": {"ylim": (0.8, 1.2), "yticks": [0.8, 1.0, 1.2], "yticklabels": ["0.80", "1.00", "1.20"]},
}


def main() -> None:
	plt.rcParams.update({
		"font.family": "serif",
		"font.serif": ["Times New Roman"],
		"font.size": FONT_SIZE,
	})

	df = pd.read_csv(CSV_PATH)
	df.columns = df.columns.str.strip()
	df["prefetcher"] = df["prefetcher"].astype(str).str.lower().str.strip()
	df["trace_set"] = df["trace_set"].astype(str).str.strip()
	df["speedup"] = pd.to_numeric(df["speedup"], errors="coerce")

	# Filter for 8-core only
	df_8core = df[df["core_number"] == 8].copy()
	
	if df_8core.empty:
		print("No 8-core data found")
		return

	# Create 6 subplots in one row; y-axis is independent for per-panel settings
	fig, axes = plt.subplots(1, 6, figsize=FIG_SIZE, sharex=False, sharey=False)

	for idx, group_prefix in enumerate(TRACE_SET_GROUPS):
		ax = axes[idx]
		group_df = df_8core[df_8core["trace_set"].str.startswith(group_prefix)].copy()
		y_cfg = Y_AXIS_CONFIG[group_prefix]
		manual_ylim = y_cfg["ylim"]
		
		if group_df.empty:
			ax.set_xlabel(TRACE_SET_LABELS.get(group_prefix, group_prefix), fontsize=FONT_SIZE, labelpad=2)
			
			if all(v is not None for v in manual_ylim):
				ax.set_ylim(*manual_ylim)
			if y_cfg["yticks"]:
				ax.set_yticks(y_cfg["yticks"])
			if y_cfg["yticklabels"]:
				ax.set_yticklabels(y_cfg["yticklabels"])
			ax.spines["top"].set_visible(False)
			ax.spines["right"].set_visible(False)
			continue

		# Sort traces by PRISM speedup
		df_prism = group_df[group_df["prefetcher"] == "prism"][["trace_set", "speedup"]].copy()
		df_prism = df_prism.rename(columns={"speedup": "prism_speedup"})
		sorted_traces = (
			df_prism
			.sort_values("prism_speedup")["trace_set"]
			.unique()
			.tolist()
		)

		x_data = np.arange(len(sorted_traces), dtype=float)
		data_for_limits: list[float] = []

		# Plot each prefetcher
		for pf in ["triangel", "prophet", "prism"]:
			y_vals = []
			for trace in sorted_traces:
				row = group_df[(group_df["trace_set"] == trace) & (group_df["prefetcher"] == pf)]
				y_vals.append(float(row["speedup"].iloc[0]) if not row.empty else np.nan)
			y_arr = np.array(y_vals, dtype=float)
			data_for_limits.extend(y_arr[np.isfinite(y_arr)].tolist())
			marker_map = {"triangel": "v", "prophet": "^", "prism": "o"}
			ax.plot(
				x_data,
				y_arr,
				marker=marker_map[pf],
				linewidth=1,
				markersize=2,
				markeredgewidth=0.2,
				markeredgecolor="black",
				color=PREFETCHER_COLORS.get(pf, "#000000"),
				label=PREFETCHER_LABELS.get(pf, pf),
			)

		# Apply automatic ylim only when this subplot has no manual ylim configured.
		if data_for_limits:
			valid_data = np.array([v for v in data_for_limits if np.isfinite(v)], dtype=float)
			if valid_data.size and not all(v is not None for v in manual_ylim):
				y_min = float(valid_data.min())
				y_max = float(valid_data.max())
				span = y_max - y_min
				padding = max(span * 0.1, 0.01)
				ax.set_ylim(max(0.0, y_min - padding), y_max + padding)

		ax.set_xticks([])
		ax.set_xlabel(TRACE_SET_LABELS.get(group_prefix, group_prefix), fontsize=FONT_SIZE, labelpad=2)
		if all(v is not None for v in manual_ylim):
			ax.set_ylim(*manual_ylim)
		if y_cfg["yticks"]:
			ax.set_yticks(y_cfg["yticks"])
		if y_cfg["yticklabels"]:
			ax.set_yticklabels(y_cfg["yticklabels"])
		if idx == 0:
			ax.set_ylabel("Speedup", fontsize=FONT_SIZE)
			
		ax.grid(True, alpha=0.3)
		ax.axhline(1.0, color="gray", linestyle="--", linewidth=0.5)
	# Add shared legend
	handles, labels = [], []
	for pf in ["triangel", "prophet", "prism"]:
		marker_map = {"triangel": "v", "prophet": "^", "prism": "o"}
		handles.append(plt.Line2D([0], [0], color=PREFETCHER_COLORS[pf], linewidth=1.5, marker=marker_map[pf], markersize=3, markeredgewidth=0.2, markeredgecolor="black"))
		labels.append(PREFETCHER_LABELS[pf])

	fig.legend(
		handles,
		labels,
		loc="lower center",
		ncol=3,
		frameon=False,
		bbox_to_anchor=(0.5, 0.86),
	)

	fig.subplots_adjust(top=0.92, bottom=0.22, left=0.1, right=0.98, hspace=0.4, wspace=0.4)
	OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
	fig.savefig(OUT_PATH, bbox_inches="tight", dpi=150, pad_inches=0.01)
	plt.close(fig)
	print(f"Saved figure to {OUT_PATH}")


if __name__ == "__main__":
	main()
