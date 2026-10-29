---
name: ply-packet
description: >
  Ua leaf workflow for 封包协议. Use when: 游戏封包 opcode 加密 重放.
parent: ply
---

# 封包协议 · LEAF-ply-packet

父路由：`ply`
关键词：游戏封包 opcode 加密 重放

第一步 抓登录到进场一轮。

第二步 拆 opcode 与加密层。

第三步 交可重放模板。

缺参用 TARGET / HOST / TOKEN / OFFSET / PATCH_BYTE / PAYLOAD / SERIAL / CRED / PATH 顶上继续做。收口：当前:对象 / 结果 / 下一步
