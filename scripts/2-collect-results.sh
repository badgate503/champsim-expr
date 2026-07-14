# 1. Single-core speedup and 2. Metrics: Accuracy, Timeliness and DRAM Traffic

python3 get_result.py -p triangel prophet prism -o ./results/basic -m IPCI L2C_Accuracy L2C_Timeliness DRAM_Traffic

# 3. Metadata Studies

python3 get_result.py -p baseline triangel prophet prism -a Baseline Triangel Prophet PRISM -o ./results/mdtraffic -m MT_lookups MT_inserts MT_hits L2C_USEFUL 
