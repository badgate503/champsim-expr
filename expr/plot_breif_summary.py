#!/usr/bin/env python3.9
"""
Plot a single stacked-bar summary `result/result.png` from `result/*_breif.txt`.

Behavior:
- Reads `expr/result/*_breif.txt` and `expr/tracelist`.
- Groups workloads by sets defined in `tracelist` (e.g., ligra, spec17, ...).
- Workloads belonging to the same set are plotted adjacent; sets are separated by visible divider lines.
- The set name is printed centered under the group's bars.
- A final `Average` column shows per-category averages across workloads (DIVIDER slots ignored for averages).
- Categories are sorted by average (descending) so large-average categories are at the bottom of stacks.
- Uses matplotlib only (no SVG fallback). If matplotlib is not installed, the script exits with instructions.

Usage:
  python3 plot_breif_summary.py

Output:
  expr/result/result.png

"""
from pathlib import Path
from collections import OrderedDict
import sys
import math

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
except Exception:
    print("matplotlib and numpy are required. Install with: pip install matplotlib numpy")
    sys.exit(1)


import argparse

HERE = Path(__file__).resolve().parent
DIVIDER = "__DIVIDER__"

def get_args():
    parser = argparse.ArgumentParser(description="Plot stacked-bar summary from *_breif.txt files.")
    parser.add_argument('--result', type=str, default=str(HERE / "result"), help='Path to result directory (default: ./result)')
    parser.add_argument('--tracelist', type=str, default=str(HERE / "tracelist"), help='Path to tracelist file (default: ./tracelist)')
    parser.add_argument('-t', '--trace', type=str, nargs='+', help='Only include specified trace(s), by name (no suffix)')
    return parser.parse_args()


def parse_tracelist(path):
    groups = OrderedDict()
    if not path.exists():
        return groups
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or ":" not in line:
            continue
        setname, rest = line.split(":", 1)
        tokens = [t.strip() for t in rest.split() if t.strip()]
        groups[setname.strip()] = tokens
    return groups


def parse_breif_file(path):
    d = OrderedDict()
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        key = parts[0]
        try:
            val = int(parts[1])
        except ValueError:
            continue
        d[key] = val
    return d


def discover_breif_files(result_dir):
    files = list(result_dir.glob("*_breif.txt"))
    datas = OrderedDict()
    for f in sorted(files):
        name = f.name
        if name.endswith("_breif.txt"):
            base = name[: -len("_breif.txt")]
        else:
            base = name
        datas[base] = parse_breif_file(f)
    return datas


def map_tracelist_to_keys(datasets_keys, tracelist_groups):
    remaining = set(datasets_keys)
    ordered = []
    for setname, tokens in tracelist_groups.items():
        matched = []
        for token in tokens:
            # exact
            if token in remaining:
                matched.append(token)
                remaining.remove(token)
                continue
            # keys that startwith token
            starts = sorted([k for k in remaining if k.startswith(token)])
            if starts:
                matched.extend(starts)
                for k in starts:
                    remaining.remove(k)
                continue
            # keys that contain token
            contains = sorted([k for k in remaining if token in k])
            if contains:
                matched.extend(contains)
                for k in contains:
                    remaining.remove(k)
                continue
            # keys that token contains
            contained = sorted([k for k in remaining if k in token])
            if contained:
                matched.extend(contained)
                for k in contained:
                    remaining.remove(k)
                continue
        if matched:
            # add divider before this set if ordered already contains items
            if ordered:
                ordered.append(DIVIDER)
            ordered.extend(matched)
        # if no matched members, skip the set
    # leftover
    if remaining:
        if ordered:
            ordered.append(DIVIDER)
        ordered.extend(sorted(remaining))
    return ordered


def build_matrix(datasets, categories, file_order):
    mat = OrderedDict()
    for fn in file_order:
        if fn == DIVIDER:
            mat[fn] = {c: 0.0 for c in categories}
            continue
        d = datasets.get(fn, {})
        total = sum(d.values())
        if total == 0:
            mat[fn] = {c: 0.0 for c in categories}
        else:
            mat[fn] = {c: (d.get(c, 0) / total) for c in categories}
    return mat


def compute_avgs(mat, categories):
    keys = [k for k in mat.keys() if k != DIVIDER]
    avgs = {}
    for c in categories:
        s = 0.0
        for k in keys:
            s += mat[k].get(c, 0.0)
        avgs[c] = s / max(1, len(keys))
    return avgs


def modern_palette(n):
    # Muted categorical palette based on ColorBrewer's 'Dark2' for
    # good distinguishability without overly bright/neon colors.
    import colorsys
    # prefer 'Dark2' then fall back to 'tab10'/'tab20'
    for try_name in ('Dark2', 'tab10', 'tab20', 'Set2'):
        try:
            cmap = plt.get_cmap(try_name)
            break
        except Exception:
            cmap = None
    if cmap is None:
        cmap = plt.get_cmap('tab10')

    if n <= 1:
        return [cmap(0.5)]

    colors = []
    # sample evenly; when cmap is small (e.g., tab10) we wrap as needed
    base = cmap.N if hasattr(cmap, 'N') else 20
    for i in range(n):
        if n <= base:
            pos = i / max(1, n - 1)
        else:
            pos = (i % base) / float(base)
        r, g, b, a = cmap(pos)
        h, s, v = colorsys.rgb_to_hsv(r, g, b)
        # slightly desaturate and clamp brightness to avoid too-bright colors
        s = max(0.12, s * 0.9)
        v = max(0.40, min(0.78, v * 0.92))
        r2, g2, b2 = colorsys.hsv_to_rgb(h, s, v)
        colors.append((r2, g2, b2, a))
    return colors


def plot(datasets, totals, file_order, categories_sorted, avgs, mat, outpath):
    # positions
    file_slots = list(file_order)
    file_slots.append('Average')
    n = len(file_slots)
    x = np.arange(n)

    bottoms = np.zeros(n)
    colors = modern_palette(len(categories_sorted))
    fig_w = max(12, n * 1)
    fig_h = 10
    fig, ax = plt.subplots(figsize=(fig_w, fig_h))

    for idx, cat in enumerate(categories_sorted):
        vals = []
        for fn in file_order:
            if fn == DIVIDER:
                vals.append(0.0)
            else:
                vals.append(mat[fn].get(cat, 0.0))
        vals.append(avgs.get(cat, 0.0))
        vals = np.array(vals)
        ax.bar(x, vals, bottom=bottoms, color=colors[idx], edgecolor='white', width=0.8)
        # annotate: choose white/black text for contrast against the bar color
        rcol, gcol, bcol, acol = colors[idx]
        for i, v in enumerate(vals):
            if v > 0.03:
                lum = 0.299 * rcol + 0.587 * gcol + 0.114 * bcol
                txt_col = 'black' if lum > 0.65 else 'white'
                ax.text(i, bottoms[i] + v / 2.0, f"{v*100:.1f}%", ha='center', va='center', color=txt_col, fontsize=8, fontweight='bold')
        bottoms += vals

    # draw dividers: DIVIDER occupies its own slot; draw a vertical line at that x
    for i, fn in enumerate(file_order):
        if fn == DIVIDER:
            ax.axvline(i, color='#222222', linewidth=3, linestyle='--', zorder=20)

    # set x labels (hide labels for DIVIDER)
    labels = [fn if fn != DIVIDER else '' for fn in file_order]
    labels.append('Average')
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=45, ha='right')

    # place set labels centered under groups
    # compute groups from file_order by scanning non-divider contiguous blocks
    i = 0
    while i < len(file_order):
        if file_order[i] == DIVIDER:
            i += 1
            continue
        # block start
        start = i
        # determine which set name this block belongs to by checking tracelist_groups
        # we will label by looking up in tracelist_groups where the first member was listed
        while i < len(file_order) and file_order[i] != DIVIDER:
            i += 1
        end = i - 1
        center = (start + end) / 2.0
        # find set name by scanning tracelist groups: pick first group containing file_order[start]
        setname = None
        for sname, toks in tracelist_groups.items():
            # tokens in tracelist are not exact; check if the workload name appears in toks or vice versa
            if any((file_order[start] == t) or (t in file_order[start]) or (file_order[start] in t) for t in toks):
                setname = sname
                break
        if setname is None:
            setname = ''
        # move set name further down so it doesn't overlap bars/labels
        ax.text(center, -0.18, setname, transform=ax.get_xaxis_transform(), ha='center', va='top', fontsize=10, fontweight='bold')

    ax.set_ylim(0, 1.0)
    ax.set_ylabel('Proportion')
    ax.set_title('Stacked-category proportions (grouped by set)')

    # legend using averages
    legend_labels = [f"{c}: {avgs[c]*100:.2f}%" for c in categories_sorted]
    handles = [plt.Rectangle((0, 0), 1, 1, facecolor=colors[i], edgecolor='k', linewidth=0.5) for i in range(len(categories_sorted))]
    ax.legend(handles, legend_labels, bbox_to_anchor=(1.02, 1), loc='upper left', title='Category (avg)')

    plt.tight_layout()
    plt.savefig(outpath, dpi=150)
    plt.close(fig)
    print(f"Wrote {outpath}")


if __name__ == '__main__':
    args = get_args()
    RESULT_DIR = Path(args.result)
    TRACELIST = Path(args.tracelist)

    if not RESULT_DIR.exists():
        print(f"Result directory not found: {RESULT_DIR}")
        sys.exit(1)

    datasets = discover_breif_files(RESULT_DIR)
    if not datasets:
        print(f"No *_breif.txt files found in {RESULT_DIR}/")
        sys.exit(1)

    tracelist_groups = parse_tracelist(TRACELIST)

    # 只保留指定 trace（trace 名精确匹配，无论属于哪个 set）
    if args.trace:
        trace_set = set(args.trace)
        datasets = {k: v for k, v in datasets.items() if k in trace_set}
        # tracelist_groups 也只保留包含这些 trace 的 set，且 set 内只保留这些 trace
        new_tracelist_groups = OrderedDict()
        for setname, tokens in tracelist_groups.items():
            filtered = [t for t in tokens if t in trace_set]
            if filtered:
                new_tracelist_groups[setname] = filtered
        tracelist_groups = new_tracelist_groups
        if not datasets:
            print(f"No specified traces found: {args.trace}")
            sys.exit(1)

    # 固定类别顺序
    fixed_order = [
        'TARGET_FIRST_APPEAR',
        'PF_TOO_LATE',
        'NO_TRIGGER',
        'NO_MD_LAST_ADDR_0',
        'NO_MD_LAST_ADDR_REPEAT',
        'PF_TOO_EARLY',
        'EVICTED_MD_CAPACITY',
        'EVICTED_MD_CONFLICT',
    ]

    # compute union of categories
    categories = []
    seen = set()
    for d in datasets.values():
        for k in d.keys():
            if k is None:
                continue
            if k.strip().upper() == 'OTHER':
                continue
            if k not in seen:
                categories.append(k)
                seen.add(k)

    # 只保留 fixed_order 中有的类别，且顺序固定，剩余类别追加在后面
    categories_sorted = [c for c in fixed_order if c in categories]
    categories_sorted += [c for c in categories if c not in fixed_order]

    if not categories_sorted:
        print("No categories found after excluding 'OTHER'. Nothing to plot.")
        sys.exit(0)

    # build ordered file list grouped by tracelist
    file_order = map_tracelist_to_keys(list(datasets.keys()), tracelist_groups)
    # ensure divider before Average
    if file_order and file_order[-1] != DIVIDER:
        file_order.append(DIVIDER)

    mat = build_matrix(datasets, categories_sorted, file_order)
    avgs = compute_avgs(mat, categories_sorted)

    outpath = RESULT_DIR / 'result.png'
    plot(datasets, {k: sum(v.values()) for k, v in datasets.items()}, file_order, categories_sorted, avgs, mat, outpath)
