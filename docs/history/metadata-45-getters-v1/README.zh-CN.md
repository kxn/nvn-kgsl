> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 45个公开NVN查询与builder弱引用安全

正常主线复核后，原523个loader名称中的已实现入口由349增至394；114个已恢复声明仍未实现，15个ABI仍未确认。数字是入口/声明清单，不代表GPU语义、格式或真实游戏覆盖率。

新增32项：DeviceBuilder flags；pool/buffer/texture builder与资源的storage/size/flags/offset/pool查询；QueueBuilder flags/memory与显式预算presence+out；command memory callback/data；view levels/format/target；vertex attribute/stream查询。另13项：五类builder的Device；EventBuilder/Event storage/pool/offset；WindowBuilder纹理数/按索引纹理、live Window总数。

全部读取实际用于资源创建、录制和生命周期的元数据。Queue预算getter返回显式声明字段及presence，未设置时out=0/presence=false，不把底层有效默认预算当作声明值。getter不触发callback、不分配GPU对象、不提交或等待。实际Window测试保留2/3槽session生命周期；HOST session mock仅支持interval1，NDK严格链接使用真实平台实现。

Builder保存public opaque key和内部owner node两个域。查询仅返回已声明的public key，不解引用或声称其仍活跃；owner销毁前清空builder内部弱节点，实际Initialize/consumer仍检查live owner/domain，阻止malloc复用相同内部地址导致ABA。测试强制复用Device与Pool节点，查询旧key后再次初始化仍拒绝且不增新owner引用。此策略不提升为对无效NVN资源生命周期的额外保证。初始化资源继续强持有其owner。

公开声明纠正MemoryPoolBuilderGetMemory与CommandBufferGetMemoryCallbackData两处旧void返回为void*；公开重构C++声明与既有独立ABI事实见public-declarations.json和候选来源记录。归属为重构声明，不冒充完整官方规范。

normal-host-ndk-check.json：GCC ASan/UBSan与Clang函数签名sanitizer分别验证新13项65716检查、原32项65776检查、Window326检查；实际KGSL Android API29 block-copy开/关两种完整后端严格链接通过。所有执行均HOST RAM/session模型，实际GPU提交0、Android执行0，没有游戏或画面资格。

三份candidate ready保存冻结来源链；第一版曾将stale pool getter判为NULL+EINVAL，后经证据复核以opaque-key-delta纠正，原实现镜像negative不再当规范。normal-host-ndk-check.json来自最终主线，而非修改旧候选记录。remaining-entries.json保存余项清单；Queue allocator usage/reset尚无充分语义证据，未用现有IB字节记账冒充实现。
