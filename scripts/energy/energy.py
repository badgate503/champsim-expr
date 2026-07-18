

from pathlib import Path
import csv
import copy
import sys

sys.path.append(str(Path(__file__).parent.parent))
from utils.defs import *

metrics = {
    "prophet": [
        "training_unit",
        "multipath_victim_buffer",
        "0_way_markov_table",
        "1_way_markov_table",
        "2_way_markov_table",
        "3_way_markov_table",
        "4_way_markov_table",
        "5_way_markov_table",
        "6_way_markov_table",
        "7_way_markov_table",
        "8_way_markov_table",
    ],
    "triangel": [
        "history_sampler",
        "second_chance_sampler",
        "training_unit",
        "reuse_buffer",
        "markov_table",
    ],
    "prism": [
        "training_unit",
        "filter_table",
        "1_way_markov_table",
        "8_way_markov_table",
        "1_way_pat_table",
        "2_way_pat_table",
    ],
    "baseline":[
        "training_unit",
        "4_way_markov_table",
    ]
}

TARGET_FIELDS = [
    "Dynamic read energy (nJ)",
    "Dynamic write energy (nJ)",
    "Standby leakage per bank(mW)",
]


def read_energy_fields(csv_path: Path):
    with csv_path.open("r", newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        row = next(reader, None)

    if not header or not row:
        return None

    normalized = {k.strip(): v.strip() for k, v in zip(header, row)}
    return {field: normalized.get(field, "N/A") for field in TARGET_FIELDS}


def read_energy_fields_with_arg(csv_path: Path, n):
    if n == 0:
        return {field: 0 for field in TARGET_FIELDS}
    with csv_path.open("r", newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        
        for i in range(0,n):
            row = next(reader, None)

    if not header or not row:
        return None

    normalized = {k.strip(): v.strip() for k, v in zip(header, row)}
    return {field: normalized.get(field, "N/A") for field in TARGET_FIELDS}

file_dir = Path(__file__).resolve().parent

all_list=dict()

for group_name, modules in metrics.items():
    print(f"\n[{group_name}]")
    met_list = dict()
    for module_name in modules:

        if group_name == "triangel" and module_name == "markov_table":
            module_name = "8_way_markov_table"
        
        if module_name == "training_unit":
            module_name = group_name+"_training_unit"

        if module_name[0].isdigit():
            module_name_parsed = "_".join(module_name.split("_")[2:])
            print(module_name)
            out_file = file_dir / "cacti_output" /f"{module_name_parsed}.out"
            if not out_file.exists():
                print(f"  {module_name}: file not found ({out_file.name})")
                continue

            values = read_energy_fields_with_arg(out_file, int(module_name.split("_")[0]))
            if values is None:
                print(f"  {module_name_parsed}: invalid or empty file")
                continue
            print(f"  {module_name_parsed}:")
            for field in TARGET_FIELDS:
                print(f"    {field}: {values[field]}")

        else:
            out_file = file_dir / "cacti_output" / f"{module_name}.out"
            if not out_file.exists():
                print(f"  {module_name}: file not found ({out_file.name})")
                continue

            values = read_energy_fields(out_file)
            if values is None:
                print(f"  {module_name}: invalid or empty file")
                continue

            print(f"  {module_name}:")
            for field in TARGET_FIELDS:
                print(f"    {field}: {values[field]}")

        if group_name == "triangel" and module_name == "8_way_markov_table":
            module_name = "markov_table"
        if module_name.endswith("markov_table") or module_name.endswith("pat_table"):
            values["Standby leakage per bank(mW)"] = "0"
        met_list[module_name] = values
    all_list[group_name] = met_list
print()


import re


def load_trace_map(tracelist_path: Path):
    trace_map = {}
    with tracelist_path.open("r") as f:
        for line in f:
            if ":" not in line:
                continue
            group_name, payload = line.split(":", 1)
            if group_name.strip() == "others":
                continue
            traces = [trace for trace in payload.strip().split() if trace]
            trace_map[group_name.strip()] = traces
    return trace_map


def read_bank_count(csv_path: Path):
    with csv_path.open("r", newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        row = next(reader, None)

    if not header or not row:
        return 1

    normalized = {k.strip(): v.strip() for k, v in zip(header, row)}
    try:
        return int(float(normalized.get("Number of banks", "1")))
    except ValueError:
        return 1


def parse_runtime_log(log_path: Path):
    module_access = {}
    cycles = None
    frequency = None
    markov_way = None
    pct_way = None
    with log_path.open("r") as f:
        for line in f:
            match_cycles = re.search(r"Simulation finished CPU 0 instructions:\s*\d+\s+cycles:\s*(\d+)", line)
            if match_cycles:
                cycles = int(match_cycles.group(1))

            match_access = re.match(r"([A-Za-z0-9_.-]+)_(read|write)\s+(\d+)", line.strip())
            match_markov = re.match(r"Init_WayForMarkov\s+(\d+)", line.strip())
            match_pct    = re.match(r"Init_WayForPCT\s+(\d+)", line.strip())
            if match_markov:
                markov_way = int(match_markov.group(1))
            
            if match_pct:
                pct_way = int(match_pct.group(1))
            
            if match_access:
                module_name, access_type, value = match_access.groups()
                module_access.setdefault(module_name, {})[access_type] = int(value)
    #print(module_access)
    return module_access, cycles, 4000, markov_way, pct_way


def calc_module_energy(module_values, module_access, cycles, frequency_mhz, bank_count=1):
    read_count = module_access.get("read", 0)
    write_count = module_access.get("write", 0)
    


    read_energy = float(module_values.get("Dynamic read energy (nJ)", 0))
    write_energy = float(module_values.get("Dynamic write energy (nJ)", 0))
    leakage = float(module_values.get("Standby leakage per bank(mW)", 0))
    if read_count == 0 and write_count == 0:
        return {
            "read_count": 0,
            "write_count": 0,
            "dynamic_energy_nj": 0.0,
            "leakage_energy_nj": 0.0,
            "total_energy_nj": 0.0,
        },"   N/A, No accesses, energy is zero."
    dynamic_energy = read_count * read_energy + write_count * write_energy
    leakage_energy = 0.0
    if cycles is not None and frequency_mhz:
        leakage_energy = leakage * bank_count * cycles / (frequency_mhz)
    
    report =f"   Reads: {read_count}\n   Writes: {write_count}\n   Read Energy: {read_energy:.6f} nJ\n   Write Energy: {write_energy:.6f} nJ\n   Leakage: {leakage:.6f} mW\n   Cycles: {cycles}\n   Frequency: {frequency_mhz} MHz\n   Dynamic Energy: {dynamic_energy:.6f} nJ\n   Leakage Energy: {leakage_energy:.6f} nJ"


    return {
        "read_count": read_count,
        "write_count": write_count,
        "dynamic_energy_nj": dynamic_energy,
        "leakage_energy_nj": leakage_energy,
        "total_energy_nj": dynamic_energy + leakage_energy,
    },report


trace_map = load_trace_map((SCRIPTS_PATH / "utils/tracelist").resolve())

energy_result = {}

print("[energy calculation]")
for scheme_name, module_values_map in all_list.items():
    scheme_dir = LOG_PATH / scheme_name
    if not scheme_dir.exists():
        print(f"[{scheme_name}] result directory not found: {scheme_dir}")
        continue

    scheme_result = {}
    print(f"[{scheme_name}]")

    for trace_group, trace_names in trace_map.items():
        group_result = []
        
        for trace_name in trace_names:
            log_path = scheme_dir / f"{trace_name}.log"
            if not log_path.exists():
                continue

            module_access_map, cycles, frequency_mhz, markov_way, pct_way = parse_runtime_log(log_path)
            trace_total = 0.0
            trace_modules = {}

            for module_name, module_values in module_values_map.items():
                module_values = copy.deepcopy(module_values)
                if module_name.endswith("markov_table") and markov_way is not None:
                    pass
                    # if int(module_name[0]) != markov_way:
                    #     module_values["Standby leakage per bank(mW)"] = 0
                    #     print(f"Init Markov={markov_way}, skipping leakage for {module_name}")

                if module_name.endswith("pat_table") and pct_way is not None:
                    pass
                    # if int(module_name[0]) != pct_way:
                    #     module_values["Standby leakage per bank(mW)"] = 0
                    #     print(f"Init PCT={pct_way}, skipping leakage for {module_name}")

                if module_name[0].isdigit():
                    module_access_name = "_".join(module_name.split("_")[2:])
                else:
                    module_access_name = module_name

                bank_count = 1

                if module_name[0].isdigit():
                    true_name = module_name
                elif module_name.endswith("_training_unit"):
                    true_name = "training_unit"
                    
                else:
                    true_name = module_name

                #print(f"234: {module_access_map}")
                #print(f"235: {true_name} {module_access_map.get(true_name, {})}")
                module_energy, report = calc_module_energy(
                    module_values,
                    module_access_map.get(true_name, {}),
                    cycles,
                    frequency_mhz,
                    bank_count,
                )
            
                trace_modules[module_name] = module_energy
                trace_total += module_energy["total_energy_nj"]
                print(f"{scheme_name}/{trace_group}/{trace_name}/{module_name}\n{report}")

            print(f"  {trace_group}/{trace_name}: {trace_total:.6f} nJ")
            group_result.append(trace_total)
            

            scheme_result[trace_name] = {
                "group": trace_group,
                "cycles": cycles,
                "frequency_mhz": frequency_mhz,
                "module_energy": trace_modules,
                "total_energy_nj": trace_total,
            }

        if group_result:
            average_energy = sum(group_result) / len(group_result)
            print(f"  {trace_group} average: {average_energy:.6f} nJ over {len(group_result)} traces")

    energy_result[scheme_name] = scheme_result


summary_rows = []
for scheme_name, scheme_result in energy_result.items():
    for trace_name, trace_data in scheme_result.items():
        summary_rows.append({
            "group_name": scheme_name,
            "set": trace_data["group"],
            "trace": trace_name,
            "trace_energy": trace_data["total_energy_nj"],
        })

os.makedirs(RESULT_PATH / "energy", exist_ok=True)

csv_path = RESULT_PATH / "energy" / "energy_result.csv"
with csv_path.open("w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=["group_name", "set", "trace", "trace_energy"])
    writer.writeheader()
    writer.writerows(summary_rows)

print(f"[energy calculation] csv written to {csv_path}")


normalized_rows = []
display_names = {
    "prophet": "Prophet",
    "triangel": "Triangel",
    "prism": "PRISM",
}
target_schemes = ["prophet", "triangel", "prism"]

baseline_result = energy_result.get("baseline", {})

for scheme_name in target_schemes:
    scheme_result = energy_result.get(scheme_name, {})
    set_values = {}

    for trace_name, trace_data in scheme_result.items():
        baseline_trace = baseline_result.get(trace_name)
        if not baseline_trace:
            continue

        baseline_energy = baseline_trace.get("total_energy_nj", 0)
        if baseline_energy == 0:
            continue

        normalized_energy = trace_data["total_energy_nj"] / baseline_energy
        set_name = trace_data["group"]
        set_values.setdefault(set_name, []).append(normalized_energy)

    set_average_values = []
    for set_name in sorted(set_values):
        set_average = sum(set_values[set_name]) / len(set_values[set_name])
        set_average_values.append(set_average)
        normalized_rows.append({
            "group_name": display_names[scheme_name],
            "set": set_name,
            "normalized_energy": set_average,
        })

    if set_average_values:
        normalized_rows.append({
            "group_name": display_names[scheme_name],
            "set": "Average",
            "normalized_energy": sum(set_average_values) / len(set_average_values),
        })


normalized_csv_path = RESULT_PATH / "energy" / "normalized_energy.csv"
with normalized_csv_path.open("w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=["group_name", "set", "normalized_energy"])
    writer.writeheader()
    writer.writerows(normalized_rows)

print(f"[energy calculation] normalized csv written to {normalized_csv_path}")



