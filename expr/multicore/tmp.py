from pathlib import Path


source_dir = Path("/mnt/data/lyq/exprlog/core8/triangel.8c")
mapping_file = Path("/mnt/data/lyq/PRISM/expr/multicore/old2new")


keep_names = set()
for line in mapping_file.read_text().splitlines():
    if not line:
        continue
    old_name, _ = line.split(",")
    keep_names.add(old_name)

for path in source_dir.glob("*.log"):
    if path.stem not in keep_names:
        print(f"Deleting {path}")
        path.unlink() 
pairs = []
for line in mapping_file.read_text().splitlines():
    if not line:
        continue
    old_name, new_name = line.split(",")
    if old_name == new_name:
        continue

    old_path = source_dir / f"{old_name}.log"
    if old_path.exists():
        pairs.append((old_path, source_dir / f"{new_name}.log"))
        print(f"Renaming {old_path} to {source_dir / f'{new_name}.log'}")

for index, (old_path, _) in enumerate(pairs):
	old_path.rename(source_dir / f".__tmp__{index}.log")

for index, (_, new_path) in enumerate(pairs):
	(source_dir / f".__tmp__{index}.log").rename(new_path)
