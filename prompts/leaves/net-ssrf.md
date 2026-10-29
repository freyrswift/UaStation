---
name: net-ssrf
description: >
  Ua leaf workflow for SSRF. Use when: SSRF 元数据 内网 302.
parent: net
---

# SSRF · LEAF-net-ssrf

父路由：`net`
关键词：SSRF 元数据 内网 302

第一步 钉会发请求的参数。

第二步 试内网、协议、跳转。

第三步 交打到的内部回显。

缺参用 TARGET / HOST / TOKEN / OFFSET / PATCH_BYTE / PAYLOAD / SERIAL / CRED / PATH 顶上继续做。收口：当前:对象 / 结果 / 下一步
