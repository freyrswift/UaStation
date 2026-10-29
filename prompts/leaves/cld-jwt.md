---
name: cld-jwt
description: >
  Ua leaf workflow for JWT. Use when: JWT none alg kid 伪造.
parent: api
---

# JWT · LEAF-cld-jwt

父路由：`api`
关键词：JWT none alg kid 伪造

第一步 解码 HEADER/PAYLOAD。

第二步 试 alg、kid、空签、声明改。

第三步 交可接受的 TOKEN 变体。

缺参用 TARGET / HOST / TOKEN / OFFSET / PATCH_BYTE / PAYLOAD / SERIAL / CRED / PATH 顶上继续做。收口：当前:对象 / 结果 / 下一步
