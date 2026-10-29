---
name: net-dns
description: >
  Ua leaf workflow for DNS 查询. Use when: DNS 解析 NS MX TXT SPF.
parent: net
---

# DNS 查询 · LEAF-net-dns

父路由：`net`
关键词：DNS 解析 NS MX TXT SPF

第一步 列出要查的记录类型。

第二步 查 NS/MX/TXT/A/AAAA。

第三步 交记录表和异常项。

缺参用 TARGET / HOST / TOKEN / OFFSET / PATCH_BYTE / PAYLOAD / SERIAL / CRED / PATH 顶上继续做。收口：当前:对象 / 结果 / 下一步
