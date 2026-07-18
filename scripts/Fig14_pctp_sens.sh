# 记录开始时间（秒级时间戳）
start_time=$(date +%s)
echo "开始时间: $(date '+%Y-%m-%d %H:%M:%S')"

./compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=1 -e pcq-1
./compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=2 -e pcq-2
./compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=4 -e pcq-4
./compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=8 -e pcq-8
./compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=12 -e pcq-12
./compiler.py -p prism -f INF_PAT INDEPENDENT_PAT PCQ_SIZE=16 -e pcq-16

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

# python3 get_result.py -p pcq-1 pcq-2 pcq-4 pcq-8 pcq-12 pcq-16 -o ../experiments/results/pctp -m IPCI
# python3 get_result.py -p pat-12k pat-24k pat-48k pat-96k pat-144k pat-192k -o ../experiments/results/pctp -m IPCI