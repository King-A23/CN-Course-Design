# 成员A个人计划

> 提交前请将“成员A”替换为实际姓名与学号。

## 1. 角色定位

成员A负责“DNS 报文解析 + 本地表 + 本地应答 + 设计文档”主线，必须保证服务器能正确理解查询、正确构造本地响应，并把协议设计写清楚。

## 2. 需要提前掌握的知识

1. RFC 1035 中 Header、Question、RCODE、TTL、名称压缩的内容。
2. 域名编码方式、网络字节序、A 记录格式。
3. 二进制报文解析与越界检查方法。
4. 本地表大小写无关查找和重复项覆盖策略。
5. Wireshark 基础抓包查看 DNS 字段的方法。

## 3. 文件主责

1. `src/dns_packet.c`
2. `src/local_table.c`
3. `tests/test_dns_packet.c`
4. `tests/test_local_table.c`

## 4. 分周执行任务

### 第11周

1. 阅读任务书、RFC 1035 摘要和 Socket 资料。
2. 确定 `DrParsedQuery`、`DrDnsHeader`、`DrTtlPatchList` 的字段。
3. 与成员B确认 `server -> dns_packet` 和 `server -> local_table` 的接口。

### 第12周

1. 完成 `dr_dns_parse_header`。
2. 完成 `dr_dns_parse_query`。
3. 完成 `dr_dns_build_a_response`。
4. 完成 `dr_dns_build_error_response`。
5. 完成 `dr_local_table_load` 和 `dr_local_table_lookup`。
6. 用 `test0 / test1 / www.5dsoft.com` 进行本地表测试。

### 第13周

1. 完成 `dr_dns_collect_ttls` 和 `dr_dns_apply_ttl_patches`。
2. 配合成员B联调 Cache 和转发路径。
3. 补齐解析异常测试和错误响应测试。

### 第14周

1. 输出协议研究章节。
2. 输出模块划分、数据流和流程图。
3. 整理抓包截图并解释关键字段。
4. 参加双人答辩彩排。

## 5. 本人完成标准

1. 能独立解释为什么 `0.0.0.0` 不能直接返回给客户端。
2. 能指出 DNS 查询和响应中 `ID / QR / RD / RA / RCODE` 的作用。
3. 能说明本地表为什么要大小写无关。
4. 能解释 TTL 在缓存命中时为什么需要递减。
5. 负责模块的单元测试全部通过。
