---
name: net-priv
description: >
  Ua leaf workflow for 权限测试. Use when: 越权 水平 垂直 IDOR.
parent: net
---

# 权限测试 · LEAF-net-priv

父路由：`net`
关键词：越权 水平 垂直 IDOR

第一步 钉两个角色 TOKEN。

第二步 换 id、换对象重放。

第三步 交越权对照表。

缺参用 TARGET / HOST / TOKEN / OFFSET / PATCH_BYTE / PAYLOAD / SERIAL / CRED / PATH 顶上继续做。收口：当前:对象 / 结果 / 下一步
