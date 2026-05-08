# 用nslookup验证dnsrelay的步骤

(1) 将 DNS Server 设为本机
(2) 运行 dnsrelay 前本机无法解析域名
(3) 运行 dnsrelay
(4) dnsrelay 可中继解析域名 baidu.com
(5) dnsrelay 可本地解析域名 bupt
(6) 退出 nslookup 命令
(7) 结束 dnsrelay