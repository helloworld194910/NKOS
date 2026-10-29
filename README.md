# Lab3：中断与异常

小组成员：杨茜茹（2412790）、程子涵（2413921）、于蕾（2411824）。个人工作分支：`lab3_yang`。

教师已明确：旧版 Challenge 1（用户态／内核态切换）今年无需完成。当前正式任务为实验2依赖补齐、Trap 上下文与入口、中断和异常处理，以及最新版手册保留的机制分析。历史态切换材料仅归档，不要求队友接入。

| 入口 | 内容 |
| --- | --- |
| [code/](code/README.md) | 内核、公共库、构建及评分工具和独立上下文验证 |
| [report/report.md](report/report.md) | 按课程模板填写的小组报告，待填位置按姓名标明 |
| [report/prompt.md](report/prompt.md) | 杨茜茹的两份完整提示词与队友待填位置 |
| [文件用途索引](report/file_guide.md) | 各文件用途与课程任务、历史材料的区分 |
| [交接入口](report/handoff/README.md) | 给程子涵、于蕾的正式任务交接 |
| [完成情况](report/checklist.md) | 已完成证据与尚待正式联调的项目 |
| [演示步骤](report/demo.md) | Trap 上下文独立验证命令与讲解 |
| [真实截图](report/images/) / [原始日志](report/logs/) | 已取得的运行证据，保持原始内容 |
| [取消任务的历史归档](report/archive/s-u-s/README.md) | 已做过的 S→U→S 探索，不作为本次完成条件 |

## 分工与当前状态

- 杨茜茹：基础 Trap 入口、恢复和上下文独立测试已有 3/3 通过记录；正式 Context、断点及中断返回待联调。
- 程子涵：中断时钟、关机、`stvec` 与临界区；组织测试、评分及截图汇总。
- 于蕾：补齐 `pmm.c`、`best_fit_pmm.c` 的 Lab2 缺口，完成异常处理；负责最终代码集成、报告整合、干净目录复现与提交。

## 独立上下文验证

在仓库根目录运行：

```bash
python3 code/tests/member_a/run_context.py
```

应检查三个 PASS 与 `RESULT: 3/3 context scenarios passed`。产物位于 `code/obj/`，新日志位于 `code/logs/`。这项独立验证不能替代正式内核 Context/Illegal/Timer 评分。

完整内核使用 `make -C code` 等命令；先补齐相应模块，再记录实际编译、启动和评分。交接压缩包另行发送，不上传仓库。
