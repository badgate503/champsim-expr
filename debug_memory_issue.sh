#!/bin/bash

# 内存释放异常调试脚本
# 根据设计文档执行调试流程

set -e  # 遇到错误立即退出

WORKSPACE="/mnt/data/lyq/Kairos"
cd "$WORKSPACE"

echo "==============================================="
echo "内存释放异常调试流程"
echo "==============================================="
echo ""

# 检查是否存在配置文件
if [ ! -f "_configuration.mk" ]; then
    echo "❌ 错误: 未找到 _configuration.mk 配置文件"
    echo "请先运行: ./config.sh <config_file> 来配置项目"
    exit 1
fi

echo "步骤 1: 清理旧的构建文件"
echo "---------------------------------------"
make clean
echo "✓ 清理完成"
echo ""

echo "步骤 2: 备份原有的 global.options"
echo "---------------------------------------"
if [ -f "global.options.backup" ]; then
    echo "备份文件已存在，跳过备份"
else
    cp global.options global.options.backup
    echo "✓ 已备份到 global.options.backup"
fi
echo ""

echo "步骤 3: 启用调试构建选项（AddressSanitizer + UBSan）"
echo "---------------------------------------"
cp debug.options global.options
echo "✓ 已启用调试选项:"
echo "  - 调试符号: -g -O0"
echo "  - AddressSanitizer: -fsanitize=address"
echo "  - UndefinedBehaviorSanitizer: -fsanitize=undefined"
echo ""

echo "步骤 4: 重新编译项目"
echo "---------------------------------------"
echo "开始编译... (这可能需要几分钟)"
if make -j$(nproc); then
    echo "✓ 编译成功"
else
    echo "❌ 编译失败"
    echo "恢复原配置..."
    cp global.options.backup global.options
    exit 1
fi
echo ""

echo "步骤 5: 准备测试"
echo "---------------------------------------"
# 查找可执行文件
EXECUTABLE=$(find bin -type f -executable | head -1)
if [ -z "$EXECUTABLE" ]; then
    echo "❌ 未找到可执行文件"
    exit 1
fi
echo "✓ 找到可执行文件: $EXECUTABLE"
echo ""

echo "步骤 6: 运行内存检测测试"
echo "---------------------------------------"
echo "注意: 如果程序需要 trace 文件作为输入，请手动运行:"
echo "  ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:log_path=asan.log \\"
echo "  $EXECUTABLE --warmup-instructions 200000000 \\"
echo "              --simulation-instructions 500000000 <trace_file>"
echo ""
echo "如果您有合适的 trace 文件，请输入路径（或按 Enter 跳过）:"
read -r TRACE_FILE

if [ -n "$TRACE_FILE" ] && [ -f "$TRACE_FILE" ]; then
    echo "运行测试..."
    export ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:log_path=asan.log"
    export UBSAN_OPTIONS="print_stacktrace=1:log_path=ubsan.log"
    
    set +e  # 允许程序失败以便查看错误输出
    $EXECUTABLE --warmup-instructions 200000000 \
                --simulation-instructions 500000000 \
                "$TRACE_FILE" 2>&1 | tee test_output.log
    EXIT_CODE=$?
    set -e
    
    echo ""
    echo "==============================================="
    echo "测试完成，退出码: $EXIT_CODE"
    echo "==============================================="
    
    if [ $EXIT_CODE -eq 0 ]; then
        echo "✓ 程序正常退出，未检测到内存错误！"
        echo ""
        echo "这表明代码修复成功。建议："
        echo "1. 检查程序输出是否正常"
        echo "2. 运行更多测试用例验证"
        echo "3. 查看 asan.log 和 ubsan.log（如果存在）"
    else
        echo "❌ 程序异常退出或检测到内存错误"
        echo ""
        echo "请检查以下文件获取详细信息："
        echo "  - test_output.log: 程序输出"
        echo "  - asan.log.*: AddressSanitizer 报告"
        echo "  - ubsan.log.*: UBSan 报告"
        echo ""
        echo "常见错误类型："
        echo "  - heap-use-after-free: 使用已释放的内存"
        echo "  - heap-buffer-overflow: 堆缓冲区溢出"
        echo "  - double-free: 双重释放"
        echo "  - memory leak: 内存泄漏"
    fi
else
    echo "跳过运行测试"
    echo ""
    echo "手动测试说明："
    echo "1. 设置环境变量:"
    echo "   export ASAN_OPTIONS=\"detect_leaks=1:abort_on_error=1:log_path=asan.log\""
    echo "   export UBSAN_OPTIONS=\"print_stacktrace=1:log_path=ubsan.log\""
    echo ""
    echo "2. 运行程序:"
    echo "   $EXECUTABLE <your_args>"
    echo ""
    echo "3. 检查输出和日志文件"
fi

echo ""
echo "步骤 7: 恢复原配置（可选）"
echo "---------------------------------------"
echo "是否恢复原来的编译选项？(y/N)"
read -r RESTORE

if [[ "$RESTORE" =~ ^[Yy]$ ]]; then
    cp global.options.backup global.options
    echo "✓ 已恢复原配置"
    echo "如需使用优化版本，请重新编译: make clean && make"
else
    echo "保持调试配置"
    echo "注意: 调试版本性能较低，生产环境请使用原配置"
fi

echo ""
echo "==============================================="
echo "调试流程完成"
echo "==============================================="
echo ""
echo "代码修改摘要:"
echo "  ✓ 修复了 PseudoLRUCache 构造函数的断言括号问题"
echo "  ✓ 为 kairos 类添加了显式析构函数，置空 llc_cache 指针"
echo "  ✓ 改进了 MetaDataTable::resize() 的安全性"
echo "  ✓ 添加了 llc_cache 的空指针检查"
echo "  ✓ 为关键类添加了析构日志（debug 模式下）"
echo ""
echo "下一步建议:"
echo "1. 如果问题已解决，运行完整的回归测试"
echo "2. 如果问题仍存在，分析 ASan/UBSan 报告"
echo "3. 考虑使用 Valgrind 进行更深入的分析"
echo "4. 更新文档记录根因和解决方案"
