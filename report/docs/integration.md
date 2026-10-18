# 成员 C 集成与覆盖检查

检查日期：2026-10-18（北京时间）。成员 C：程子涵，2413921。

## 输入来源与目录映射

| 来源 | 固定提交 | 使用内容 |
| --- | --- | --- |
| [A：lab2](https://github.com/helloworld194910/NKOS/tree/27dbe9bbc627689402634cd853d52701cb798d0b) | `27dbe9bbc627689402634cd853d52701cb798d0b` | 初始化、First-Fit 解释、Best-Fit、内存探测回答、实际提示词 |
| [B：lab2-yang](https://github.com/helloworld194910/NKOS/tree/403c33031962321579b54dd0f77a4dfdd19f01f1) | `403c33031962321579b54dd0f77a4dfdd19f01f1` | 包含 A 的代码，另有 Buddy、BUDDY 开关、28 个宿主场景和完整报告 |
| C 本地材料 | 10 月 17 日实现与原日志，10 月 18 日最终联调 | SLUB、MMU 探针、双 GDB 脚本、8 张指定目录原图、报告与覆盖核对 |

B 相对于 A 的差异只有 Buddy 源码、接入、宿主测试、设计说明与报告，因此以 B 的 `code/` 为完整底座，映射到本目录。来源提交号不冒充本次最终交付的 Git 提交号。

## C 的集成修改

- 新增 `kern/mm/slub.c`、`slub.h`、`slub_check.c`、`mmu_probe.c`；代码标记包含 2413921。
- `init.c` 增加显式 MMU/SLUB 条件调用。SLUB 后再次检查底层管理器，Buddy 日志中可见前后两次不变量检查成功。
- Makefile 保留 B 的 BUDDY 开关和默认 Best-Fit，增加 SLUB_CHECK/MMU_PROBE、宿主测试入口、可覆盖的现代 RISC-V 架构参数。清理目标覆盖 obj/bin/build。
- 自动调试直接使用最终内核，不再补写替代 `page_init()`。`pmm.c`、`best_fit_pmm.c`、`buddy_pmm.c` 的算法来自固定输入分支，未替换其实现。
- SLUB 与 Buddy 取还均传原始请求页数。8193 字节对象加头部请求 3 页，Buddy 实际占 4 页，释放仍传 3；最终总计数恢复。
- Windows Git 检出使部分脚本为 CRLF，首次评分在 shell 解析阶段失败；统一源码/脚本为 LF 后重跑，20/20。未修改评分规则或原有两个 Best-Fit 测试。

## 本次真实验证

| 项目 | 结果 | 证据（均在 evidence/member-c） |
| --- | --- | --- |
| SLUB ASan/UBSan | 两轮检查、底层失败、耗尽与碎片化通过，恢复 256 页 | `slub-host.log` |
| Buddy 宿主测试 | 28/28，含 18 个预期拒绝场景 | `buddy-host.log` |
| 默认配置干净构建与 QEMU | Best-Fit，PMM 成功，31929 空闲页 | `build-default.log`、`pmm-default.log` |
| Buddy＋SLUB | 恢复 31792 页，SLUB 前后 Buddy 不变量成功 | `build-buddy.log`、`slub-qemu-buddy.log` |
| Best-Fit＋SLUB＋探针 | 恢复 31927 页，PMM/SLUB 成功 | `build-bestfit.log`、`slub-qemu-final.log` |
| 原有评分 | Page Init、Best-Fit 均 OK，20/20 | `grade.log` |
| 最终内核双 GDB | Bare 真实存储；Sv39 两次 ld、一次目标 walk；TLB 快照 | `qemu-host-trace.log`、`ucore-guest-trace.log` |
| 最终内核页表单步 | PTE、PPN、PA、权限与手算一致 | `qemu-host-step.log`、`ucore-guest-step.log` |
| 原有测试完整性 | basic_check / best_fit_check 与原骨架一致 | `integration.json` 中规范化哈希 |

各构建内核大小不同，页数与各自基线核对，不进行跨配置泄漏推断。Buddy 宿主测试覆盖更多行为边界，QEMU 的 `.check` 主要检查结构；28 个独立场景没有在真实客体中逐个执行。

## 调试六项与报告七项

| 要求 | 交付 |
| --- | --- |
| 调用路径、分支、真实翻译 | 第四章调试任务一与 trace 日志；prompt2.md |
| 页表单步、实际数值与手算 | 第四章调试任务二与 step 日志；prompt3.md |
| TLB 查询、miss/fill/hit | 第四章调试任务三；宿主三次快照与客体两条 ld；prompt4.md |
| Bare/Sv39、软件与硬件区别 | 第四章调试任务四，真实 sd 与 ld 对照；prompt5.md |
| 有趣现象与知识 | 第四章调试任务五；prompt6.md |
| AI 问题情景、思路、修正 | 第四章调试任务六；prompt7.md；明确整理稿与真实交互范围 |
| Markdown 与文本说明 | report.md，8 张图配解释 |
| 整体逻辑、核心函数 | 第三章、第四章各任务 |
| spec 与 prompt | prompt.md—prompt7.md；README 逐份列出七项检查 |
| bug 与修正 | 第四章各实现迭代与调试任务六、本页 CRLF 修复 |
| OS 原理对应、差异及未覆盖内容 | 第六章 |

## 全组任务检查与移交

| 任务 | 状态 |
| --- | --- |
| A 初始化与 Best-Fit | 已接入，启动与评分通过；无效 DTB 等边界不因评分通过就宣称全部动态验证 |
| A First-Fit、硬件探测回答、两份 prompt | 材料在 reports/A。原稿未填写的模板已删除，Best-Fit 另附明确署明 C 整理的代码阅读说明，不虚构 A 的迭代历史 |
| B Buddy、两份 prompt、设计与测试 | 完整材料在 reports/B、docs/buddy_*；本次补充 C 联调 |
| C 六项调试、SLUB、测试、报告 | 已完成；SLUB 两份与 QEMU 六份提示词独立保存；最终联调依赖已消除 |
| lab2 标记和学号 | 保留 A 的 2411824、B 的 2412790；C 为 2413921 |
| 目录与附件 | 已统一为 labcodes/lab2，图片使用相对链接 |
| 小组确认、最终远端上传 | 未执行；按原分工由 A/B 处理，本目录供用户检查，无虚构提交号 |

A/B 未提供的个人工具型号、边界实验或 AI 对话不由 C 编造。C 的代码、报告、证据和复现资料已可交接，三人仍需确认小组总报告及最终远端版本。
