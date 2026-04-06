import csv
from collections import defaultdict

# 定义 CSV 文件路径
csv_file = "sample_8core.csv"

# 存储 trace 数据的字典
trace_dict = defaultdict(list)

# 读取 CSV 文件
with open(csv_file, "r") as file:
    reader = csv.reader(file)
    header = next(reader)  # 读取表头

    # 遍历每一行数据
    for row in reader:
        workload_name = row[0]  # 假设第一列是 workload 名称
        traces = tuple(sorted(row[1:]))  # 假设后续列是 trace 数据，排序后作为键
        trace_dict[traces].append(workload_name)

# 查找具有相同 trace 数据的 workload
duplicates = {key: value for key, value in trace_dict.items() if len(value) > 1}

# 输出结果
if duplicates:
    print("以下 workload 具有相同的 trace 数据:")
    for traces, workloads in duplicates.items():
        print(f"Workloads: {', '.join(workloads)}")
        print(f"Traces: {traces}")
        print("-")
else:
    print("没有发现具有相同 trace 数据的 workload。")