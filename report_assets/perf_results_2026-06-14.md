# DNS 中继服务器性能测试记录

测试时间：2026-06-14 19:36:22 CST +0800

测试环境：

| 项目 | 数据 |
|---|---|
| 机器 | Apple M5 Pro |
| 逻辑 CPU | 15 |
| 内存 | 51539607552 bytes |
| 系统 | Darwin 25.5.0 arm64 |
| macOS 版本 | 26.5.1 |

构建与校验：

```bash
cmake -S . -B build -DDNSRELAY_DEV_PORT=8053 -DDNSRELAY_BUILD_PERF_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
ctest --test-dir build -L perf --output-on-failure
```

校验结果：

| 检查项 | 结果 |
|---|---|
| 全量自动化测试 | 7/7 Passed |
| CTest 性能冒烟测试 | 1/1 Passed |

测试方法：

使用 `tests/perf_dnsrelay.py` 在 `127.0.0.1` UDP loopback 上启动 `dnsrelay` 和本地假上游 DNS。每组测试分别覆盖本地表命中、Cache 命中和上游中继未命中。`local_table_hit` 与 `cache_hit` 每组 3000 次请求，`relay_miss` 每组 1000 次请求，并发客户端数分别为 1、4、8。

原始机器可读结果保存在 `report_assets/perf_results_2026-06-14.json`，单组原始输出保存在 `report_assets/perf_runs/clients_1.json`、`report_assets/perf_runs/clients_4.json`、`report_assets/perf_runs/clients_8.json`。

## 测试数据

| 并发客户端 | 场景 | 请求数 | QPS | 平均延迟(ms) | P50(ms) | P95(ms) | P99(ms) | 最大延迟(ms) | 上游请求数 |
|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | local_table_hit | 3000 | 10390.5 | 0.094 | 0.087 | 0.130 | 0.186 | 1.162 | 0 |
| 1 | cache_hit | 3000 | 10949.0 | 0.090 | 0.087 | 0.110 | 0.130 | 0.194 | 0 |
| 1 | relay_miss | 1000 | 5129.6 | 0.193 | 0.177 | 0.250 | 0.618 | 1.389 | 1000 |
| 4 | local_table_hit | 3000 | 13870.8 | 0.283 | 0.271 | 0.364 | 0.443 | 0.783 | 0 |
| 4 | cache_hit | 3000 | 14655.4 | 0.271 | 0.262 | 0.330 | 0.409 | 0.525 | 0 |
| 4 | relay_miss | 1000 | 7258.3 | 0.548 | 0.536 | 0.630 | 0.667 | 0.803 | 1000 |
| 8 | local_table_hit | 3000 | 13808.6 | 0.569 | 0.548 | 0.736 | 0.921 | 1.230 | 0 |
| 8 | cache_hit | 3000 | 14684.1 | 0.541 | 0.533 | 0.621 | 0.664 | 0.739 | 0 |
| 8 | relay_miss | 1000 | 7247.1 | 1.096 | 1.099 | 1.208 | 1.262 | 1.282 | 1000 |

## 结论

在本机 loopback 和本地假上游环境下，本地表命中与 Cache 命中路径都不产生上游请求，说明本地解析和缓存拦截逻辑生效。并发客户端为 4 或 8 时，本地表命中吞吐约 13.8k QPS，Cache 命中约 14.7k QPS；上游中继未命中路径需要经过假上游转发，吞吐约 7.25k QPS。

延迟方面，8 并发下 `local_table_hit` 的 P95 为 0.736 ms，`cache_hit` 的 P95 为 0.621 ms，`relay_miss` 的 P95 为 1.208 ms。三类路径均保持毫秒级以内或接近毫秒级的尾延迟，满足课程设计中本地 DNS 中继服务器的性能验证需求。
