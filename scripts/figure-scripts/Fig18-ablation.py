"""Plot ablation study grouped bars from ablation.csv."""

from __future__ import annotations

from pathlib import Path
import math

import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import numpy as np
import pandas as pd
import sys
sys.path.append(str(Path(__file__).parent.parent))
from utils.defs import *


BASE_DIR = Path(__file__).resolve().parent
CSV_PATH = RESULT_PATH / "Fig18" / "average.csv"
OUT_PATH = EXPR_PATH / "figure_out" / "Fig18-ablation.pdf"

FIG_SIZE = (8, 1.5)
FONT_SIZE = 9

PREFETCHER_ORDER = [
	"prism-none",
	"prism-ol-pctp",
	"prism-ol-tgp",
	"prism-ol-bmp",
	"prism-ol-irp",
	"prism",
	"prism-wo-pctp",
	"prism-wo-tgp",
	"prism-wo-bmp",
	"prism-wo-irp",
]

PREFETCHER_LABELS = {
	"baseline.4way": "Baseline",
	"prism-ol-pctp": "+PCTP",
	"prism-ol-tgp": "+TGP",
	"prism-ol-bmp": "+BMP",
	"prism-ol-irp": "+IRP",
	"prism-none": r"Baseline",
	"prism": "PRISM",
	"prism-wo-pctp": "−PCTP",
	"prism-wo-tgp": "−TGP",
	"prism-wo-bmp": "−BMP",
	"prism-wo-irp": "−IRP",
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
def blend_with_white(color: str, amount: float) -> tuple[float, float, float]:
	r, g, b = mcolors.to_rgb(color)
	return (
		r + (1.0 - r) * amount,
		g + (1.0 - g) * amount,
		b + (1.0 - b) * amount,
	)


def blend_with_black(color: str, amount: float) -> tuple[float, float, float]:
	r, g, b = mcolors.to_rgb(color)
	return (
		r * (1.0 - amount),
		g * (1.0 - amount),
		b * (1.0 - amount),
	)


PRISM_UNOPT_COLOR = "#BEDAF2"
PRISM_COLOR = "#345470"

PREFETCHER_COLORS = [
	"#AA3377","#D25A70","#DF7B73","#EDA87E","#F7DFA4",
	
	

'#B2DAD8','#77A9B6','#5F8EA3','#49718C',"#355571",

]


def main() -> None:
	plt.rcParams.update(
		{
			"font.family": "serif",
			"font.serif": ["Times New Roman"],
			"font.size": FONT_SIZE,
            'mathtext.fontset': 'dejavuserif'
		}
	)

	df = pd.read_csv(CSV_PATH)
	df.columns = df.columns.str.strip()
	df["Set"] = df["Set"].astype(str).str.strip()
	df["Prefetcher"] = df["Prefetcher"].astype(str).str.strip()
	df["IPCI"] = pd.to_numeric(df["IPCI"], errors="coerce")

	sets = [name for name in SET_ORDER if name in df["Set"].unique()]
	x = np.arange(len(sets))
	n_pf = len(PREFETCHER_ORDER)
	width = 0.86 / max(n_pf, 1)
	offsets = (np.arange(n_pf) - (n_pf - 1) / 2.0) * width

	fig, ax = plt.subplots(1, 1, figsize=FIG_SIZE)

	y_max = 0.0
	for idx, pf in enumerate(PREFETCHER_ORDER):
		values = []
		for set_name in sets:
			row = df[(df["Set"] == set_name) & (df["Prefetcher"] == pf)]
			values.append(row["IPCI"].iloc[0] if not row.empty else np.nan)
		arr = np.array(values, dtype=float)
		valid = arr[np.isfinite(arr)]
		if valid.size > 0:
			y_max = max(y_max, float(np.max(valid)))
		ax.bar(
			x + offsets[idx],
			arr,
			width=width,
			edgecolor="#000000",
			linewidth=0.2,
			color=PREFETCHER_COLORS[idx % len(PREFETCHER_COLORS)],
			label=PREFETCHER_LABELS.get(pf, pf),
		)

	ax.set_xticks(x)
	ax.set_xticklabels([SET_LABELS.get(set_name, set_name) for set_name in sets])
	ax.set_ylabel("Speedup")
	ax.set_ylim(0.95, 1.104)
	ax.set_yticks([0.95, 1.0, 1.05, 1.10])
	ax.set_yticklabels(["0.95", "1.00", "1.05", "1.10"])
	ax.axhline(y=1.0, color="gray", linestyle="--", linewidth=1)
	ax.axvline(x=4.5, color="#CCCCCC", linestyle="--", linewidth=1)
	handles, labels = ax.get_legend_handles_labels()
	# Matplotlib fills legend entries by columns when ncol > 1; reorder so it reads left-to-right by rows.
	ncol = 5
	nrow = math.ceil(len(handles) / ncol)
	order = [r * ncol + c for c in range(ncol) for r in range(nrow) if r * ncol + c < len(handles)]
	#handles = [handles[i] for i in order]
	#labels = [labels[i] for i in order]
	fig.legend(
		handles[0:5],
		labels[0:5],
		loc="upper center",
		ncol=ncol*2,
		frameon=False,
		bbox_to_anchor=(0.33, 0.93),
		columnspacing=0.5,
		handletextpad=0.4,
		handlelength=1,
	)
	fig.legend(
		handles[5:],
		labels[5:],
		loc="upper center",
		ncol=ncol*2,
		frameon=False,
		bbox_to_anchor=(0.73, 0.93),
		columnspacing=0.5,
		handletextpad=0.4,
		handlelength=1,
	)
	plt.margins(x=0.02)

	fig.subplots_adjust(top=0.76, bottom=0.24, left=0.08, right=0.99)
	OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
	fig.savefig(OUT_PATH, bbox_inches="tight",pad_inches=0.01)
	plt.close(fig)
	print(f"Saved figure to {OUT_PATH}")


if __name__ == "__main__":
	main()
