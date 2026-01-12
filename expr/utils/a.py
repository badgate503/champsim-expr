import json
import os
import glob
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--prefetcher", "-p", required=True, help="prefetcher name")
args = parser.parse_args()
result = {}

for path in glob.glob(f"../result/data/{args.prefetcher}/*.txt"):

    name = os.path.splitext(os.path.basename(path))[0].replace("_breif", "")  # xxx.txt -> xxx
    print(name)
    if name == "ipc":
        continue
    data = {}

    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            key, value = line.split()
            data[key] = int(value)

    result[name] = data

with open(f"../result/data/{args.prefetcher}/result.json", "w") as f:
    json.dump(result, f, indent=2)

print("Generated result.json")