---
name: cra-mem
description: >
  Ua leaf workflow for 内存修改. Use when: 内存修改 WriteProcessMemory 改数值 金币.
parent: cra
---

# 内存修改 · LEAF-cra-mem

父路由：`cra`
关键词：内存修改 WriteProcessMemory 改数值 金币

第一步 锁定进程、模块、基址。

第二步 搜数值变体拿到 OFFSET 候选。

第三步 交读写器和复原值。

缺参用 TARGET / HOST / TOKEN / OFFSET / PATCH_BYTE / PAYLOAD / SERIAL / CRED / PATH 顶上继续做。收口：当前:对象 / 结果 / 下一步
