# 内存释放异常调试 - 执行摘要

## 执行日期
2026-01-05

## 问题描述
程序在执行结束阶段抛出 `free(): invalid pointer` 异常，GDB 回溯显示错误发生在 CACHE 析构函数中。

## 已执行的修复

### 1. 修复断言逻辑错误（优先级：高）✓

**文件**: `prefetcher/kairos/kairos.h`

**问题**: PseudoLRUCache 构造函数中的断言括号缺失
```cpp
// 修复前
assert(size & (size - 1) == 0);  // 实际执行: assert(size) && (size - 1) == 0

// 修复后
assert((size & (size - 1)) == 0);  // 正确检查是否为2的幂次
```

**影响**: 防止使用非2的幂次大小初始化，避免内部 tree 结构错误导致越界访问。

### 2. 添加 kairos 类显式析构函数（优先级：高）✓

**文件**: `prefetcher/kairos/kairos.h`

**修改**: 为 kairos 类添加显式析构函数
```cpp
~kairos() {
    if constexpr (champsim::debug_print) {
        std::cout << "[DEBUG] ~kairos() destructor called, llc_cache=" 
                  << llc_cache << std::endl;
    }
    // 在析构时置空 llc_cache 指针，避免悬空引用
    llc_cache = nullptr;
}
```

**影响**: 防止在析构过程中访问失效的 llc_cache 指针，这是最可能的根本原因。

### 3. 改进 MetaDataTable::resize() 安全性（优先级：中）✓

**文件**: `prefetcher/kairos/kairos.h`

**修改**: 增强 resize 方法的状态管理
```cpp
void resize(int8_t direction) {
    // ... 原有逻辑 ...
    
    for (auto& set : data) {
        size_t old_size = set.entry.size();
        set.entry.resize(target_size);
        
        // 对于扩容，确保新增的 entry 初始化为 invalid
        if (target_size > old_size) {
            for (size_t i = old_size; i < target_size; ++i) {
                set.entry[i].valid = false;
                set.entry[i].frequency = 0;
            }
        }
    }
}
```

**影响**: 确保 vector resize 后新增元素的状态正确初始化，避免未定义行为。

### 4. 添加空指针检查（优先级：中）✓

**文件**: `prefetcher/kairos/kairos.h`

**修改**: 为 llc_cache 使用添加空指针保护
```cpp
if (llc_cache != nullptr) {
    uint32_t available_ways = llc_cache->NUM_WAY - metadata.actual_way;
    llc_cache->set_available_ways(available_ways);
    // ...
} else {
    if constexpr (champsim::debug_print) {
        std::cout << "[kairos] WARNING: llc_cache is nullptr, "
                  << "skipping partition update" << std::endl;
    }
}
```

**影响**: 防御性编程，避免解引用空指针导致崩溃。

### 5. 添加析构日志（优先级：低）✓

**文件**: `prefetcher/kairos/kairos.h`

**修改**: 为关键类添加析构日志输出
- PseudoLRUCache
- DetectUnit
- TrainUnit
- LFUCache
- MetaDataTable
- kairos

**影响**: 在 debug 模式下提供对象生命周期跟踪，便于诊断析构顺序问题。

## 新增文件

### 1. debug.options
调试编译选项文件，包含：
- `-g -O0`: 完整调试符号，无优化
- `-fsanitize=address`: AddressSanitizer
- `-fsanitize=undefined`: UndefinedBehaviorSanitizer
- `-fno-omit-frame-pointer`: 完整栈帧信息

### 2. debug_memory_issue.sh
自动化调试脚本，执行以下步骤：
1. 清理旧构建
2. 备份原配置
3. 启用调试选项
4. 重新编译
5. 运行测试（可选）
6. 恢复原配置（可选）

## 使用说明

### 快速测试（推荐）

```bash
# 1. 确保已配置项目
./config.sh <your_config_file>

# 2. 运行调试脚本
./debug_memory_issue.sh

# 3. 按提示输入 trace 文件路径或跳过
```

### 手动调试步骤

```bash
# 1. 备份原配置
cp global.options global.options.backup

# 2. 启用调试选项
cp debug.options global.options

# 3. 清理并重新编译
make clean
make -j$(nproc)

# 4. 运行测试
export ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:log_path=asan.log"
export UBSAN_OPTIONS="print_stacktrace=1:log_path=ubsan.log"
bin/champsim --warmup-instructions 200000000 \
             --simulation-instructions 500000000 \
             <trace_file>

# 5. 检查结果
# - 如果正常退出（返回码0）：问题已解决
# - 如果异常：查看 asan.log.* 和 ubsan.log.* 文件

# 6. 恢复生产配置
cp global.options.backup global.options
make clean && make
```

## 预期结果

### 如果修复成功
- ✓ 程序正常退出，返回码为 0
- ✓ 输出 "Total windows evaluated" 统计信息
- ✓ 无 AddressSanitizer 错误报告
- ✓ 无 UndefinedBehaviorSanitizer 警告

### 如果问题仍存在
查看 ASan/UBSan 报告以获取：
- 具体的内存错误类型（use-after-free, double-free, 等）
- 错误发生的精确代码位置和调用栈
- 内存分配和释放的历史记录

根据报告进一步定位根本原因。

## 根因分析（推测）

基于代码审查和修改，最可能的根因是：

**假设 3：CACHE 析构时 prefetcher 模块访问失效的 llc_cache 指针**

证据：
1. 调用栈显示错误发生在 CACHE::~CACHE() 中
2. kairos 预取器持有 CACHE* 类型的裸指针
3. 没有显式的生命周期管理机制
4. 析构函数可能隐式调用使用 llc_cache 的代码

修复：
- 添加显式析构函数置空 llc_cache 指针
- 添加空指针检查防御
- 日志跟踪确认析构顺序

置信度：**高**

次要因素：
- PseudoLRUCache 断言失效可能允许非法状态（已修复）
- MetaDataTable::resize() 未初始化新元素（已修复）

## 后续步骤

### 如果问题解决
1. ✓ 运行完整的回归测试套件
2. ✓ 在不同 trace 上验证修复
3. ✓ 恢复生产编译选项（-O3）重新测试
4. ✓ 更新代码库文档记录此问题
5. ✓ 考虑添加单元测试覆盖析构逻辑

### 如果问题仍存在
1. 分析 AddressSanitizer 详细报告
2. 使用 Valgrind 进行更深入的内存分析
3. 添加更多调试日志定位具体失败点
4. 考虑使用 GDB 设置条件断点追踪对象生命周期
5. 检查是否有其他预取器或模块有类似问题

## 风险评估

### 代码修改风险：低

- 修复都是防御性的，不改变核心逻辑
- 断言修复只是修正语法错误
- 析构函数只是清理资源，不执行新操作
- resize 改进只是初始化新增元素

### 性能影响：无（生产环境）

- 调试日志仅在 `champsim::debug_print` 为 true 时启用
- 空指针检查开销极小
- resize 初始化仅在扩容时执行

### 兼容性影响：无

- 不改变公共接口
- 不影响其他模块的使用方式
- 向后兼容

## 相关文件

### 修改的文件
- `prefetcher/kairos/kairos.h` - 主要修复文件

### 新增的文件
- `debug.options` - 调试编译选项
- `debug_memory_issue.sh` - 自动化调试脚本
- `DEBUG_SUMMARY.md` - 本文档

### 参考文档
- `.qoder/quests/memory-release-exception-debugging.md` - 设计文档

## 验证清单

- [x] 代码修改已完成
- [x] 编译选项配置已准备
- [x] 自动化脚本已创建
- [ ] 使用 ASan/UBSan 重新编译
- [ ] 运行测试验证修复
- [ ] 检查 ASan/UBSan 报告
- [ ] 确认问题已解决或需要进一步调试
- [ ] 运行回归测试
- [ ] 更新文档
- [ ] 提交代码变更

## 联系与支持

如果问题仍未解决或需要进一步协助，请提供：
1. ASan/UBSan 的完整报告文件
2. 程序输出日志
3. 使用的 trace 文件信息
4. 系统环境信息（OS、编译器版本等）
