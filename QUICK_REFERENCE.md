# 内存释放异常调试 - 快速参考

## 立即执行

```bash
cd /mnt/data/lyq/Kairos
./debug_memory_issue.sh
```

## 代码修改摘要

### ✓ 已完成的修复

1. **断言括号修复** (kairos.h:70)
   ```cpp
   // 修复前: assert(size & (size - 1) == 0);
   // 修复后: assert((size & (size - 1)) == 0);
   ```

2. **kairos 析构函数** (kairos.h:470-478)
   ```cpp
   ~kairos() {
       llc_cache = nullptr;  // 防止悬空指针
   }
   ```

3. **MetaDataTable::resize() 改进** (kairos.h:312-339)
   - 初始化新增元素的 valid 和 frequency 字段

4. **llc_cache 空指针检查** (kairos.h:449-465)
   - 添加 nullptr 检查保护

5. **析构日志** (多处)
   - 所有关键类添加 debug 日志

## 测试步骤

### 方法 1: 自动脚本（推荐）

```bash
./debug_memory_issue.sh
# 按提示操作
```

### 方法 2: 手动测试

```bash
# 1. 启用调试编译
cp global.options global.options.backup
cp debug.options global.options

# 2. 重新编译
make clean
make -j$(nproc)

# 3. 运行测试
export ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:log_path=asan.log"
export UBSAN_OPTIONS="print_stacktrace=1:log_path=ubsan.log"
bin/champsim <your_args>

# 4. 检查结果
echo $?  # 应该是 0
ls asan.log.* ubsan.log.*  # 查看是否有错误报告

# 5. 恢复生产配置
cp global.options.backup global.options
make clean && make
```

## 成功标志

✓ 程序返回码为 0  
✓ 输出 "Total windows evaluated"  
✓ 无 "free(): invalid pointer" 错误  
✓ 无 ASan/UBSan 报告文件  

## 失败处理

如果仍有错误：

1. **查看 ASan 报告**
   ```bash
   cat asan.log.*
   ```
   关注：heap-use-after-free, double-free, buffer-overflow

2. **查看 UBSan 报告**
   ```bash
   cat ubsan.log.*
   ```
   关注：undefined behavior 类型

3. **使用 Valgrind**
   ```bash
   valgrind --leak-check=full --show-leak-kinds=all \
            --track-origins=yes --log-file=valgrind.log \
            bin/champsim <your_args>
   ```

4. **GDB 调试**
   ```bash
   gdb bin/champsim
   (gdb) run <your_args>
   # 当崩溃时
   (gdb) bt  # 查看调用栈
   (gdb) info locals  # 查看局部变量
   ```

## 关键修复位置

| 文件 | 行号 | 修复内容 |
|------|------|----------|
| kairos.h | 70 | 断言括号 |
| kairos.h | 470-478 | kairos 析构 |
| kairos.h | 312-339 | resize 改进 |
| kairos.h | 449-465 | 空指针检查 |

## 文档

- 设计文档: `.qoder/quests/memory-release-exception-debugging.md`
- 执行摘要: `DEBUG_SUMMARY.md`
- 本指南: `QUICK_REFERENCE.md`

## 回滚

如果需要撤销所有修改：

```bash
cd /mnt/data/lyq/Kairos
git checkout prefetcher/kairos/kairos.h
cp global.options.backup global.options
make clean && make
```

## 问题报告

如果问题仍存在，请收集：
- `asan.log.*`
- `ubsan.log.*`
- `test_output.log`
- 程序输出的最后 100 行
- 使用的 trace 文件路径
- 系统信息 (`uname -a`, `g++ --version`)
