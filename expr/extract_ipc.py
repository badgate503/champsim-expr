import os
import re

LOG_DIR = "log"
OUT_DIR = "result"
OUT_FILE = os.path.join(OUT_DIR, "ipc.txt")

os.makedirs(OUT_DIR, exist_ok=True)

ipc_pattern = re.compile(
    r"CPU 0 cumulative IPC:\s*([0-9]+(?:\.[0-9]+)?)"
)

with open(OUT_FILE, "w") as out:
    for fname in sorted(os.listdir(LOG_DIR)):
        print(fname)
        if not fname.endswith(".log"):
            continue

        trace = fname[:-4]
        log_path = os.path.join(LOG_DIR, fname)

        ipc_value = None

        with open(log_path, "r", errors="ignore") as f:
            for line in f:
                m = ipc_pattern.search(line)
                if m:
                    ipc_value = m.group(1)
                    break

        if ipc_value is not None:
            out.write(f"{trace} : {ipc_value}\n")
        else:
            out.write(f"{trace} : NA\n")