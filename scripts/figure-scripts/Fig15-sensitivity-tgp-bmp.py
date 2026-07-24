from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from typing import Optional
import sys
sys.path.append(str(Path(__file__).resolve().parent.parent))
from utils.defs import *

TARGET_PREFETCHERS = [
	"acc_low15_high55",
	"acc_low15_high65",
	"acc_low15_high75",
	"acc_low15_high85",
	"acc_low15_high95",
]

K_PREFETCHERS = [
	"k_1.00",
	"k_1.25",
	"k_1.50",
	"k_1.75",
	"k_2.00",
]


def load_data(data_dir: Path, target_prefetchers: list[str]) -> pd.DataFrame:
	csv_files = [
		data_dir / "gap.csv",
		data_dir / "google.csv",
		data_dir / "ligra.csv",
		data_dir / "ml.csv",
		data_dir / "spec17.csv",
	]
	frames = [pd.read_csv(csv_file) for csv_file in csv_files]
	data = pd.concat(frames, ignore_index=True)
	data = data[data["Prefetcher"].isin(target_prefetchers)].copy()
	return data


def load_trace_aliases(mapping_path: Path) -> dict[str, str]:
	mapping = pd.read_csv(mapping_path)
	return dict(zip(mapping["trace_name"], mapping["alias"]))


def build_diff_table(data: pd.DataFrame) -> pd.DataFrame:
	pivot = data.pivot_table(
		index="Trace", columns="Prefetcher", values="IPCI", aggfunc="first"
	)
	pivot = pivot.dropna(subset=TARGET_PREFETCHERS)

	ipci_span = pivot[TARGET_PREFETCHERS].max(axis=1) - pivot[TARGET_PREFETCHERS].min(axis=1)
	selected = pivot[ipci_span > 0.05].copy()
	if selected.empty:
		return selected

	selected["d55_75"] = selected["acc_low15_high55"] - selected["acc_low15_high75"]
	selected["d65_75"] = selected["acc_low15_high65"] - selected["acc_low15_high75"]
	selected["d85_75"] = selected["acc_low15_high85"] - selected["acc_low15_high75"]
	selected["d95_75"] = selected["acc_low15_high95"] - selected["acc_low15_high75"]

	return selected.sort_index()


def build_k_diff_table(data: pd.DataFrame) -> pd.DataFrame:
	pivot = data.pivot_table(
		index="Trace", columns="Prefetcher", values="IPCI", aggfunc="first"
	)
	pivot = pivot.dropna(subset=K_PREFETCHERS)

	ipci_span = pivot[K_PREFETCHERS].max(axis=1) - pivot[K_PREFETCHERS].min(axis=1)
	selected = pivot[ipci_span > 0.05].copy()
	if selected.empty:
		return selected

	selected["d100_150"] = selected["k_1.00"] - selected["k_1.50"]
	selected["d125_150"] = selected["k_1.25"] - selected["k_1.50"]
	selected["d175_150"] = selected["k_1.75"] - selected["k_1.50"]
	selected["d200_150"] = selected["k_2.00"] - selected["k_1.50"]

	return selected.sort_index()


def plot_grouped_bars(
	ax: plt.Axes,
	diff_table: pd.DataFrame,
	series: list[tuple[str, str]],
	title: str,
	trace_aliases: dict[str, str],
	rotation: int = 25,
	titlepad: int = 13,
	colors: list[str] = ["#AA3377", "#D25A70", "#DF7B73", "#EDA87E"],
) -> None:
	plt.rcParams["font.family"] = "Times New Roman"
	plt.rcParams["font.size"] = 9

	traces = diff_table.index.tolist()
	x = np.arange(len(traces))
	bar_width = 0.2
	hatches = ["/////", "|||||", "-----", "\\\\\\\\\\"]

	for idx, (col, label) in enumerate(series):
		ax.bar(
			x + (idx - 1.5) * bar_width,
			diff_table[col].values,
			width=bar_width,
			label=label,
			color=colors[idx],
			edgecolor='#000000',
                        linewidth=0.2,
						hatch=hatches[idx % len(hatches)],
						
		)

	ax.axhline(0.0, color="black", linewidth=0.8)
	ax.set_ylabel(r"$\Delta$Speedup")
	#ax.set_title(title, pad=titlepad)
	ax.set_xticks(x)
	ax.set_xticklabels(
		[trace_aliases.get(trace, trace) for trace in traces],
		rotation=rotation,
		ha="center",
		fontsize=7,
	)
	ax.legend(
		frameon=False,
		ncol=len(series),
		fontsize=7,
		loc="upper center",
		bbox_to_anchor=(0.5, 1.08),
		columnspacing=0.8,
		handletextpad=0.3,
		borderaxespad=-1.0,
	)
	ax.margins(x=0.01)


def set_top_y_axis(ax: plt.Axes) -> None:
	ax.set_ylim(-0.1, 0.1)
	ax.set_yticks([0.10, 0.05, 0.0, -0.05, -0.10])
	ax.set_yticklabels(["0.10", "0.05", "0", "-0.05", "-0.10"])


def set_bottom_y_axis(ax: plt.Axes) -> None:
	ax.set_ylim(-0.1, 0)
	ax.set_yticks([0.0,-0.05, -0.1])
	ax.set_yticklabels(["0", "-0.05", "-0.10"])


def add_panel_caption(ax: plt.Axes, caption: str) -> None:
	ax.text(
		0.5,
		-0.42,
		caption,
		transform=ax.transAxes,
		ha="center",
		va="top",
		fontsize=8,
	)


def annotate_bottom_values(ax: plt.Axes, diff_table: pd.DataFrame) -> None:
	traces = diff_table.index.tolist()
	if len(traces) < 3:
		return

	bar_width = 0.2
	annotations = [
		(0, 0, diff_table.iloc[0]["d100_150"]),
		(2, 3, diff_table.iloc[2]["d200_150"]),
	]
	for trace_idx, series_idx, value in annotations:
		x_pos = trace_idx + (series_idx - 1.5) * bar_width
		ax.text(
			x_pos,
			-0.04,
			f"{value:.2f}",
			transform=ax.get_xaxis_transform(),
			ha="center",
			va="top",
			fontsize=6,
		)



def plot_diff(
	acc_table: pd.DataFrame,
	k_table: pd.DataFrame,
	output_path: Path,
	trace_aliases: dict[str, str],
) -> None:
	plt.rcParams["font.family"] = "Times New Roman"
	plt.rcParams["font.size"] = 9
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
				'hatch.linewidth': 0.3
            }
        )

	acc_traces = acc_table.index.tolist()
	k_traces = k_table.index.tolist()
	fig_height = 2.2
	fig, axes = plt.subplots(2, 1, figsize=(3.5, fig_height))

	plot_grouped_bars(
		axes[0],
		acc_table,
		[
			("d55_75", r"$55\%$"),
			("d65_75", r"$65\%$"),
			("d85_75", r"$85\%$"),
			("d95_75", r"$95\%$"),
		],
		r"$T_{high}$ (%)",
		trace_aliases,
		rotation=0,
		titlepad=13,
		colors=["#AA3377", "#D25A70", "#DF7B73", "#EDA87E"]
	)
	set_top_y_axis(axes[0])
	add_panel_caption(axes[0], r"(a) Accuracy Threshold $T_{high}$")
	set_bottom_y_axis(axes[1])
	# annotate_outliers(axes[1], k_table, ["d100_150", "d125_150", "d175_150", "d200_150"])
	plot_grouped_bars(
		axes[1],
		k_table,
		[
			("d100_150", r"$1.00$"),
			("d125_150", r"$1.25$"),
			("d175_150", r"$1.75$"),
			("d200_150", r"$2.00$"),
		],
		r"$\kappa$",
		trace_aliases,
		rotation=0,
		titlepad=13,
		colors=["#355571",'#5f8ea3','#77A9B6','#B2DAD8']
	)
	annotate_bottom_values(axes[1], k_table)
	add_panel_caption(axes[1], r"(b) Partitioning Parameter $\kappa$")

	fig.subplots_adjust(left=0.18, right=0.99, top=0.98, bottom=0.2, hspace=0.9)
	fig.savefig(output_path,  dpi=1000, bbox_inches="tight", pad_inches=0.01)
	plt.close(fig)


def main() -> None:
	
	acc_dir = RESULT_PATH/ "Fig15" / "sens_acc"
	k_dir = RESULT_PATH/ "Fig15" / "sens_k"
	trace_aliases = load_trace_aliases(SCRIPTS_PATH / "utils" / "trace_alias.csv")

	acc_data = load_data(acc_dir, TARGET_PREFETCHERS)
	acc_diff_table = build_diff_table(acc_data)
	k_data = load_data(k_dir, K_PREFETCHERS)
	k_diff_table = build_k_diff_table(k_data)

	if acc_diff_table.empty:
		print("No sens_acc trace satisfies: max(IPCI) - min(IPCI) > 0.03")
		return
	if k_diff_table.empty:
		print("No sens_k trace satisfies: max(IPCI) - min(IPCI) > 0.03")
		return

	out_png = FIGURE_PATH / "Fig15-sensitivity-tgp-bmp.pdf"
	plot_diff(acc_diff_table, k_diff_table, out_png, trace_aliases)

	print("Selected sens_acc traces (max-min > 0.03):")
	for trace in acc_diff_table.index:
		print(trace)
	print(f"Total selected sens_acc traces: {len(acc_diff_table)}")
	print("Selected sens_k traces (max-min > 0.03):")
	for trace in k_diff_table.index:
		print(trace)
	print(f"Total selected sens_k traces: {len(k_diff_table)}")
	print(f"Figure saved to: {out_png}")


if __name__ == "__main__":
	main()
