# Lab3 工程说明

本目录是工程根目录，包含 `Makefile`、内核、公共库、原评分工具与独立扩展测试。小组报告和截图在上一级的 `report/`。

| 路径 | 作用与当前归属 |
| --- | --- |
| `kern/trap/trapentry.S` | 杨茜茹完成 Trap 入口传帧与上下文恢复；保留原 SAVE_ALL |
| `kern/trap/trap.c` | 统一分派框架；中断由程子涵负责，异常由于蕾负责 |
| `kern/mm/pmm.c`、`best_fit_pmm.c` | Lab2 依赖补齐，由于蕾负责 |
| `extensions/member_a_switch/` | 已取消 Challenge 1 的历史探索源码，不参与正式内核构建，不要求接入 |
| `tests/member_a/` | 真实 Trap 汇编的三个独立上下文验证场景 |
| `tests/member_a_switch/` | 已取消任务的历史自测夹具，不作为课程测试要求 |
| `tools/` | 原构建、链接、调试与评分工具；不能用修改评分脚本代替修复实现 |

所有文件的具体用途见 [文件用途索引](../report/file_guide.md)。

从仓库根目录运行独立测试：

```bash
python3 code/tests/member_a/run_context.py
```

或进入本目录后执行 `python3 tests/...`。本目录下的 `obj/`、`bin/` 和新生成的 `logs/` 不提交；已取得的测试记录归档在 [report/logs/](../report/logs/)。

上下文独立测试已有 3/3 通过记录；完整内核评分及持续时钟路径仍需正式模块完成后验证。教师已取消旧版用户态切换 Challenge 1，相关历史源码不参与正式构建，不再要求于蕾完成用户态上层。
