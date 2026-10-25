> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Source 存储格式族：37 → 69，HOST 与 NDK 资格

实际原始 D2R v15 已越过 RGBA32F BUFFER 创建/注册，下一处为 R11G11B10F63、IMAGE|COMPRESSIBLE12、2880×1620 存储查询。本批补齐 R/RG 普通颜色、8 种 packed 颜色和 6 种深度/模板的 source 存储元数据，共 69 种；不改变 immutable no-NVIDIA-compression/Generic16Bx2 政策，不放开未资格 GPU 消费。

格式字节数复用已有独立 metadata helper，颜色路径排除 RGB8/16/32 等未资格 source 形状。原 SDK53.314 原始 NSO 与三段 SHA 全核验后，以既有 dm/tools/nso.py 解包，在此前确认的格式表读各记录及 RawStorageClass 静态表。记录 +0x10 为 storage bytes，+0xe/+0xf 是 depth/stencil bits，不能当 block 尺寸。69 项记录 SHA 与字段保留于 original-sdk-format-facts.json；没有归档原字节/代码/ASM。深度格式的 RawStorageClass 分别为3/3/4/2/4/5，按原表实现，不能统填颜色2。

正常几何检查每 sanitizer 780,353 个地址/布局检查及 8,198 个原转换编码检查通过；公共资源检查每 sanitizer 17,962 个检查，含 v15 的实际尺寸/flags、所有新增格式、原 BO/offset/别名、注册快照、pool eligibility、OOM 和未资格消费者拒绝。原资源49,453、清除32,848、边界197,171 检查同时通过，真实 Android ARM64 KGSL --no-undefined 链接通过。未运行新增格式的 GPU 或真实游戏，不能据此声称 packed HDR 或 depth 渲染已支持。

旧 target_clear fixture 的 refs2/1 在原 a389871 源码也失败：已有 Call 状态快照增加了对象引用。独立诊断实际为3/2；fixture 保留精确计数、重新绑定不能改变旧 clear 地址及 discard 后 refs0，只将当前真实 owner 纳入断言，未改生产生命周期来迎合测试。baseline 失败证据单独保留。
