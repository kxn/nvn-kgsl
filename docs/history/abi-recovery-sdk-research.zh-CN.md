> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# D2R SDK ABI recovery batch

仅ignored候选；不修改tracked/HEAD、不使用ADB、不执行原模块/GPU、无Metal/Vulkan源码依据。批量恢复**调用约定**，不增加backend实现或GPU资格。

## 输入与结果

- 原SDK NSO SHA256 `1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017`，实际SDK53.314；`research.py`重新校验段hash、MOD0/DYNAMIC/RELA、523名dictionary，保存每个相关callee的offset/hash/指令。
- 原main SHA256 `55fd909bb7230b002357e5b0d8499c11f06888f345c49e69eb4734df44a27d0e`；现有canonical batch2 names-record有478 public名字、45 unknown。本轮candidate使用原extractor正常从该原main重新生成和重复`--verify`。
- **27个getter的ABI补入header；3个已知声明的opaque-by-value返回错误改为pointer；18个仍unknown**。新生成names为505 public、504 typed forwards +1 adapter resolver、18 unknown；backend ENTRY数量不变。
- `main-static-evidence.json`记录原slots元数据：45 unknown及3个错误aggregate getter的静态槽读计数均0。因此本次**没有main调用者寄存器消费证据**，不能编造main callsite。零仅代表原工具的静态ADRP/slot-read集合，不能证明动态永不调用。

## 已确定的3个返回声明错误

|方法|SDK callee|公开独立声明|候选修复|
|---|---|---|---|
|BufferBuilderGetMemoryPool|45cb70: 读取对象内保存的内存池指针并返回|BufferBuilder::GetMemoryPool→MemoryPool*|NVNmemoryPool→NVNmemoryPool*|
|TextureBuilderGetMemoryPool|475520: 同上|TextureBuilder::GetMemoryPool→MemoryPool*|同上|
|TextureGetMemoryPool|4773f0: 同上|Texture::GetMemoryPool→MemoryPool*|同上|

原callee加载一个真实pointer到X0返回；没有向X8提供的隐藏结果区写opaque对象。原header把NVNmemoryPool（256B opaque）按值返回，会使AAPCS64 wrapper使用hidden-X8 sret，ABI实质错误；不能因它来自公开重建头便照抄。

独立public声明：[BufferBuilder](https://smo.monsterdruide.one/docs/classnvn_1_1BufferBuilder.html)、[TextureBuilder](https://smo.monsterdruide.one/docs/classnvn_1_1TextureBuilder.html)、[Texture](https://smo.monsterdruide.one/docs/classnvn_1_1Texture.html)。这几页用于类型/签名，不作行为规范。其版本不等同本gameSDK；callee的实际X0访问与公开返回类型独立吻合。

## 27个恢复项及证据分层

完整PFN和参数在`recovered.json`，逐callee在`static.json`。下列是**有效C ABI的重建类型**，不是声称取得了原官方C头。

|家族|恢复项|实际SDK证据|
|---|---|---|
|DeviceBuilder|GetFlags|465e60从+4加载W0；独立public DeviceFlagBits→32位enum|
|QueueBuilder|GetDevice、GetFlags、GetMemorySize、GetMemory|471570/80/d0/e0，依次X0 pointer、W0 flags、W0 int、X0 memory pointer|
|QueueBuilder|GetCommand/Compute/ControlMemorySize、GetCommandFlushThreshold|471590/a0/b0/c0：W1是out int*，无条件写32位值，W0是presence byte；**不是size_t无out参数getter**|
|QueueBuilder|GetQueuePriority|471620 W0读+38；SetDefaults4711a0写100，setter4715f0存W1；public enum LOW10/NORMAL100/HIGH1000/LARGE-1。仅补32位enum getter，不补setter|
|WindowBuilder|GetDevice/GetNumTextures/GetTexture|477650/60/70：X0 device、W0 int；Texture按W1有符号index索引caller pointer数组并X0返回，匹配公开constTexture*|
|WindowBuilder/Window|GetNumActiveTextures；WindowGetNumTextures|4776a0 W0读builder+14；477c60通过platform out32读W0返回；477c50 W0读liveWindow+8。当前旧public wrapper无这几个新增方法，**类型由本SDK count字段/32位标量访问确定，不伪称存在该公开声明**|
|MemoryPool/Buffer/Texture/SamplerBuilder|各GetDevice|46cd20、45cb60、4753e0从+8加载X0；Sampler473490从+0加载X0；四类公开wrapper返回constDevice*|
|TextureBuilder|GetPackagedTextureLayout|4754c0 X0 builder、X1 out layout，写out首32位，W0返回presence低nibble；公开wrapper为NVNboolean(PackagedTextureLayout*)。不是layout本身或void-return|
|TextureBuilder|GetRawStorageClass|4757c0：与已恢复TextureGetRawStorageClass相同flags&50分支/format分类，W0 enum；RawStorageClass公开独立enum来源已在前批固定。无aggregate/out参数|
|EventBuilder|GetStorage|46bd90 X0 builder、X1 int64_t*out；写offset64，X0返回pool；公开wrapper为constMemoryPool*(int64_t*)|
|EventBuilder/Event|GetMemoryPool/GetMemoryOffset|46bda0/b0、46be70/80返回同一已确认pool pointer/offset字段。Pool类型和const限定沿已公开GetStorage/SetStorage合同；offset沿其明确int64_t。当前public旧Event类未列这些拆分getter，故不是完整公开C PFN直接摘录|

公开补充类型页：[DeviceBuilder](https://smo.monsterdruide.one/docs/classnvn_1_1DeviceBuilder.html)、[QueueBuilder](https://smo.monsterdruide.one/docs/classnvn_1_1QueueBuilder.html)、[WindowBuilder](https://smo.monsterdruide.one/docs/classnvn_1_1WindowBuilder.html)、[MemoryPoolBuilder](https://smo.monsterdruide.one/docs/classnvn_1_1MemoryPoolBuilder.html)、[SamplerBuilder](https://smo.monsterdruide.one/docs/classnvn_1_1SamplerBuilder.html)、[EventBuilder](https://smo.monsterdruide.one/docs/classnvn_1_1EventBuilder.html)、[QueuePriority](https://smo.monsterdruide.one/docs/structnvn_1_1QueuePriority.html)。C++ builder setter返回引用不决定原C setter返回型，因此不借相似wrapper补void原型。

指针const限定和int/int64别名不改变AAPCS64寄存器宽度；这些声明以对应public对象/字段合同选定。没有把未改X0当pointer-return：所有新getter均有确定的load/生成X0/W0，或确定platform-out32→W0。未访问X8的指针返回与opaque-by-value不同。

## 明确保留的18个unknown

1. QueueBuilderSetQueuePriority：仅保存输入标量、返回时未重写X0，缺直接return声明，不能区分void/pointer返回。
2. WindowBuilderSetNumActiveTextures、WindowSetNumActiveTextures、ProgramSetSampleShading、CommandBufferSetCommandMemoryCallbackEnabled：分别pure store或tailcall，独立public旧wrapper缺该方法，返回合同仍不补猜测。
3. ProgramSetShadersExt：确有24B shader records并传内部函数，但第三字段类型/合同未有独立声明；不能把struct布局猜成普通ShaderData。
4. SetColorReductionEnable/Thresholds、SetStencilCullCriteria：观察到真实GPU packet生成，不能从名称填参数enum或no-op；继续unknown。
5. SyncInitializeFromFencedGLSync/CreateGLSync：有GL/platform路径，具体GL对象参数/结果enum等需更完整证据，不能以通用void*替代。
6. InsertCheckpointStatic/Dynamic及5个Device checkpoint setters：本SDK落到不使用参数并返回整数1，**无法恢复未使用参数**；不能因为它返回1就伪造prototype。

实际剩余名单取candidate names-record；与上表逻辑一致，共18。

## extractor 的薄fail-closed加固

候选仍按更新后的public PFN自动生成typed forward，未写特例lookup或手工patch names缓存。新增校验：若返回类型直接是public header的`reserved[]` opaque NVN对象，拒绝生成并明确报API/类型；真实value aggregate（separate handles等）不受此检查。这样未来漏`*`不能再次静默生成hidden-sret wrapper。

`minimal-promote.patch`只修改`third_party/nvn/nvn.h`及`adapters/d2r/extract_nvn_names.py`。不含core ENTRY、成功stub、GPU工作或设备flags变动。已有canonical records应正常重建，不能手修缓存。

## 验证

- `checks-3/proof.json`：真实原main重提取及重复verify；505/504/18计数；GCC ASan/UBSan、Clang function-signature trap测试30个typed forward（27恢复+3纠正），pointer、enum、false合法返回/out canary、64位offset、unknown仍fatal、missing implementation仍fatal、sticky-before截断。
- Android API29 actual bridge `-Wall -Wextra -Werror`+`--no-undefined` shared link；readelf确认ELF64/AArch64和原HLE_FORCE swforce。未执行该ARM64产物。
- 人为把候选TextureBuilderGetMemoryPool退回aggregate后，真实extractor明确拒绝且不发布names-record，证明不是只靠metadata计数通过。
- `core-ndk.json`：当前tracked实际core搭配候选public头严格NDK syntax通过，core源码无改动。
- HOST mock仅验证ABI和桥规则，**不证明任何新getter已在独立driver实现或实际D2R调用**。原SDK从未执行。checks-1/2是fixture宏/期望字符串问题的失败历史，最终只以checks-3为通过证明。
