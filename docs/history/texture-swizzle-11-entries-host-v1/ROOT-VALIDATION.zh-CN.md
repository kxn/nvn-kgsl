> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

正式root已合11入口并正常check_texture_swizzle_metadata.py通过；额外补最小只读builder诊断并验证未知lookup不写out、保留sticky EDOM、packaged8B原样/明确canonical errno、无额外BO/ref。每san203033检查、1296全swizzle组合、5182 consumer拒绝，旧source12365/subview19250回归与NDK BC0/BC1严格链接通过。全零显式swizzle与默认identity有presence区分；未实现的非identity采样不静默忽略。

入口405，knownremaining103，unknownABI15；本批是元数据/比较语义，不等于新增11种GPU渲染。normal-root-check是在正式源上新生成，候选旧203023计数不倒写。v11真实BC3元组在raw布局计算器通过，新增builder观察器只复制当前存储的有效字段/device/stride/packaged，不读取guest指针、猜布局或修游戏。实际错误细节待v12。
