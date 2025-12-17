import os
import re

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




def get_ipc(load, test):
    M5OUT_PATH = f"./log/{load}/{test}.log"
    prefix = "cumulative IPC:"
    find = False 
    with open(M5OUT_PATH, "r") as f:
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

def get_mpki(load, test):
    M5OUT_PATH = f"./log/{load}/{test}.std.log"
    prefix = "MPKI:"
    find = False 
    with open(M5OUT_PATH, "r") as f:
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

def get_l2cpf_issue(load,test):
    M5OUT_PATH = f"./log/{load}/{test}.std.log"
    prefix = "ISSUED:"
    key = "cpu0->cpu0_L2C PREFETCH"
    find = False 
    with open(M5OUT_PATH, "r") as f:
            for line in f:
                if key in line:
                    find = True
                if not find:
                    continue

                pos = line.find(prefix)
                if pos != -1:
                    rest = line[pos + len(prefix):].lstrip()
                    return float(rest.split()[0])   
    return 0.0

def get_l2cpf_useful(load,test):
    M5OUT_PATH = f"./log/{load}/{test}.std.log"
    prefix = "USEFUL:"
    key = "cpu0->cpu0_L2C PREFETCH"
    find = False 
    with open(M5OUT_PATH, "r") as f:
            for line in f:
                if key in line:
                    find = True
                if not find:
                    continue

                pos = line.find(prefix)
                if pos != -1:
                    rest = line[pos + len(prefix):].lstrip()
                    return float(rest.split()[0])   
    return 0.0
def get_l2cpf_accuracy(load,test):
    M5OUT_PATH = f"./log/{load}/{test}.std.log"
    prefix = "USEFUL:"
    prefix2 = "ISSUED:"
    key = "cpu0->cpu0_L2C PREFETCH"
    find = False 
    with open(M5OUT_PATH, "r") as f:
            for line in f:
                if key in line:
                    find = True
                if not find:
                    continue

                pos = line.find(prefix)
                pos2 = line.find(prefix2)
                if pos != -1:
                    rest = line[pos + len(prefix):].lstrip()
                if pos2 != -1:
                    rest2 = line[pos2 + len(prefix2):].lstrip()

                    if(float(rest2.split()[0]) != 0):
                        return float(rest.split()[0]) / float(rest2.split()[0])
                    else:
                        return 0.0
    return 0.0