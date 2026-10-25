> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 提交时资源快照与三类 canonical 复制

本批在独立 Adreno/KGSL 分支实现公开 NVN 的有限行为子集。依据是公开声明、独立硬件/布局资料及本游戏原 SDK53.314 静态证据；不把原 Metal 实现作为契约或内部架构。公开资料的版本、性质与限制见相邻 `nvn-behavior-public-audit-v1`、`sdk-copy-semantics-v1`。

## 本批行为

- 通用 Draw 录制保留源 Program 身份、API 绑定和 CPU 元数据生命周期。每次 enqueue 对实际 descriptor 槽做一次不可变深快照，查询、specialization 和最终 native 资源消费使用同一版本；不同重放允许观察新注册的 descriptor。整个 batch 在 GPU 分配/cache/submit 前预检。未知 shader/resource 查询仍拒绝，没有从 archive 期望值补答案。
- 内存补充回调前先强持有 Program、target 和参数池，成功后转交 command handle；失败完整回滚。原漏洞已由实际公共回调在 ASan 下重现。归档的 baseline 是预期失败，修复后的 normal v3 才是通过证据。
- canonical TextureToTexture 使用两次正常原始块 GPU kernel 和一个私有 tight-linear 中间区；CPU 只准备命令/代码/参数，不映射源、中间或目标像素。保留原公开 pool、地址和 GOB 数据。两次正常 IB 间沿用 cache flush/wait/invalidate。公开 CopyCommands、重放、源/目标 pins 和失败 owner 同时覆盖。
- 支持范围是 flags0、同原格式、等大正区域、2D/array GOB、已核37种原始块格式。与既有 B2T/T2B 共享 region/stride/layout 验证。缩放、格式转换、非零 flags 和 LINEAR 混合仍未支持；这是后端限制，不是 NVN 定义。物理范围别名依据真实 backing 校验，不能把不同 BO 当成不重叠证明。
- GetCopyRowStride/GetCopyImageStride 使用公开 ABI，并与原 SDK 的 W0 零扩展相符；值保存原 low32，Begin/End 保留 metadata。入口数现346，不能据此推导完整行为覆盖率。
- KGSL ALLOC ioctl 成功后，metadata/mmap 被拒而 cleanup free 失败的 owner 继续隔离持有，保留原 allocated free 路径。此修复不宣称覆盖 ALLOC ioctl 本身失败但内核状态不明的情况。

## 正常工具与验证

归档均来自 root 最终生产源码的正常工具，没有修改构建记录来绕过校验：

- enqueue v3：55项 stage metadata 检查；两 sanitizer 各包含 color/C1、descriptor 修改、每次提交快照、预检失败及 callback 生命周期。callback 各3057项/15场景；实际 KGSL/Android cache 开关0/1严格链接。没有 GPU 执行。
- canonical 三族：两 sanitizer 各68584项、151次非执行 mock Submit、38个 T2T 场景，含37格式与同 BO 不相交范围；真实 API29 backend 严格链接。没有 GPU 执行。
- allocator：两 sanitizer 各256项/16分配场景，既有 import regression 各4297项/37场景通过；真实 API29 链接。没有 Android/GPU 执行。
- public 三族探针：两 sanitizer 各3301项/234公开录制场景，468次非执行 mock Submit；40项独立字节 oracle/传输/CLI 负例。真实完整 API29 backend 和 Activity 严格链接。此处 HOST 成功不能代替真机资格。
- production shader archive 通过正常 inventory 合并、原 Maxwell 前端和 IR3 编译/打包/verify：12 stages、16 native pairs，保留完整源 code/control/C1 身份和查询要求。明确实际1:1像素映射 policy；未增加运行时编译器，不把 SPIR-V 当 Vulkan renderer。

公共三族的实际 GPU 结果将单独归档；本目录 HOST 记录不宣称 D2R 上传、Draw 或出图。此批完成前最近真实游戏证据仍为 v7：原 ARM64 模块运行至 BC1 CopyBufferToTexture errno95，没有游戏 Draw/Present/frame。
