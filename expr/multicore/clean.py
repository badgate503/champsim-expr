import csv
from pathlib import Path


kill = {
    "gap": [1, 2],
    "google": [1, 2],
    "spec17": [1, 2],
    "ml": [1, 2],
    "ligra": [1, 2],
}


def build_kill_set(kill_map: dict[str, list[int]]) -> set[str]:
    return {f"{suite}-{idx}" for suite, ids in kill_map.items() for idx in ids}


def clean_speedup_csv(csv_path: Path, kill_map: dict[str, list[int]]) -> tuple[int, int]:
    kill_set = build_kill_set(kill_map)

    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames or "trace_set" not in reader.fieldnames:
            raise ValueError("CSV must contain a 'trace_set' column")
        rows = list(reader)
        fieldnames = reader.fieldnames

    kept_rows = [row for row in rows if row["trace_set"] not in kill_set]
    removed = len(rows) - len(kept_rows)

    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(kept_rows)

    return removed, len(kept_rows)


if __name__ == "__main__":
    csv_path = Path(__file__).with_name("speedup.csv")
    removed_count, kept_count = clean_speedup_csv(csv_path, kill)
    print(f"Removed {removed_count} rows, kept {kept_count} rows in {csv_path.name}.")
