> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# packaged8B 后续来源与 consumer 传播审计（只读，冻结 v1）

本批没有改 tracked、没有 ADB、没有执行 GPU/原游戏模块，没有扩展已提升的实验 gate。原作者工具仅作 HOST 容器元数据读写；未调用 CPU swizzle/deswizzle、BC codec，未读原 SDK CPU 代码、Metal/Vulkan 后端。

结论：**没有新增足以放宽8B编码或 mip/array extent 的独立 NVN 规范证据。现支持域里的显式 BH 在代码检查上完整传播到转换 kernel；mip/array 公共采样依然严格拒绝。** 下一未知 profile 应保留实际八字节/target/format/mips/layers/stride，不把 parser 能读当作 GPU 语义已知。

## 公开容器作者：能确认什么

1. [BntxLibrary / BntxFile.cs](https://github.com/KillzXGaming/BntxLibrary/blob/e5c8d20cb5aed9262b16d7be9e0e48e0f9c43987/BntxLibrary/BntxFile.cs)，MIT，固定 commit。`BlockHeightLog2` 在436–444仅映射 TextureLayout 的低3位；结构630/633给出相邻两 u32，默认第二字0x10007。其 BNTX BinaryHeader 默认容器版本4.0.0，不能当作 NVN API/SDK 版本。现有 profile 只选上位清零的6种字型；作者保留其它位不说明其 NVN 含义。
2. [原 BntxWriter](https://github.com/KillzXGaming/BntxLibrary/blob/e5c8d20cb5aed9262b16d7be9e0e48e0f9c43987/BntxLibrary/BntxWriter.cs) 108以 ImageData.Length 设置 ImageSize，316–332使用调用者的 MipOffsets 写绝对数据指针并写已有 payload。它**没有计算/验证 mip stride、array stride 或 NVN storage extent**；文件 Align 与 texture payload/公开 StorageAlignment 不是同一结论。
3. [原 BntxReader](https://github.com/KillzXGaming/BntxLibrary/blob/e5c8d20cb5aed9262b16d7be9e0e48e0f9c43987/BntxLibrary/BntxReader.cs) 49读实际结构、56读取 MipCount 个数据指针、60按 ImageSize取字节、64–66将 mip 指针相对首指针归一化。这是可以直接复用的元数据 parser，不需要自造 BNTX decoder。

`metadata-reader-proof.json` 是本批实际 HOST 证明：原作者代码不改，Save/Load往返6个BH，width73/height163/array3/mip4，故意非 canonical 的 offsets=[0,513,1025,2049]、ImageSize=4097 也都保留。该实验说明 parser/writer 的容器保存边界，**并不说明这些 offset 可用于 NVN 纹理**。无真实游戏 BNTX 被解析，无原始游戏字节写入报告。

现成工具用法（文件必须为实际 BNTX，不能把任意游戏资产或8B本身当完整容器）：

```
${D2R_REFERENCE_ROOT}/.local/dotnet/dotnet metadata-output/metadata.dll /ignored/path/input.bntx
```

`metadata-harness/Program.cs` 仅调用现成 `BntxFile(filename)` 并序列化公开对象字段；没有实现文件解析、extent 数学或像素转换。不带参数时运行上述 authored metadata 往返。

## 第二套 parser 的证据局限

[ScanMountGoat/bntx](https://github.com/ScanMountGoat/bntx/blob/87fffc7d17238cb07ff93ec75e0a7b5db15f4683/src/lib.rs) 的 `Brti` 有 layer_count、block_height_log2、image_size、align 和 mipmap_offsets（约222–241行）；但第二字被纳入 `unk4[6]`，没有证明0x10007。其 `deswizzled_data` 326行明确给底层工具传 `None` 并留有使用 header BH 的 TODO，故不能用该便利函数验证显式BH。该仓库本次未找到明确顶层许可，保持研究用途、不移植。作者新增 `surface.rs` 依赖已有 tegra_swizzle；本批未调用任何转换。

[tegra_swizzle v0.4.0](https://github.com/ScanMountGoat/tegra_swizzle/tree/6fb9dfef9e6630bd155632802b3ead5ce5c656c8) 是 MIT 工具，但审阅时发现 blockheight.rs 文件头明确标为从 gdkchan/Ryujinx C# driver 代码移植，README 也声明部分测试数据来自 emulator。因此**本批不把这些 mip/BH/array 算法当作独立于历史 renderer 的新 NVN 行为来源，不移植到 runtime，也未执行**。下载后识别此来源限制即止于记录，不沿链接读取旧 renderer。公共源 URL/commit/SHA 都在 downloads.json。

## 当前 native sampling 对 explicitBH 的传播

源代码输入哈希见 `consumer-source-inputs.json`；这是静态所有权/参数流审计，不是采样 GPU 资格。

| 边界 | 当前文件/行 | 事实与限制 |
|---|---|---|
| 8B→explicit source desc | texture_source_resource.inc:36–48 | gate1才decode全8B，BH显式写desc；没有回落-1/implicit。flags0、2D/array/sample0及原pool策略保留。 |
| 初始化所有权 | 同文件:66–72 | 完整layout按值分配克隆，保留8B，持canonical pool ref；不靠已销毁builder指针。 |
| layout自校验 | texture_source.c:91–103 | validate从保存的base_block_height_log2重建expected；不是从尺寸重新选择implicit BH。各mip有效BH/offset/layerstride必须完整memcmp一致。 |
| registration | nvn.c:1334–1357 | descriptor entry另存完整source_layout克隆，CPU对象Finalize不丢布局。view仅存选择metadata，没有把full texture重新解释成subset native资源。 |
| enqueue snapshot | nvn_pipeline_draw_prepare.inc:59/89 | 同一次entry捕获把完整layout按值复制到prepared数组；query、native admission、materialize用同一份。 |
| 生产采样 gate | 同文件:179、194–195 | 只普通2D、depth1、level1、sample0；BC plan count1，view subset在约74行先95。多mip/array不会被偷偷当作单层单mip。 |
| BC shadow参数 | texture_bc_shadow.c:13–31 | 实际FDL产生native布局；原source每mip传nn_block_copy_plan，源BH/pitch/offset/layerstride来自保存的source，不来自FDL/native descriptor。 |
| raw-block UBO | block_copy.c:20–22 | args持实际mip base/layerstride/widthGobs/有效BH。bind57–72重算并memcmp plan，防止换成另一个隐式参数。 |
| 真GPU转换指令 | shaders/block_copy.comp:13–18、35–36 | GOB地址使用UBO blockHeightLog2，与canonicalBase/layerstride相加；不CPU转换。native采样descriptor描述FDL shadow，不应保留NVIDIA GOB BH位。 |
| RGBA8 shadow | texture_bridge.c:49–55、83 | validate保持单mip/层/2D gate；nn_tex_gob_copy_args从source的实际mip BH填写参数（texture_source.c约133–140），没有default重算。 |
| materialize/retire | nvn_pipeline_draw_submit.inc:35–58 | 使用prepared source，返回storage/code/uniform/dispatch owner全部进入既有job集合，失败也保留返回owner。没有读取registration的新版来替换已捕获版本。 |

未发现当前支持子集里“显式BH被尺寸默认值覆盖”的路径；这不是 packaged sampled 的新 GPU 通过结论。public packaged Copy144 GPU由root另测，不能继承成 sampling/frame 通过。

## 后续最小工作边界

- 未知word/marker/upperbits/target/flags仍95；真实builder observer原样保存8B已足够辨别新分支，不需要新增CPU指令工具或猜version。
- 若有实际容器文件，先用现成reader导出 **全TextureLayout两字、MipOffsets、ImageSize、ArrayCount、Alignment、容器版本**，与实际NVN builder profile单独关联；仅容器字段一致仍不等于原driver语义。
- 扩mip/array采样时必须同时扩actual descriptor/query/view selection、所有shadow dispatch owner/refs、samplerLOD行为及GPU像素资格。目前materialize只转交BC单mip的4个private BO；**不能只删prepare的count1/levels1 gate**，否则会遗失后续mip uniform/dispatch owners。未建议本批修改。
- packed RGBA8/BC单mip未来采样资格应显式选与implicit不同的有效BH（含BH5高块行），使用现成GPU转换＋真正纹理unit，而非CPU布局搬运。原copy资格不能代替它。
