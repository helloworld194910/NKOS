> 当前状态（2026-10-20）：以下按阶段保留历史记录。A、B、C 代码已在 lab2-yang 完成所列集成回归；最终汇总见 [小组总报告](../../report/report.md)。前文的“待接入”“待验证”不得当作当前未完成项。

# Buddy 独立测试设计与结果

本轮使用第二份测试提示词，为 Buddy 的已实现契约提供可重复运行的独立测试。测试直接链接 `kern/mm/buddy_pmm.c`，通过 `buddy_pmm_manager` 的公开回调调用算法，不包含其 C 文件，不访问或修改内部 static 状态，也不改动算法、公共头文件或原有评分测试。

## 运行方式

在 WSL 的项目根目录执行：

```bash
make buddy-host-test
```

依赖原生 GCC、GNU Make 和 Python 3，不依赖 RISC-V 编译器、QEMU 或 `page_init()`。该独立目标跳过内核依赖文件的自动重建，使用独立 `obj/buddy-host/` 目录，避免混用交叉编译对象。

构建由三个编译单元组成：真实 Buddy 实现、公共接口测试、宿主平台适配。前两个使用真实工程头文件，平台适配使用宿主标准库头文件且不添加工程 include 路径，避免内核 `size_t` 等定义与 libc 冲突。采用 GNU99、O2、Wall、Wextra、Werror 及 fno-builtin。

产物为 `obj/buddy-host/buddy-test`，每项输出保存在 `obj/buddy-host/logs/用例名.log`。全部通过时输出 `RESULT: 28/28 passed`；任一失败，驱动和 make 应以非零状态结束。不要使用会掩盖退出状态的管道记录结果。

单独调试某个场景：

```bash
./obj/buddy-host/buddy-test nonpartners
./obj/buddy-host/buddy-test free_rounded
```

第二个场景预期产生断言并返回 86，直接运行出现非零是预期；完整驱动负责判定它是否准确命中预期断言。

## 夹具和预期值

模拟 Page 数组使用真实 `struct Page`，全局 `nbase` 使用 `DRAM_BASE / PGSIZE`，`npage = nbase + 描述符数量`。每个进程创建独立状态；待登记页从保留状态开始。大部分案例使用 64 页，阶数覆盖案例使用当前完整容量 32768 页。

测试单独维护登记位图、占用位图和预期空闲数；每次成功分配验证实际占用区间、绝对物理页号对齐及不重叠，并按原始请求长度释放。关键操作后同时核对独立模型、公开计数及管理器基础检查。未登记区域应保持保留状态。

分配失败的案例保存并逐字段对比公开描述符状态，另检查计数、结构不变量及后续正常分配。此方法验证可观察行为；不宣称对私有状态逐字节证明不变。非法操作在进程退出后无法检查状态，因此失败前是否先验证后修改还通过代码审查确认。

同阶分配顺序不作假设。需要地址断言时，先耗尽并按返回地址识别块，或构造唯一合法候选。请求长度和实际页数由测试独立计算；小规模合并案例的预期地址及数量预先手算。

## 覆盖矩阵

| 原提示词 Case | 可运行用例 | 关键预期及检验价值 |
| --- | --- | --- |
| 1 初始化 | `empty` | 空闲计数为 0，申请失败，描述符不变 |
| 2 单页与各阶 | `orders` | 1 至 32768 页的每一阶均可分配释放，按物理页号对齐，计数恢复 |
| 3 非二次幂 | `rounding` | 3、5、9 页实际占用 4、8、16 页，传原始请求释放 |
| 4 分裂耗尽 | `split` | 32 个单页互异，耗尽失败，按乘数 13 生成的排列释放后可申请整个 32 页块 |
| 5 递归合并 | `merge` | 8 个单页按地址识别，分阶段释放并申请 2、4、8 页，证明逐级恢复大块 |
| 6 相邻非伙伴 | `nonpartners` | 保留外侧块，仅释放 `[2,4)`、`[4,6)`，总量 4 但不能申请 4；释放剩余后恢复 8 |
| 7 不规整区间 | `irregular` | `[3,16)`、`[5,19)` 可逐页耗尽且完整恢复，边界外页面不被使用 |
| 8 多次登记 | `registration` | 逆序登记 `[20,24)`、`[16,20)` 可形成 8 页块；有洞的 `[32,34)`、`[36,38)` 不能形成 4 页块 |
| 9 失败不变 | `failure` | 超总量、size_t 极大值失败，公开状态不变；既有分配可正常释放，后续分配正常 |
| 10 多轮交错 | `interleave` | 固定种子 0x2badd00d，4 轮各 256 步，混合 1 至 17 页请求；逐步核对存活区间和计数，最终恢复 64 页整块 |
| 11 非法申请释放 | 下表 11 项 | 每项隔离运行，命中精确断言而非无关失败 |
| 12 非法登记容量 | 下表 7 项 | 登记参数、重叠和容量检查，测试夹具本身不越界 |

上述下标区间的精确地址案例先断言真实 nbase 满足必要对齐，避免将数组下标当成一般物理页号。当前工程 nbase 满足这些约束。

`interleave` 还使用独立字节数组模拟页内容，对每个存活分配的实际占用空间写不同模式，逐步验证模式。它补充说明模型中的区间隔离，不代表真实物理页写入或 MMU 行为已验证。算法不会访问这份模拟内容数组。

## 18 个异常场景

| 场景 | 目的 |
| --- | --- |
| `alloc_zero` | 申请零页 |
| `free_zero` | 释放长度为零 |
| `free_null` | NULL 释放 |
| `free_outside` | 描述符范围外地址 |
| `free_unaligned` | 指向描述符内部的未对齐地址 |
| `free_interior` | 分配块内部的非首描述符 |
| `free_length` | 申请 3 页却用 2 页释放 |
| `free_rounded` | 申请 3 页却用实际大小 4 页释放 |
| `free_twice` | 重复释放 |
| `free_reserved` | 释放未登记保留页 |
| `free_unregistered` | 释放未登记且人为清除保留位的页 |
| `register_zero` | 登记零页 |
| `register_start` | 登记起点超出描述符范围 |
| `register_end` | 登记区间末尾越界 |
| `register_nonreserved` | 输入区间包含非保留页 |
| `register_twice` | 重复登记相同区间 |
| `register_overlap` | 部分重叠登记 |
| `capacity` | 描述符数量超过私有静态容量；fixture 实际有足够空间，避免夹具先越界 |

异常操作前输出并刷新 `EXPECTED_OPERATION`。平台适配 `__panic` 输出来源文件、行号和真实断言表达式，退出码为 86。驱动同时要求：标记出现且仅一次、标记前未 panic、panic 来自真实 `buddy_pmm.c`、表达式与该场景一致、退出码为 86、非法操作没有正常返回。测试自身断言失败、段错误、超时、进程启动失败或未知场景不能判通过。每个场景超时为 15 秒。

宿主适配只实现输出与终止，不替换框架 assert/panic 宏、不实现分配算法。为保持日志简洁，抑制 cprintf 的可选成功输出；断言诊断仍完整记录。

## 实际执行记录

2026 年 10 月 17 日，助手在 Windows 宿主上使用 MSYS2 UCRT64 GCC 和 Python 3.12，对真实工程文件的副本进行了同等编译组织和驱动运行。三个编译单元分开构建，严格警告通过；运行结果为 **28/28 passed**，10 个正常场景及 18 个异常场景全部通过。本轮没有发现需要修改分配算法的问题。

示例异常证据：`free_rounded` 在标记后命中真实分配器的 `records[index].requested == n` 断言，并返回 86。这是预期拒绝错误释放长度，不是未处理的测试故障。

Windows 本次编译命令形态如下，平台单元不带工程头文件路径：

```text
gcc -std=gnu99 -Wall -Wextra -Werror -O2 -fno-builtin -nostdinc -Ilibs -Ikern/mm -Ikern/debug -c kern/mm/buddy_pmm.c
gcc -std=gnu99 -Wall -Wextra -Werror -O2 -fno-builtin -nostdinc -Ilibs -Ikern/mm -Ikern/debug -c tools/buddy_host/cases.c
gcc -std=gnu99 -Wall -Wextra -Werror -O2 -fno-builtin -c tools/buddy_host/platform.c
gcc allocator.o cases.o platform.o -o buddy-test.exe
python tools/buddy_host/run.py buddy-test.exe
```

这是命令结构摘要；实际使用工作区副本的绝对路径和独立输出目录。正式项目提供的 Makefile 自动组织对应 Linux 命令，不需要手工复制源文件。

随后，本人在 VS Code 的 Ubuntu WSL 终端执行 `git diff --check` 和 `make buddy-host-test`，反馈了完整输出：格式检查无错误，三个宿主编译单元编译和链接成功，10 个正常场景及 18 个隔离异常场景全部通过，最终为 `RESULT: 28/28 passed`。项目 `obj/buddy-host/logs/` 下的 28 份日志已核对。该 WSL/Linux 结果由本人执行并反馈，不是助手直接调用 WSL 所得。

实现提交为 `876e009`，测试提交为 `fc84bdb`。这些结果确认宿主算法测试完成，不代表完整内核或 QEMU 启动已验证。

## 仍需完成

- WSL 宿主测试已完成；后续修改模块或测试后按需重新运行。
- A 的初始化接入后执行干净的 RISC-V 全工程构建与 QEMU 启动。
- 在内核中验证管理器选择、真实描述符布局与初始空闲页数；后续与默认 Best-Fit、C 的 SLUB 集成。
- 当前固定的多轮序列不是所有可能序列的穷举，不验证多核并发。
- 最终报告准确区分宿主算法验证、RISC-V 编译、QEMU 动态结果及代码审查，不将未运行部分写成已通过。
