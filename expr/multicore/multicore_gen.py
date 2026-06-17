import json
import random
import argparse
import os
from typing import Dict, Iterable, List, Sequence

# 参数解析
parser = argparse.ArgumentParser()
parser.add_argument("--core", "-c", type=int, default=2, help="每个组合的核数")
parser.add_argument("--num-per-suite", "-n", type=int, default=2, help="每个类别抽取的组合数量")
# 8core 场景下，禁止重复可能导致某些类别无法抽取足够的组合，因此在8 core下允许重复。
parser.add_argument("--no-repeat", action="store_true", help="禁止同一个组合内挑选重复的 trace")
args = parser.parse_args()

# 内部逻辑使用的布尔值，与参数相反
ALLOW_REPEAT = not args.no_repeat

filtered = ["pr-3", "pr-14", "rwkv-1"]
t_set = ["ml", "google", "spec17", "ligra", "gap"]
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
    if not os.path.exists(path):
        print(f"Error: {path} not found.")
        return {}
        
    with open(path, "r") as f:
        for line in f:
            if ":" not in line:
                continue
            label, payload = line.split(":", 1)
            traces = [t for t in payload.strip().split() if t not in banned_set]
            label_clean = label.strip()
            if label_clean in t_set:
                trace_map_local[label_clean] = _deduplicate(traces)
    return trace_map_local

def sample_intra_list_tuples(traces: Sequence[str], rng: random.Random) -> List[List[str]]:
    if not traces:
        return []
    
    # 如果禁止重复且 trace 数量不足，则跳过该类别
    if not ALLOW_REPEAT and len(traces) < tuple_size:
        print(f"Warning: Not enough traces to sample without repeat (need {tuple_size}, have {len(traces)})")
        return []
    
    chosen: List[List[str]] = []
    seen = set()
    max_attempts = tuple_num * 100
    attempts = 0
    
    while len(chosen) < tuple_num and attempts < max_attempts:
        if ALLOW_REPEAT:
            # 默认：允许重复（有放回）
            combo = rng.choices(traces, k=tuple_size)
        else:
            # 禁止重复（无放回）
            combo = rng.sample(traces, k=tuple_size)
            
        key = tuple(sorted(combo))
        if key not in seen:
            seen.add(key)
            chosen.append(list(combo))
        attempts += 1
    return chosen

def sample_cross_list_tuples(trace_map_local: Dict[str, List[str]], rng: random.Random) -> List[List[str]]:
    available = {k: v for k, v in trace_map_local.items() if v}
    list_names = list(available.keys())
    if not list_names:
        return []
        
    chosen: List[List[str]] = []
    seen = set()
    max_attempts = tuple_num * 100
    attempts = 0
    
    while len(chosen) < tuple_num and attempts < max_attempts:
        tuple_candidate = []
        tuple_sources = []
        
        inner_attempts = 0
        while len(tuple_candidate) < tuple_size and inner_attempts < 50:
            lname = rng.choice(list_names)
            trace = rng.choice(available[lname])
            
            # 如果禁止重复，检查 trace 是否已存在于当前组合
            if not ALLOW_REPEAT and trace in tuple_candidate:
                inner_attempts += 1
                continue
                
            tuple_candidate.append(trace)
            tuple_sources.append(lname)
        
        # 验证组合有效性：必须跨类别，且长度达标
        if len(set(tuple_sources)) < 2 or len(tuple_candidate) < tuple_size:
            attempts += 1
            continue
            
        key = tuple(sorted(tuple_candidate))
        if key not in seen:
            seen.add(key)
            chosen.append(tuple_candidate)
        attempts += 1
    return chosen

def main() -> None:
    # 请确保该路径正确
    tracelist_path = "../utils/tracelist"
    trace_map = load_trace_map(tracelist_path, filtered)
    
    if not trace_map:
        return

    rng = random.Random()
    per_list = {name: sample_intra_list_tuples(traces, rng) for name, traces in trace_map.items()}
    cross_list = sample_cross_list_tuples(trace_map, rng)
    
    output_file = f"sample_{args.core}core.csv"
    with open(output_file, "w") as f:
        for k, v in per_list.items():
            for i, u in enumerate(v):
                f.write(f"{k}-{i},{','.join(u)}\n")
        for i, u in enumerate(cross_list):
            f.write(f"mix-{i},{','.join(u)}\n")
            
    mode_str = "允许重复" if ALLOW_REPEAT else "禁止重复"
    print(f"任务完成 | 模式: {mode_str} | 核数: {args.core} | 文件: {output_file}")

if __name__ == "__main__":
    main()