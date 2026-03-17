import os
import re
from .defs import *
RED = '\033[91m'
GREEN = '\033[92m'
YELLOW = '\033[93m'
BLUE = '\033[94m'
MAGENTA = '\033[95m'
CYAN = '\033[96m'
WHITE = '\033[97m'
BOLD = '\033[1m'
UNDERLINE = '\033[4m'
END = '\033[0m'




def get_ipc(path):
    prefix = "cumulative IPC:"
    find = False 
    with open(path, "r") as f:
        for line in f:
            if "=== Simulation ===" in line:
                 find = True
            if not find:
                 continue


            pos = line.find(prefix)
            if pos != -1:
                rest = line[pos + len(prefix):].lstrip()
                return float(rest.split()[0])   
    return 0.0
