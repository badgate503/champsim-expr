## expr.py

- `-p` 指定当前框架使用的预取器
- `-t` 指定单个 trace 名
- `-l` 指定 tracelist 中定义的整个 trace 列表
- `-c` 编译当前框架
- `-i` 指定执行的 Interval

在 ./log/{prefetcher} 下生成 log 文件 {tracename}.txt

## analyze.py

- `-a` 打印详细 log，在 ./result/{prefetcher} 下生成文件 {tracename}_full.txt
- `-t` 指定对某几个 log 进行分析
- `-p` 指定对某个预取器进行分析

读取 ./log/{prefetcher}/{tracename}.txt 进行分析，在 ./result/{prefetcher} 下生成 {tracename}_breif.txt