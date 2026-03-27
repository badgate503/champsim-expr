import itertools
import json
import random
from typing import Dict, Iterable, List, Sequence
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--core", "-c", type=int, default=2)
parser.add_argument("--num-per-suite", "-n", type=int, default=2)
args = parser.parse_args()



filtered = ["pr-3", "pr-14", "rwkv-1"]
t_set = ["ml","google","spec17","ligra","gap"]
tuple_num = args.num_per_suite
tuple_size = args.core


def _deduplicate(items: Iterable[str]) -> List[str]:
    seen = set()
    deduped = []
    for item in items:
        if item and item not in seen:
            seen.add(item)
            deduped.append(item)
    return deduped


def load_trace_map(path: str, banned: Sequence[str]) -> Dict[str, List[str]]:
    banned_set = set(banned)
    trace_map_local: Dict[str, List[str]] = {}
    with open(path, "r") as f:
        for line in f:
            if ":" not in line:
                continue
            label, payload = line.split(":", 1)
            traces = [t for t in payload.strip().split() if t not in banned_set]
            if label.strip() in t_set:
                trace_map_local[label.strip()] = _deduplicate(traces)
    return trace_map_local


def sample_intra_list_tuples(traces: Sequence[str], rng: random.Random) -> List[List[str]]:
    if len(traces) < tuple_size:
        return []
    combos = list(itertools.combinations(traces, tuple_size))
    rng.shuffle(combos)
    limit = min(tuple_num, len(combos))
    return [list(combo) for combo in combos[:limit]]


def sample_cross_list_tuples(trace_map_local: Dict[str, List[str]], rng: random.Random) -> List[List[str]]:
    available = {k: v for k, v in trace_map_local.items() if v}
    list_names = list(available.keys())
    total_traces = sum(len(v) for v in available.values())
    if total_traces < tuple_size:
        return []
    chosen: List[List[str]] = []
    seen = set()
    max_attempts = max(tuple_num * 50, 100)
    attempts = 0
    while len(chosen) < tuple_num and attempts < max_attempts:
        tuple_candidate: List[str] = []
        tuple_lists: List[str] = []
        used_traces = set()
        inner_attempts = 0
        while len(tuple_candidate) < tuple_size and inner_attempts < tuple_size * 5:
            list_name = rng.choice(list_names)
            value = rng.choice(available[list_name])
            inner_attempts += 1
            if value in used_traces:
                continue
            used_traces.add(value)
            tuple_candidate.append(value)
            tuple_lists.append(list_name)
        if len(tuple_candidate) < tuple_size:
            attempts += 1
            continue
        if len(set(tuple_lists)) == 1:
            attempts += 1
            continue
        key = tuple(sorted(tuple_candidate))
        if key in seen:
            attempts += 1
            continue
        seen.add(key)
        chosen.append(tuple_candidate)
        attempts += 1
    return chosen


trace_map = load_trace_map("../utils/tracelist", filtered)
trace_all = [trace for traces in trace_map.values() for trace in traces]
import os


trace_path_map = dict()
for root, dirs, files in os.walk("/mnt/data/lyq/champtraces"):
    for f in files:
        if f.endswith(".xz") or f.endswith(".gz"):
            trace_name = f.replace('.champsimtrace.xz',"").replace('.champsimtrace.gz',"").replace('.champsim.gz',"")
            trace_path_map[trace_name] = os.path.join(root, f)

def main() -> None:
    rng = random.Random()
    per_list = {name: sample_intra_list_tuples(traces, rng) for name, traces in trace_map.items()}
    cross_list = sample_cross_list_tuples(trace_map, rng)
    output = {
        "per_list": per_list,
        "cross_list": cross_list,
    }
    with open(f"sample_{args.core}core.csv", "w") as f:
        for k,v in per_list.items():
            i = 0
            for u in v:
                f.write(f"{k}-{i},{','.join(u)}\n")
                i += 1
        i = 0
        for u in cross_list:
            
            f.write(f"mix-{i},{','.join(u)}\n")
            i += 1

if __name__ == "__main__":
    main()

