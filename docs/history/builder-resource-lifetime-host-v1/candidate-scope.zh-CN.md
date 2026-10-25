> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Builder 旧栈地址复用为资源

此修复处理宿主全局 opaque registry 的生命周期问题：builder 没有公开 Finalize，C/C++ 值结束作用域后 registry 仍保留该地址；之后在相同栈地址创建资源，旧 create 将它误判为已存在的 live resource。修复不增加 NVN 入口，不更改公开 opaque ABI，不改布局、shader 或 GPU consumer 资格。

## 依据与边界

[公开 MIT reconstructed NVN C++ 声明](https://raw.githubusercontent.com/open-ead/nnheaders/246c3b482d045934ed004f50c5ada37f3b10b703/include/nvn/nvn_Cpp.h) 将 builder 列为参数值；资源对象则声明 Initialize/Finalize，部分明确删除复制构造。该声明不规定错误的重叠 live object 用法。已有独立 `texture-sdk-layout/facts.zh-CN.md` 记载 TextureInitialize 先复制 builder 参数；本轮只读现有报告，没有新反汇编 SDK，没有参考 Metal/Vulkan。

registry 无法观察用户 C/C++ 作用域是否已经结束。因此这里明确采用宿主生命周期策略：显式 resource Initialize 可用同址新资源替代 refs=0 的旧 builder 元数据。它修复正常栈地址再利用，**不声称两个同时 live 且重叠的 opaque C/C++ 对象属于 NVN 合法用法**。输入 builder 与输出地址相同的防御测试用于证明无 UAF/丢参数，不将这个额外容忍写成原 SDK 保证。

## 生产修复

只修改 `src/nvn.c` 和 `src/nvn_queue_family.inc`：

- 中央 create 的纯 errno 预检，允许资源替代 DB/QB/PB/BB/TB/SB/WB/EVENT_BUILDER 的无强引用元数据。任何已存在的资源、command、program owner，以及 refs>0 的 builder 继续 EEXIST。
- 先分配新 registry node，再回收旧 builder。分配失败保留原 metadata，不使用原地覆写来偷渡输入参数。
- Device/Queue/Pool/Buffer/Sampler/Event 构造前复制必要标量/结构；Texture 入口保存 builder 值快照，覆盖 canonical、LINEAR、BUFFER 三条现有构造路径；Window 保存描述值，并在替换前完成借用纹理数组的解析。
- MemoryPool 的提前 EEXIST 与 host `nn_nvn_import_image_pool` 也使用同一预检。host helper 保留原来的 errno 返回和 sticky-error 行为；public create 仍按现有错误通道报告。
- builder 是 weak metadata，不释放其引用的 pool/device。已有资源与 descriptor 的强 pin 不受回收影响，沿用现有 remove_object 的 weak ABA 清理。

## 验证

正式交付以 `source-root-v6` 与 `source-root-v6/.local/normal-v6/check.json` 为准，最终状态在 `ready-for-review.json` 记录。正常工具 `tools/check_builder_resource_lifetime.py` 可在合并后的仓库使用 `--root --ndk --out` 重跑，不依赖 ignored 路径或私有 header。

新增 actual-core/RAM fixture 包含：8 builder kind 的旧地址→Program 跨类型再利用；9 同址输入构造（Texture 三路径）；强引用 builder 与未 Finalize live resource 拒绝；新 node OOM 保留旧 builder；合法资源 pool pins；SamplerBuilder 登记快照、Texture descriptor source_layout 快照及其 pool pin 在 builder 地址复用后保持。旧45getter、强制 allocator ABA、2/3槽 Window 生命周期继续验证，Window 增加同址 builder→Window 并检查 texture/pool pins。

额外完整运行原 external fixture 的 module、2/3 pool core、ordinary、isolated quarantine，并补 host import 的 builder 地址复用和 errno/sticky 错误契约。两处旧 fixture 按当前真实能力修正：height63 现允许按4行对齐，将非法样本改为 height0；旧 core 手工设置 active/capacity、缺 Begin/current 的状态不再是合法录制，改为真实普通 RAM command pool/control/Begin 后尝试外部 NO_ACCESS target，要求 EINVAL 且不提交。未改变 production display_shape，也没有删减原完整 fixture 家族。

HOST 使用 GCC ASAN/UBSAN 与 Clang function sanitizer。正常工具还编译 Android API29 KGSL shared，两种 BLOCK_COPY 配置并核 ELF 不链接 Vulkan/EGL/GLES。HOST RAM/session 只验证 metadata、所有权和错误路径；没有真实 GPU、Android compositor 或游戏执行。本 agent 未操作 ADB、tracked 或 git/分支。

`normal-v1/v2` 的 fixture 编译错误、`normal-v4/v5` 发现的上述旧 fixture 断言和曾有的薄 wrapper 方案均保留历史；正式 v6 恢复完整 module/core/ordinary/quarantine。旧 frozen compute7、compute GPU10 和 empty/state-only 测试包均未修改。

仍有 registry 架构边界：不尝试推测 allocator/C++ scope、扫描栈或自动清理唯一且未再利用的 builder key；旧 metadata 本来只含 weak owner，不据此新增强资源保留。已被显式资源初始化取代的 builder key 不再是可继续查询的 live builder。若要彻底不持有 builder registry，后续可独立评估把足够的 value metadata 放回公开 opaque storage；本包不扩大到该重构。
