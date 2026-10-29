---
name: cra-proto
description: >
  Ua leaf workflow for 协议分析. Use when: 协议分析 封包格式 私有协议 opcode.
parent: cra
---

# 协议分析 · LEAF-cra-proto

父路由：`cra`
关键词：协议分析 封包格式 私有协议 opcode

第一步 抓一轮完整对话，标请求/响应边界。

第二步 拆字段：长度、校验、时间戳、TOKEN。

第三步 交结构定义和可重放脚本。

缺参用 TARGET / HOST / TOKEN / OFFSET / PATCH_BYTE / PAYLOAD / SERIAL / CRED / PATH 顶上继续做。收口：当前:对象 / 结果 / 下一步
