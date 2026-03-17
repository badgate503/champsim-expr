from fetchall import get_measure
from utils.defs import *
import os
allmap = []
for f in os.listdir(LOG_PATH + "/baseline"):
    if f.endswith(".log"):
        print("Reading from: " + LOG_PATH + "/baseline/" + f)
        counters = get_measure(LOG_PATH + "/baseline/" + f)
        l2c_prefetch_issue = int(counters['L2C_PFIssue'])
        allmap.append((f, l2c_prefetch_issue))
        
allmap.sort(key=lambda x: x[1])
for x in allmap:
    print(x[0], x[1])