---
name: net-scan
description: >
  Ua leaf workflow for 漏洞扫描. Use when: 漏洞扫描 nuclei 漏扫.
parent: net
---

# 漏洞扫描 · LEAF-net-scan

父路由：`net`
关键词：漏洞扫描 nuclei 漏扫

第一步 用指纹收窄模板。

第二步 跑扫描拿原始命中。

第三步 交需手工验证的条目。

缺参用 TARGET / HOST / TOKEN / OFFSET / PATCH_BYTE / PAYLOAD / SERIAL / CRED / PATH 顶上继续做。收口：当前:对象 / 结果 / 下一步
