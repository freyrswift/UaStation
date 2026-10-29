# 叶子索引 · UA-LEAVES

共 80 片。工单先过 `NORMALIZER.md` 的口语归一表，命中就直接按叶子三拍做；不命中退到 `ROUTER.md` 的 6 父路由。路由过程不要念出来，命中就开跑。

## 二进制 / 逆向（17 片） · 父路由 `cra`

| 技能 id | 小类 | 关键词 |
| --- | --- | --- |
| `cra-software` | 软件破解 | 破解 crackme 去掉限制 去验证 unlock |
| `cra-unpack` | 脱壳 | 脱壳 unpack UPX Themida VMProtect 加壳 |
| `cra-keygen` | 注册机 | 注册机 keygen 序列号 SERIAL 激活码 |
| `cra-license` | 授权逻辑 | 授权 license 试用 过期 在线验证 |
| `cra-proto` | 协议分析 | 协议分析 封包格式 私有协议 opcode |
| `cra-dll` | DLL 注入 | DLL 注入 injector LoadLibrary 远程线程 |
| `cra-mem` | 内存修改 | 内存修改 WriteProcessMemory 改数值 金币 |
| `cra-patch` | 补丁制作 | 补丁 patch 改字节 JNZ JMP NOP |
| `cra-antidebug` | 反调试绕过 | 反调试 IsDebuggerPresent NtQuery 反附加 |
| `cra-dotnet` | .NET 逆向 | .NET dnSpy IL 反编译 C# |
| `cra-java` | Java 逆向 | Java JAR jadx 字节码 class |
| `cra-native` | Native 逆向 | Native so dll syscall 反汇编 |
| `cra-pe` | PE 分析 | PE 节表 IAT DOS NT 头 |
| `cra-elf` | ELF 分析 | ELF so 动态链接 GOT PLT |
| `cra-apk` | APK 分析 | APK Android smali jadx 加固 |
| `cra-fw` | 固件分析 | 固件 firmware binrootfs squashfs |
| `cra-bin-else` | 其它二进制 | 二进制 binary 样本 未知格式 |

## 游戏 / 外挂（13 片） · 父路由 `ply`

| 技能 id | 小类 | 关键词 |
| --- | --- | --- |
| `ply-cheat` | 游戏外挂 | 外挂 cheat 辅助 挂 |
| `ply-trainer` | Trainer | Trainer 修改器 无限弹药 金钱 |
| `ply-esp` | ESP | ESP 方框 骨骼 物品描边 |
| `ply-wall` | 透视 | 透视 wallhack 穿墙 看见 |
| `ply-aim` | 自瞄 | 自瞄 aimbot 压枪 锁定 |
| `ply-offset` | 内存与偏移 | 偏移 offset 指针链 基址 |
| `ply-packet` | 封包协议 | 游戏封包 opcode 加密 重放 |
| `ply-unity` | Unity 逆向 | Unity il2cpp Dump 全局表 |
| `ply-unreal` | Unreal 逆向 | Unreal GNames GObjects 蓝图 |
| `ply-il2cpp` | IL2CPP 逆向 | IL2CPP libil2cpp metadata |
| `ply-overlay` | Overlay | Overlay 透明窗 D3D EndScene |
| `ply-mech` | 游戏机制修改 | 机制 冷却 伤害 移速 重力 |
| `ply-ac` | 反作弊分析 | 反作弊 EasyAntiCheat BattlEye 检测点 |

## 网络 / 渗透（15 片） · 父路由 `net`

| 技能 id | 小类 | 关键词 |
| --- | --- | --- |
| `net-asset` | 资产发现 | 资产发现 测绘 暴露面 |
| `net-port` | 端口扫描 | 端口扫描 nmap 开放端口 服务 |
| `net-sub` | 子域名发现 | 子域名 爆破 crt.sh 证书 |
| `net-dns` | DNS 查询 | DNS 解析 NS MX TXT SPF |
| `net-dir` | 目录枚举 | 目录枚举 ffuf dirb 备份文件 |
| `net-fp` | 技术栈指纹 | 指纹 框架 Wappalyzer 中间件 |
| `net-scan` | 漏洞扫描 | 漏洞扫描 nuclei 漏扫 |
| `net-sqli` | SQL 注入 | SQL注入 SQLi union 盲注 sqlmap |
| `net-xss` | XSS | XSS 反射 存储 DOM |
| `net-ssrf` | SSRF | SSRF 元数据 内网 302 |
| `net-priv` | 权限测试 | 越权 水平 垂直 IDOR |
| `net-brute` | 登录爆破 | 爆破 撞库 hydra 登录口 |
| `net-cred` | 凭证测试 | 凭证 默认密码 弱口令 泄露 |
| `net-cve` | CVE 验证 | CVE POC 复现 版本 |
| `net-report` | 渗透报告 | 渗透报告 证据 利用链 |

## 接口 / 云 / 代码（13 片） · 父路由 `api`

| 技能 id | 小类 | 关键词 |
| --- | --- | --- |
| `cld-api` | API 安全 | REST API 接口 未授权 BOLA |
| `cld-gql` | GraphQL | GraphQL introspection 过度取数 |
| `cld-ws` | WebSocket | WebSocket 握手 订阅 推送 |
| `cld-jwt` | JWT | JWT none alg kid 伪造 |
| `cld-oauth` | OAuth | OAuth redirect_uri 授权码 Implicit |
| `cld-docker` | Docker | Docker sock 逃逸 镜像 |
| `cld-k8s` | Kubernetes | K8s RBAC secret 仪表盘 |
| `cld-iam` | 云权限 | 云权限 IAM 元数据 STS |
| `cld-audit` | 源码审计 | 源码审计 sink 污染 代码审查 |
| `cld-deps` | 依赖漏洞 | 依赖漏洞 CVE 锁文件 sbom |
| `cld-supply` | 供应链安全 | 供应链 CI 投毒 构建脚本 |
| `cld-fuzz` | 模糊测试 | 模糊测试 fuzz corpus AFL |
| `cld-biz` | 业务逻辑 | 业务逻辑 支付 状态机 竞态 |

## 样本 / 取证（11 片） · 父路由 `lab`

| 技能 id | 小类 | 关键词 |
| --- | --- | --- |
| `lab-sample` | 恶意样本分析 | 样本分析 malware 木马 行为 |
| `lab-yara` | YARA | YARA 规则 匹配 |
| `lab-ioc` | IOC | IOC 域名 IP 哈希 mutex |
| `lab-memfor` | 内存取证 | 内存取证 volatility 转储 |
| `lab-pcap` | 流量分析 | 流量 pcap C2 DNS 隧道 |
| `lab-log` | 日志溯源 | 日志溯源 登录失败 EVTX |
| `lab-vulnres` | 漏洞研究 | 漏洞研究 补丁对比 根因 |
| `lab-batch` | 批量扫描 | 批量扫描 目录 并发 |
| `lab-harvest` | 数据采集 | 采集 爬虫 签名 断点续爬 |
| `lab-auto` | 自动化工具 | 自动化 流水线 编排 |
| `lab-report` | 报告生成 | 报告生成 取证报告 汇总 |

## 内容 / 剧情（11 片） · 父路由 `ink`

| 技能 id | 小类 | 关键词 |
| --- | --- | --- |
| `ink-tech` | 技术文章 | 技术文章 教程 原理 |
| `ink-docs` | 项目文档 | 项目文档 README 接口文档 |
| `ink-analysis` | 分析报告 | 分析报告 研判 结论 |
| `ink-dialogue` | 脚本对白 | 对白 台词 剧本 |
| `ink-cast` | 角色设定 | 角色设定 人设 外貌 |
| `ink-world` | 世界观 | 世界观 设定集 规则 |
| `ink-thriller` | 悬疑惊悚 | 悬疑 惊悚 恐怖 |
| `ink-dark` | 黑暗题材 | 黑暗 暴力 虐 残酷 |
| `ink-adult` | 成人文学 | 成人 情色 做爱 身体 |
| `ink-seq` | 剧情续写 | 续写 下一章 接着写 |
| `ink-voice` | 文风定制 | 文风 仿写 语气 |
