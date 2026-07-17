# 记录开始时间（秒级时间戳）
start_time=$(date +%s)
echo "开始时间: $(date '+%Y-%m-%d %H:%M:%S')"

./compiler.py -p prism -f T_ACC_LOW=0 T_ACC_HIGH=0.55 -e acc_low00_high55
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0 T_ACC_HIGH=0.55 -e acc_low00_high55
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0 T_ACC_HIGH=0.65 -e acc_low00_high65
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0 T_ACC_HIGH=0.75 -e acc_low00_high75
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0 T_ACC_HIGH=0.85 -e acc_low00_high85
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0 T_ACC_HIGH=0.95 -e acc_low00_high95

 ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.05 T_ACC_HIGH=0.55 -e acc_low05_high55
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.05 T_ACC_HIGH=0.65 -e acc_low05_high65
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.05 T_ACC_HIGH=0.75 -e acc_low05_high75
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.05 T_ACC_HIGH=0.85 -e acc_low05_high85
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.05 T_ACC_HIGH=0.95 -e acc_low05_high95
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.05 T_ACC_HIGH=1 -e acc_low05_high100

# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.10 T_ACC_HIGH=0.55 -e acc_low10_high55
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.10 T_ACC_HIGH=0.65 -e acc_low10_high65
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.10 T_ACC_HIGH=0.75 -e acc_low10_high75
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.10 T_ACC_HIGH=0.85 -e acc_low10_high85
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.10 T_ACC_HIGH=0.95 -e acc_low10_high95
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.10 T_ACC_HIGH=1 -e acc_low10_high100

# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.15 T_ACC_HIGH=0.55 -e acc_low15_high55
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.15 T_ACC_HIGH=0.65 -e acc_low15_high65
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.15 T_ACC_HIGH=0.75 -e acc_low15_high75
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.15 T_ACC_HIGH=0.85 -e acc_low15_high85
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.15 T_ACC_HIGH=0.95 -e acc_low15_high95
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.15 T_ACC_HIGH=1 -e acc_low15_high100

# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.20 T_ACC_HIGH=0.55 -e acc_low20_high55
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.20 T_ACC_HIGH=0.65 -e acc_low20_high65
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.20 T_ACC_HIGH=0.75 -e acc_low20_high75
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.20 T_ACC_HIGH=0.85 -e acc_low20_high85
# ./compiler.py -p prism -m ipc -f N_LLC_SET=4096 T_ACC_LOW=0.20 T_ACC_HIGH=0.95 -e acc_low20_high95

./compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=12*1024 -e pat-12k
./compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=24*1024 -e pat-24k
./compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=48*1024 -e pat-48k
./compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=96*1024 -e pat-96k
./compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=144*1024 -e pat-144k
./compiler.py -p prism -f INDEPENDENT_PAT PAT_SIZE=192*1024 -e pat-192k

# 记录结束时间
end_time=$(date +%s)
echo "结束时间: $(date '+%Y-%m-%d %H:%M:%S')"

# 计算耗时（秒）
duration=$((end_time - start_time))

# 转换为 时:分:秒 格式显示
hours=$((duration / 3600))
minutes=$(( (duration % 3600) / 60 ))
seconds=$((duration % 60))

echo "编译耗时: ${hours}小时 ${minutes}分钟 ${seconds}秒"

# 记录开始时间（秒级时间戳）
start_time=$(date +%s)
echo "开始时间: $(date '+%Y-%m-%d %H:%M:%S')"

# 执行你的命令
./runner.py -p pcq-1 pcq-2 pcq-4 pcq-8 pcq-12 pcq-16 pat-12k pat-24k pat-48k pat-96k pat-144k pat-192k -l google -s

# 记录结束时间
end_time=$(date +%s)
echo "结束时间: $(date '+%Y-%m-%d %H:%M:%S')"

# 计算耗时（秒）
duration=$((end_time - start_time))

# 转换为 时:分:秒 格式显示
hours=$((duration / 3600))
minutes=$(( (duration % 3600) / 60 ))
seconds=$((duration % 60))

echo "运行耗时: ${hours}小时 ${minutes}分钟 ${seconds}秒"