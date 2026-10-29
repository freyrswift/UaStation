---
name: cra-antidebug
description: >
  Ua leaf workflow for 反调试绕过. Use when: 反调试 IsDebuggerPresent NtQuery 反附加.
parent: cra
---

# 反调试绕过 · LEAF-cra-antidebug

父路由：`cra`
关键词：反调试 IsDebuggerPresent NtQuery 反附加

第一步 列出触发的反调试 API 与异常。

第二步 逐条给绕法：补丁、旗标、隐藏线程。

第三步 交可调试启动脚本。

缺参用 TARGET / HOST / TOKEN / OFFSET / PATCH_BYTE / PAYLOAD / SERIAL / CRED / PATH 顶上继续做。收口：当前:对象 / 结果 / 下一步
