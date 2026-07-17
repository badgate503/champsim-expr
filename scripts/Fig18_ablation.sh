# 记录开始时间（秒级时间戳）
start_time=$(date +%s)
echo "开始时间: $(date '+%Y-%m-%d %H:%M:%S')"

# ./compiler.py -p baseline
./compiler.py -p prism -f ABLATION_STUDY PC_TRIGGER_PREFETCHING -e prism-ol-pctp
./compiler.py -p prism -f ABLATION_STUDY TG_PREFETCHING -e prism-ol-tgp
./compiler.py -p prism -f ABLATION_STUDY BMP_RESIZE -e prism-ol-bmp
./compiler.py -p prism -f ABLATION_STUDY INSERTION_POLICY REPLACEMENT_POLICY -e prism-ol-irp

# ./compiler.py -p prism
./compiler.py -p prism -f ABLATION_STUDY TG_PREFETCHING BMP_RESIZE INSERTION_POLICY REPLACEMENT_POLICY -e prism-wo-pctp
./compiler.py -p prism -f ABLATION_STUDY PC_TRIGGER_PREFETCHING BMP_RESIZE INSERTION_POLICY REPLACEMENT_POLICY -e prism-wo-tgp
./compiler.py -p prism -f ABLATION_STUDY PC_TRIGGER_PREFETCHING TG_PREFETCHING INSERTION_POLICY REPLACEMENT_POLICY -e prism-wo-bmp
./compiler.py -p prism -f ABLATION_STUDY PC_TRIGGER_PREFETCHING TG_PREFETCHING BMP_RESIZE -e prism-wo-irp

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
./runner.py -p prism-ol-pctp prism-ol-tgp prism-ol-bmp prism-ol-irp prism-wo-pctp prism-wo-tgp prism-wo-bmp prism-wo-irp -l ligra gap spec17 ml google -s

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