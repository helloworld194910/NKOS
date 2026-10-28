# Lab3：中断与异常

杨茜茹（2412790）的工作分支 `lab3_yang`。Trap 基础汇编、S→U→S 底层实现及两组独立 QEMU 验证已完成；正式内核联调与原评分尚未完成。

小组成员：杨茜茹（2412790）、程子涵（2413921）、于蕾（2411824）。

## 文件入口

| 目录或文件 | 内容 |
| --- | --- |
| [code/](code/) | 内核、公共库、构建工具、扩展及独立测试 |
| [report/report.md](report/report.md) | 按课程模板填写的小组报告；本人进度已填，队友部分待补 |
| [report/prompt.md](report/prompt.md) | 本人三份完整四段提示词；预留队友任务位置 |
| [report/images/](report/images/) | 两张实际测试截图 |
| [report/handoff/](report/handoff/README.md) | 给程子涵、于蕾的交接说明及接口协议 |
| [report/demo.md](report/demo.md) | 本人部分的演示命令与讲解 |
| [report/logs/](report/logs/) | 已完成测试的原始日志与结果记录 |
| [report/checklist.md](report/checklist.md) | 当前完成情况及后续所需输入 |
| [report/notes/](report/notes/) | 设计细节、分项验证说明与提示词分稿；正文以 report.md、prompt.md 为准 |

逐个文件的职责、修改归属与证据范围见 [文件用途索引](report/file_guide.md)。

## 队友交接

- [程子涵：中断、时钟与返回联调](report/handoff/to_member_b.md)
- [于蕾：PMM、异常及用户态上层接入](report/handoff/to_member_c.md)
- [Trap 返回约定](report/handoff/trap_return_contract.md)
- [S→U→S 接口与栈协议](report/handoff/switch_contract.md)

尚未开始的成员可从本分支创建自己的工作分支，保留共同历史。交接压缩包另行发送，不放进仓库。

## 复现已完成的两组独立测试

在仓库根目录的 WSL 终端分别执行：

```bash
python3 code/tests/member_a/run_context.py
```

```bash
python3 code/tests/member_a_switch/run_switch.py
```

脚本自动定位 `code/`，构建文件写入 `code/obj/`，新日志写入 `code/logs/`。仓库归档的旧日志位于 `report/logs/`，不由复跑覆盖。

已有记录为两组各 3/3 通过；它们不代表原 Context/Illegal/Timer 评分已通过。完整工程编译从 `code/` 开始，例如 `make -C code`，实际运行仍需相应模块完成。
