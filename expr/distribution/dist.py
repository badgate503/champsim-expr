from pathlib import Path
import csv
import re


BASE_DIR = Path(__file__).resolve().parent
TRACE_LIST_PATH = (BASE_DIR / "../utils/tracelist").resolve()
LOG_DIR = (BASE_DIR / "../../../exprlog/prism").resolve()
OUTPUT_CSV = BASE_DIR / "pat_lookup_distribution.csv"


def load_trace_map(tracelist_path: Path):
	trace_map = {}
	with tracelist_path.open("r") as f:
		for line in f:
			if ":" not in line:
				continue
			set_name, payload = line.split(":", 1)
			set_name = set_name.strip()
			if set_name == "others":
				continue
			traces = [trace for trace in payload.strip().split() if trace]
			trace_map[set_name] = traces
	return trace_map


def parse_lookup_times(log_path: Path):
	lookup_counts = {}
	pattern = re.compile(r"pat_lookup_times_(\d+)\s+(\d+)")

	with log_path.open("r") as f:
		for line in f:
			match = pattern.search(line)
			if match:
				lookup_index = int(match.group(1))
				lookup_counts[lookup_index] = lookup_counts.get(lookup_index, 0) + int(match.group(2))

	return lookup_counts


def main():
	trace_map = load_trace_map(TRACE_LIST_PATH)

	rows = []
	max_lookup_index = 0

	for set_name, traces in trace_map.items():
		for trace_name in traces:
			log_path = LOG_DIR / f"{trace_name}.log"
			if not log_path.exists():
				print(f"skip missing log: {log_path}")
				continue

			lookup_counts = parse_lookup_times(log_path)
			if lookup_counts:
				max_lookup_index = max(max_lookup_index, max(lookup_counts))

			rows.append((set_name, trace_name, lookup_counts))

	fieldnames = ["Set", "Trace"] + [f"Lookup{i}" for i in range(1, max_lookup_index + 1)]

	with OUTPUT_CSV.open("w", newline="") as f:
		writer = csv.DictWriter(f, fieldnames=fieldnames)
		writer.writeheader()

		for set_name, trace_name, lookup_counts in rows:
			row = {"Set": set_name, "Trace": trace_name}
			for index in range(1, max_lookup_index + 1):
				row[f"Lookup{index}"] = lookup_counts.get(index, 0)
			writer.writerow(row)

	print(f"written: {OUTPUT_CSV}")


if __name__ == "__main__":
	main()
