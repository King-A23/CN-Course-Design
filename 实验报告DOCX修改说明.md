# 实验报告 DOCX 修改说明

本文档用于指导后续修改 `计算机网络课程设计实验报告.docx`。它是一份修改清单，不直接替代实验报告正文，也不修改 Word 文件本体。后续编辑 DOCX 时，应按本说明同步更新章节内容、测试口径、目录编号和相关问答。

## 1. 修改目标

本次 DOCX 需要补充三类已经在代码和测试中落地的内容：

1. pending 映射满或 pending 插入失败时，服务器不再静默丢弃请求，而是向客户端返回 DNS `REFUSED`。
2. 上游 DNS 超时后，服务器会弹出超时 pending 项，并基于客户端原始查询构造 DNS `SERVFAIL` 返回客户端。
3. 将已完成的性能测试结果写入实验报告，包括 QPS、平均延迟、P50、P95、P99、最大延迟和上游请求数。

同时需要删除或替换旧报告中关于“上游超时只清理状态、不向客户端反馈失败”的过期描述。新的报告口径应强调：程序会主动反馈明确的 DNS 失败响应，避免客户端长时间等待；迟到上游响应仍然会被丢弃，避免影响后续请求。

## 2. 必须同步的实现变化

| 功能变化 | 当前实现 | 报告需要怎么改 |
|---|---|---|
| pending 映射满或插入失败反馈 | `dr_pending_map_insert` 失败时，`server` 调用 `respond_with_error(..., DR_DNS_RCODE_REFUSED, ...)`，向客户端返回 `REFUSED` | 在客户端查询流程、上游中继与 ID 转换、调试问题和答辩问答中补充 `REFUSED` 处理 |
| 上游 DNS 超时反馈 | `select` 周期唤醒后调用 `dr_pending_map_pop_expired`，对超时请求返回 `SERVFAIL` | 将旧的“超时只清理 pending、不返回 SERVFAIL”改为“清理 pending 后返回 SERVFAIL” |
| 迟到响应处理 | 超时项已删除，后续迟到上游响应按未知 ID 丢弃 | 保留“迟到响应丢弃”的说明，但要和 `SERVFAIL` 反馈组合描述 |
| 错误响应构造 | `dr_dns_build_error_response` 复用原查询 ID，并尽量保留规范化 Question | 在协议/实现描述中说明 `REFUSED` 和 `SERVFAIL` 都是 DNS 标准错误响应 |
| 性能测试结果 | 已生成 `report_assets/perf_results_2026-06-14.md` 和 `.json` | 在第 8 章新增性能测试小节，引用已记录数据，不重新造数据 |

## 3. 按章节修改清单

| DOCX 章节 | 修改类型 | 修改要点 | 参考依据 |
|---|---|---|---|
| `1. 实验目的与任务要求` | 补充 | 在功能概述中加入“pending 满返回 `REFUSED`、上游超时返回 `SERVFAIL`”这类异常反馈能力 | `src/server.c:208`、`src/server.c:357` |
| `4.2 方案选择` | 替换 | 将“超时策略：只清理状态，不伪造响应、不重传”替换为“超时策略：周期清理 pending，向客户端返回 `SERVFAIL`，迟到响应仍丢弃” | `src/server.c:354`、`src/server.c:363` |
| `6.2 客户端查询流程` | 补充 | 在 Cache 未命中、准备转发上游之前说明：若无法分配 pending 上游 ID，则构造 `REFUSED` 返回客户端 | `src/server.c:209`、`src/server.c:221` |
| `7.3 上游中继与 ID 转换` | 补充 | 在 pending 保存客户端 ID、地址、查询三元组后补充失败分支：pending 无可用槽位或插入失败时，服务端返回 `REFUSED`，不转发上游 | `src/server.c:208`、`src/server.c:222`、`src/pending_map.c` |
| `7.4 超时与迟到响应` | 重点改写 | 删除“中继服务器不向客户端伪造 `SERVFAIL`”相关表述，改为“pending 超过默认 5000ms 后被弹出，并基于原始查询返回 `SERVFAIL`；若上游响应之后才到达，则因 pending 已删除而被丢弃” | `include/dnsrelay/config.h`、`src/server.c:357`、`src/server.c:363` |
| `8. 测试用例与运行结果` | 新增/调整 | 在 `8.2 人工运行验证` 后新增 `8.3 性能测试`；原 `8.3 验收清单` 顺延为 `8.4`。同时在测试说明中补充 `silent.example` 验证超时 `SERVFAIL` | `tests/test_server_integration.py:204`、`report_assets/perf_results_2026-06-14.md` |
| `9. 调试中遇到并解决的问题` | 新增两行 | 新增“pending 映射满导致无法继续转发”的问题，解决方式为返回 `REFUSED`；新增“上游无响应导致客户端长时间等待”的问题，解决方式为超时返回 `SERVFAIL` | `src/server.c:221`、`src/server.c:370` |
| `10. 答辩准备` | 替换/新增 | 删除或改写“上游超时为什么不返回 `SERVFAIL`？”问答；新增“为什么 pending 满返回 `REFUSED`？”和“为什么上游超时返回 `SERVFAIL`？” | DNS RCODE 语义、`src/server.c` |
| `11. 课程设计总结` | 微调 | 总结中把“上游超时处理”表述为“超时反馈、迟到响应丢弃、pending 状态回收” | `src/server.c`、集成测试 |

## 4. 需要替换的旧表述

| 旧表述或旧含义 | 新表述方向 |
|---|---|
| “上游超时只清理状态，不伪造响应、不重传。” | “上游超时后清理 pending，并基于原始查询构造 `SERVFAIL` 返回客户端；程序不主动重传，上游迟到响应仍会被丢弃。” |
| “中继服务器不向客户端伪造 `SERVFAIL`。” | “中继服务器在确认等待上游超过超时时间后，返回标准 DNS `SERVFAIL`，表示服务器侧无法完成该查询。” |
| “保持 DNS Relay 透明性，避免改变客户端原本面对上游无响应时的行为。” | “主动反馈失败原因，避免客户端无谓等待；同时保留 pending 清理和迟到响应丢弃机制，避免串包。” |
| “上游超时为什么不返回 `SERVFAIL`？” | 改为“上游超时为什么返回 `SERVFAIL`？” |

注意：如果需要在报告中引用旧说法，只能出现在“旧表述需要替换”的说明上下文中，不能作为最终实现结论保留。

## 5. 性能测试加入位置

在 DOCX 的 `8.2 人工运行验证` 后新增：

```text
8.3 性能测试
```

原 `8.3 验收清单` 顺延为：

```text
8.4 验收清单
```

性能测试数据直接引用以下文件，不重新跑、不重新编造：

| 数据文件 | 用途 |
|---|---|
| `report_assets/perf_results_2026-06-14.md` | 给 Word 报告粘贴性能测试说明、表格和结论 |
| `report_assets/perf_results_2026-06-14.json` | 作为机器可读原始汇总数据 |
| `report_assets/perf_runs/clients_1.json` | 1 并发原始结果 |
| `report_assets/perf_runs/clients_4.json` | 4 并发原始结果 |
| `report_assets/perf_runs/clients_8.json` | 8 并发原始结果 |

Word 中建议保留这些核心结论：

| 并发客户端 | 场景 | 请求数 | QPS | 平均延迟(ms) | P95(ms) | P99(ms) | 上游请求数 |
|---:|---|---:|---:|---:|---:|---:|---:|
| 8 | `local_table_hit` | 3000 | 13808.6 | 0.569 | 0.736 | 0.921 | 0 |
| 8 | `cache_hit` | 3000 | 14684.1 | 0.541 | 0.621 | 0.664 | 0 |
| 8 | `relay_miss` | 1000 | 7247.1 | 1.096 | 1.208 | 1.262 | 1000 |

若 Word 篇幅允许，可以使用 `report_assets/perf_results_2026-06-14.md` 中的完整 9 行性能表；若篇幅紧张，至少保留上表和完整数据文件引用。

## 6. 测试和验收口径

DOCX 中测试章节需要同步以下口径：

| 检查项 | 报告口径 |
|---|---|
| 常规自动化测试 | 未启用性能测试时为 `6/6 Passed` |
| 启用性能测试后的全量测试 | `7/7 Passed` |
| 性能冒烟测试 | `ctest --test-dir build -L perf --output-on-failure` 为 `1/1 Passed` |
| 上游超时反馈 | `silent.example` 不返回上游响应，客户端最终收到 `SERVFAIL`，`ANCOUNT=0` |
| Cache 命中反馈 | 重复查询 `relay-only.example` 不再访问上游 |
| 上游响应异常 | Question mismatch 响应被丢弃，正确响应仍返回客户端 |

建议把验收清单中的“自动化测试”结论改为：

```text
常规 6/6 Passed；启用性能测试后 7/7 Passed。
```

## 7. 答辩准备建议改法

| 问题 | 建议回答要点 |
|---|---|
| 为什么 pending 满返回 `REFUSED`？ | pending 表示服务器当前无法继续保存新的上游转发状态。此时返回 `REFUSED` 比静默丢弃更明确，客户端能立即知道服务器拒绝处理该查询。 |
| 为什么上游超时返回 `SERVFAIL`？ | 上游长时间无响应时，服务器无法完成递归查询。返回 `SERVFAIL` 是标准 DNS 失败响应，可避免客户端一直等待，同时释放 pending 状态。 |
| 返回 `SERVFAIL` 后迟到响应怎么办？ | 超时 pending 已被删除，迟到包到达后按未知上游 ID 丢弃，不会错误回发给其他客户端。 |
| `REFUSED` 和 `SERVFAIL` 有什么区别？ | `REFUSED` 表示服务器拒绝处理该请求，适合 pending 无可用槽位；`SERVFAIL` 表示服务器处理过程中失败，适合上游 DNS 超时。 |

## 8. 后续修改 DOCX 检查清单

- [ ] 刷新或重建目录，确认 `8.3 性能测试` 和 `8.4 验收清单` 编号正确。
- [ ] 删除最终正文中所有“超时不返回 `SERVFAIL`”“只清理状态不反馈客户端”的旧结论。
- [ ] 将“透明中继”相关描述改为“主动反馈失败原因 + 迟到响应丢弃”，避免与当前实现冲突。
- [ ] 确认 `REFUSED`、`SERVFAIL`、`NXDOMAIN` 三类错误响应的语义区分清楚。
- [ ] 确认测试结果口径包含常规 `6/6 Passed` 和启用性能测试后的 `7/7 Passed`。
- [ ] 检查性能测试表格中的 QPS、平均延迟、P95、P99、上游请求数和 `report_assets/perf_results_2026-06-14.md` 一致。
- [ ] 检查 Word 中新增表格没有溢出页面，图片引用、页码和目录页码已刷新。
- [ ] 修改 DOCX 后按 docx 技能流程进行渲染检查，再导出最终 PDF。
