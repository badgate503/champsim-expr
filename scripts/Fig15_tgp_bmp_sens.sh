# 记录开始时间（秒级时间戳）
start_time=$(date +%s)
echo "开始时间: $(date '+%Y-%m-%d %H:%M:%S')"

./compiler.py -p prism -f T_ACC_LOW=0.15 T_ACC_HIGH=0.55 -e acc_low15_high55
./compiler.py -p prism -f T_ACC_LOW=0.15 T_ACC_HIGH=0.65 -e acc_low15_high65
# ./compiler.py -p prism -f T_ACC_LOW=0.15 T_ACC_HIGH=0.75 -e acc_low15_high75
./compiler.py -p prism -f T_ACC_LOW=0.15 T_ACC_HIGH=0.85 -e acc_low15_high85
./compiler.py -p prism -f T_ACC_LOW=0.15 T_ACC_HIGH=0.95 -e acc_low15_high95

./compiler.py -p prism -f T_ACC_LOW=0.05 T_ACC_HIGH=0.75 -e acc_low05_high75
./compiler.py -p prism -f T_ACC_LOW=0.10 T_ACC_HIGH=0.75 -e acc_low10_high75
# ./compiler.py -p prism -f T_ACC_LOW=0.15 T_ACC_HIGH=0.75 -e acc_low15_high75
./compiler.py -p prism -f T_ACC_LOW=0.20 T_ACC_HIGH=0.75 -e acc_low20_high75
./compiler.py -p prism -f T_ACC_LOW=0.25 T_ACC_HIGH=0.75 -e acc_low25_high75

./compiler.py -p prism -f K_AGGR=1.00 K_CONS=1.00 -e k_1.00_1.00
./compiler.py -p prism -f K_AGGR=1.25 K_CONS=1.25 -e k_1.25_1.25
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=1.50 -e k_1.50_1.50
./compiler.py -p prism -f K_AGGR=1.75 K_CONS=1.75 -e k_1.75_1.75
./compiler.py -p prism -f K_AGGR=2.00 K_CONS=2.00 -e k_2.00_2.00

# ./compiler.py -p prism -f T_ACC_LOW=0.05 T_ACC_HIGH=0.55 -e acc_low05_high55
# ./compiler.py -p prism -f T_ACC_LOW=0.05 T_ACC_HIGH=0.65 -e acc_low05_high65
# ./compiler.py -p prism -f T_ACC_LOW=0.05 T_ACC_HIGH=0.75 -e acc_low05_high75
# ./compiler.py -p prism -f T_ACC_LOW=0.05 T_ACC_HIGH=0.85 -e acc_low05_high85
# ./compiler.py -p prism -f T_ACC_LOW=0.05 T_ACC_HIGH=0.95 -e acc_low05_high95

# ./compiler.py -p prism -f T_ACC_LOW=0.10 T_ACC_HIGH=0.55 -e acc_low10_high55
# ./compiler.py -p prism -f T_ACC_LOW=0.10 T_ACC_HIGH=0.65 -e acc_low10_high65
# ./compiler.py -p prism -f T_ACC_LOW=0.10 T_ACC_HIGH=0.75 -e acc_low10_high75
# ./compiler.py -p prism -f T_ACC_LOW=0.10 T_ACC_HIGH=0.85 -e acc_low10_high85
# ./compiler.py -p prism -f T_ACC_LOW=0.10 T_ACC_HIGH=0.95 -e acc_low10_high95

# ./compiler.py -p prism -f T_ACC_LOW=0.15 T_ACC_HIGH=0.55 -e acc_low15_high55
# ./compiler.py -p prism -f T_ACC_LOW=0.15 T_ACC_HIGH=0.65 -e acc_low15_high65
# ./compiler.py -p prism -f T_ACC_LOW=0.15 T_ACC_HIGH=0.75 -e acc_low15_high75
# ./compiler.py -p prism -f T_ACC_LOW=0.15 T_ACC_HIGH=0.85 -e acc_low15_high85
# ./compiler.py -p prism -f T_ACC_LOW=0.15 T_ACC_HIGH=0.95 -e acc_low15_high95

# ./compiler.py -p prism -f T_ACC_LOW=0.20 T_ACC_HIGH=0.55 -e acc_low20_high55
# ./compiler.py -p prism -f T_ACC_LOW=0.20 T_ACC_HIGH=0.65 -e acc_low20_high65
# ./compiler.py -p prism -f T_ACC_LOW=0.20 T_ACC_HIGH=0.75 -e acc_low20_high75
# ./compiler.py -p prism -f T_ACC_LOW=0.20 T_ACC_HIGH=0.85 -e acc_low20_high85
# ./compiler.py -p prism -f T_ACC_LOW=0.20 T_ACC_HIGH=0.95 -e acc_low20_high95

# ./compiler.py -p prism -f T_ACC_LOW=0.25 T_ACC_HIGH=0.55 -e acc_low25_high55
# ./compiler.py -p prism -f T_ACC_LOW=0.25 T_ACC_HIGH=0.65 -e acc_low25_high65
# ./compiler.py -p prism -f T_ACC_LOW=0.25 T_ACC_HIGH=0.75 -e acc_low25_high75
# ./compiler.py -p prism -f T_ACC_LOW=0.25 T_ACC_HIGH=0.85 -e acc_low25_high85
# ./compiler.py -p prism -f T_ACC_LOW=0.25 T_ACC_HIGH=0.95 -e acc_low25_high95

# ./compiler.py -p prism -f K_AGGR=1.00 K_CONS=0.50 -e k_1.00_0.50
# ./compiler.py -p prism -f K_AGGR=1.00 K_CONS=0.75 -e k_1.00_0.75
# ./compiler.py -p prism -f K_AGGR=1.00 K_CONS=1.00 -e k_1.00_1.00
# ./compiler.py -p prism -f K_AGGR=1.00 K_CONS=1.25 -e k_1.00_1.25
# ./compiler.py -p prism -f K_AGGR=1.00 K_CONS=1.50 -e k_1.00_1.50

# ./compiler.py -p prism -f K_AGGR=1.25 K_CONS=0.50 -e k_1.25_0.50
# ./compiler.py -p prism -f K_AGGR=1.25 K_CONS=0.75 -e k_1.25_0.75
# ./compiler.py -p prism -f K_AGGR=1.25 K_CONS=1.00 -e k_1.25_1.00
# ./compiler.py -p prism -f K_AGGR=1.25 K_CONS=1.25 -e k_1.25_1.25
# ./compiler.py -p prism -f K_AGGR=1.25 K_CONS=1.50 -e k_1.25_1.50

# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=0.50 -e k_1.50_0.50
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=0.75 -e k_1.50_0.75
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=1.00 -e k_1.50_1.00
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=1.25 -e k_1.50_1.25
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=1.50 -e k_1.50_1.50

# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=0.50 -e k_1.50_0.50
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=0.75 -e k_1.50_0.75
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=1.00 -e k_1.50_1.00
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=1.25 -e k_1.50_1.25
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=1.50 -e k_1.50_1.50

# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=0.50 -e k_1.50_0.50
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=0.75 -e k_1.50_0.75
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=1.00 -e k_1.50_1.00
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=1.25 -e k_1.50_1.25
# ./compiler.py -p prism -f K_AGGR=1.50 K_CONS=1.50 -e k_1.50_1.50

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
./runner.py -p acc_low15_high55 acc_low15_high65 acc_low15_high85 acc_low15_high95 acc_low05_high75 acc_low10_high75 acc_low20_high75 acc_low25_high75 k_1.00_1.00 k_1.25_1.25 k_1.75_1.75 k_2.00_2.00 -l ligra gap spec17 ml google -s

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

python3 get_result.py -p acc_low15_high55 acc_low15_high65 acc_low15_high85 acc_low15_high95 acc_low05_high75 acc_low10_high75 acc_low20_high75 acc_low25_high75 -o ../experiments/results/tgp -m IPCI
python3 get_result.py -p k_1.00_1.00 k_1.25_1.25 k_1.75_1.75 k_2.00_2.00 -o ./results/fig15_tgp_bmp_sens -o ../experiments/results/bmp -m IPCI