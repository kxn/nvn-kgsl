> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# canonical texture subresource registration candidate

本批无新增 NVN 入口；修复已有 TexturePoolRegisterTexture 对合法 mip/layer subset 的软件登记语义。只写 ignored 候选，无 tracked/ADB/游戏代码改动。

## 行为依据

采用仓库既有 `docs/research/nvn-turnip-study-20261023/texture-view-sdk-research.zh-CN.md`：已独立核实 view 为无指针值语义，未指定字段继承源，count0 为从 base 起剩余范围；公开第三方重建 NVN C PFN 确认 ABI。此次没有对原 SDK 新增反汇编，也没有以 Metal/Vulkan 作为规范或模板。format70/0x46 的公开枚举是 RGBA_DXT1_SRGB（BC1 RGBA sRGB）。

## 实现

原始 resource/pool/offset/size 和完整 canonical source_layout 仍是唯一源 owner。descriptor_entry 另存归一化的 base_level/level_count/base_layer/layer_count 与相对原源的 source_offset；不剪改原 GOB 布局，不将 subset 冒充完整源，也不生成 GPU descriptor。NULL/default/full-range 等价选择保持 subset0；真 subset 独立快照 subset1。

登记对目标 2D、2D_ARRAY、3D 的已有 canonical 资源有效；3D 的逻辑 array layer 总数是1，未实现任意 Z-slice view。当前只实现 same-format/same-target 的 view 子集，这是此后端的实现范围，不表示 NVN 规范禁止合法的格式或目标 reinterpretation。合法 alias format/target 的资格、尺寸/分量解释和采样/存储 descriptor 规则仍需独立实现，当前明确 ENOTSUP；没有臆造跨格式或目标兼容表。

两个现有真实 GPU consumer（legacy gpu_late_draw_access 和 pipeline_capture_domain）在选中 subset 时仍 ENOTSUP，且在 pin、GPU 分配/转换前拒绝；因此登记通过不表示 GPU 能采样该子资源。既有 RT/复制/线性 BUFFER resolver 没有放宽。后续 native sampler 实现需消费独立 selection，并保留原 source owner 和原 mip/layer 地址系，不能直接复用 full-source descriptor。

软件登记替换保持原子性：越界、错误 private view 编码、暂不支持 reinterpret、source clone 分配或缓存发布失败，均保留旧 private entry、source ref 与32B table slot。view 按值解析和复制，caller 后续改写/移动及 texture finalize 不改变已登记快照。

## 验证与范围

正常 checker：`source-root-v3/nvn-native/tools/check_texture_subresource_registration.py`。以真实 normal FDL 记录验证库及源码一致性，不修改它的 provenance；双 HOST sanitizer、原 source_resource 回归、实际 Android29 KGSL 完整 source 严格链接，BC0/BC1 均覆盖。记录最终结果见 check-v3/check.json。

新用例覆盖真实失败的 128×128 array84 mip7 BC1 sRGB / size1204224 / id293 NULL、294 default、295 layer0/count1；此外 RGBA8 多 mip array/2D 与 RGBA16F IMAGE4 真3D 单/多mip。每个 mip/layer 的显式 count1 与 count0剩余、view memcpy/move、原子回滚、owner 引用和 consumer fail-closed 都实际执行。

旧 source_resource 中两条原本把合法 mip/layer subset 当作 ENOTSUP 的断言改为真正越界 EINVAL；其 stale canonical InvalidateTexels 断言改为当前已实现 cache 操作的成功断言。ReadTexels 此旧 fixture 未配置 block-copy kernel，仍明确 ENOTSUP。没有回退现有生产 Texels 能力。

失败历史：v1 只因测试 helper builder 与 core 静态 helper 同名编译失败；v2 新 fixture ASan/UBSan 全过，但旧 source_resource 对已实现 InvalidateTexels 的陈旧 ENOTSUP 断言失败。均完整保留，fresh v3 修正fixture重跑，未改历史证明。

HOST 使用真实核心和 RAM memory backend，未执行 GPU、原游戏或 Android ARM64；NDK 是真实 AArch64 编译链接验证，不能替代真机。当前调用覆盖计数不变，本批不能计为新增完整 GPU NVN 调用。
