# 使用与分发说明

Ua 官方免费提供下载。本项目当前新增的 Ua 自有内容采用
[Ua 非商业源码可见许可证](../LICENSE)，用于个人非商业学习、研究、参考和使用。
公开源码不等于授予任意商用权；免费取得、标注来源或修改名称也不构成商用授权。

> 声明：严禁利用本工具从事本许可证未授权的商业牟利，或恶意破坏、干扰他人程序、系统及数据。
> 一经发现，相关权利人将保留证据，在各自权利范围内依法追究责任，包括要求停止侵害、
> 向平台投诉、依法请求赔偿，或向有关机关反映涉嫌违法行为。具体责任依法认定。

## 允许的使用

- 个人阅读源码、学习实现、研究配置管理和技能加载机制。
- 在自己的设备和个人非商业项目中运行、编译、调试与修改。
- 不收费地交流学习心得，引用时注明来源；引用内容仍按其原许可证处理。
- 免费分享适用本许可证的原版或修改版，同时保留来源、许可证、版权与第三方声明。
- 发布修改后的二进制时，一并提供对应修改源码，明确修改者和改动内容。

## 明确禁止的使用

以下限制适用于当前许可证覆盖的 Ua 自有内容，包含源码、二进制、提示词、文档及其修改版：

- 倒卖、出售、出租、收费下载、付费解锁、收取激活码费用。
- 收费代装、部署、维护、技术支持、咨询或为客户提供有偿服务。
- 作为会员、订阅、收费课程、训练营、付费群或商业整合包的内容、附件或赠品。
- 接入商业网站、SaaS、企业内部生产流程、商业自动化、付费机器人或客户交付项目。
- 通过广告、推广返佣、购买其他商品、强制打赏或其他间接收费方式提供访问。
- 去掉作者、来源、许可证或第三方版权声明后再次发布，或以换名、换图标、少量修改规避限制。
- 冒充官方、宣称独家授权、把免费版本包装成官方收费版，或暗示维护者为第三方产品背书。
- 恶意破坏、干扰他人的程序、服务、系统或数据，或以本项目名义为侵害行为背书。

是否收费不是唯一判断标准：用于公司业务、受薪工作或为商业服务创造价值，也属于商业用途。
在公开仓库中 Fork、标注“仅供学习”或将软件写成“免费赠送”，不会自动改变实际用途。
确有超出个人非商业范围的需求时，须事先取得相关权利人的明确书面许可；本仓库不默认提供此类许可。

## 免费分享需要保留什么

1. 保留原始项目地址、当前适用许可证和完整版权声明。
2. 原样附带适用的第三方许可证，保留各组件自己的授权边界。
3. 明确是官方原版、个人构建还是修改版，不使用容易混淆的官方身份描述。
4. 说明修改内容；修改版二进制附对应源码，不把第三方服务账号或密钥打包进去。
5. 不添加隐蔽联网、遥测、捆绑安装或额外收费，不误导下载者。

官方来源为 [freyrswift/UaStation](https://github.com/freyrswift/UaStation)。
遇到冒充收费版，可保存页面、订单与沟通记录，通过对应平台申诉，并在仓库反馈可公开的证据；
提交时遮盖个人信息、订单隐私、账号与密钥。项目维护者仅能就自身权利采取措施；
其他软件或数据的损害由相关权利人及有权机关依法处理，发布说明不等于预先认定违法或犯罪。

## 旧版与第三方的边界

本次调整不撤销早先已授予的 MIT 等许可。旧版本及已按 MIT 授出的代码部分仍依其原条款使用；
新版说明不对旧授权追溯增加限制。旧 Ua MIT 文本保留在
[历史授权文本](../licenses/ua-prior-MIT.txt)；实际适用范围以取得的版本和原版权记录为准。

Qt、Fluent 图标字体、REA、reverse-skill 及嵌套第三方项目保持各自许可证。
本项目的非商业条款不改写第三方授权，也不限制相关许可证另行授予的商业使用、修改、调试、
重新链接或库替换权利。对这些独立组件，以其自身条款为准；详见
[第三方声明](THIRD_PARTY_NOTICES.md)。

产品名称与 Logo 属于各自权利人，适配卡片不代表合作、授权销售或官方背书。
本软件不出售模型额度、会员、账号或 API 服务，不替代第三方订阅和服务条款。
AI 输出的权利与使用条件还取决于其来源，本项目不会替用户取得他人作品的权利。

## 使用前应了解

- 安装和卸载会修改选定工具的本地配置；先确认路径，保留重要配置备份。
- 安装成功和 `Ua` 回复只验证对应文件或提示词链路，不等于全部技能已被模型调用。
- 原生技能与 MCP 都依赖客户端能力、插件设置和实际运行环境；不承诺所有模型或插件一致。
- MCP 只读技能文件，不执行随附脚本；运行脚本仍由使用者自行检查并决定。
- 不上传密钥、令牌、私钥、账号资料、个人隐私或商业秘密。Issue 和截图也应脱敏。
- 本项目不授权访问第三方系统或数据，不承诺 AI 输出正确，也不承诺规避平台规则或获得特定结果。
- Windows 更新会替换本地程序；运行中的 MCP 服务占用文件时，应先在客户端停止服务再重试。
- 不具备独立签名或公证的构建应如实标明；平台安装提示、适配范围和已知限制见构建文档。

## English Summary

Newly licensed Ua-owned material is available for personal, non-commercial
learning, research, reference and use. Sale, resale, paid installation or
support, commercial deployment, business operations, paid bundles, subscriptions,
advertising-funded access, and indirect payment gates are outside that grant.
Free sharing must retain attribution, license texts and notices; modified binary
distributions must include corresponding modifications in source form and be
clearly marked as unofficial.

Prohibited commercial exploitation and malicious interference with other
people's programs, systems or data may result in evidence preservation and
legal action by relevant rights holders. Responsibility and remedies are
determined under applicable law; no criminal outcome is presumed.

Earlier MIT grants and third-party licenses remain unchanged. This is a
source-available project under a non-commercial license, not an unrestricted
open-source license. The complete English terms are in [LICENSE](../LICENSE).
