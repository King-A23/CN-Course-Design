# 成员B个人计划

> 提交前请将“成员B”替换为实际姓名与学号。

## 1. 角色定位

成员B负责“事件循环 + 中继转发 + 状态表 + 缓存 + 测试落地”主线，必须保证程序在真实网络环境下稳定工作，并支撑验收中的并发与异常场景。

## 2. 需要提前掌握的知识

1. UDP socket 编程：`socket`、`bind`、`recvfrom`、`sendto`。
2. `select` 多路复用与单线程事件循环。
3. 为什么需要客户端 ID 与上游 ID 转换。
4. 超时回收、迟到响应处理和请求状态表。
5. Cache 生命周期、TTL 递减和命中路径。
6. Windows/Linux 构建与运行差异。

## 3. 文件主责

1. `src/config.c`
2. `src/platform_win.c`
3. `src/platform_posix.c`
4. `src/logger.c`
5. `src/pending_map.c`
6. `src/cache.c`
7. `src/server.c`
8. `src/main.c`
9. `tests/test_pending_map.c`

## 4. 分周执行任务

### 第11周

1. 确定 `DrPendingRequest`、`DrPendingMap`、`DrCacheEntry` 的结构。
2. 设计单 socket + `select` 主循环。
3. 确定构建命令与双平台编译选项。

### 第12周

1. 完成 `dr_config_parse`。
2. 完成 `dr_platform_init`、`dr_close_socket`、`dr_set_reuseaddr`。
3. 完成 `dr_server_run` 的基础收包和上游转发路径。
4. 确保未知域名可经上游解析成功。

### 第13周

1. 完成 `dr_pending_map_insert/remove/expire`。
2. 完成 `dr_cache_put/lookup/expire`。
3. 完成上游超时回收与迟到应答丢弃。
4. 完成并发测试与缓存命中测试。

### 第14周

1. 完成 Windows/Linux 双平台构建验证。
2. 整理测试命令、日志和演示步骤。
3. 撰写测试分析、异常处理和运行结果章节。
4. 参加双人答辩彩排。

## 5. 本人完成标准

1. 能解释为什么本题不应使用忙等待或双 socket。
2. 能解释为什么超时时不能由中继伪造重传或假响应。
3. 能说明 `pending_map` 如何避免并发查询串包。
4. 能现场演示本地命中、转发命中、缓存命中和超时场景。
5. 负责模块的构建与测试全部通过。
