> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# RGBA16F 真3D shadow owner 增量（冻结）

本包只扩展已冻结的 `volume-shadow-candidate` 四文件前置模块，使其支持 RGBA16F（NVN 0x29）原始8-byte texel→实际 FDL 真3D native sampled storage。没有公共 NVN success stub，没有生产 Draw/query 接通，没有 CPU 像素转换。原 canonical BO/offset/GPUVA、Mip/BD/Z 布局不替换；Android/GPU/游戏执行均未进行。

## 最小文件与应用顺序

1. root 已合并并正常生成43-format纯metadata：`.local/nvn-native/runtime-fdl-color-formats-integrated-v1/check.json`。该功能可按原 Mesa 实际能力支持43 metadata formats，**本包不增加人为的volume6格式metadata门，也不修改FDL算法、映射、ABI、producer或旧记录**。
2. 尚未合并原shadow四文件时，应用 `minimal-promote.patch`（四个新增文件）。
3. 若已合并原冻结shadow四文件，改用 `rgba16f-delta-against-frozen-owner.patch`（同四文件小增量）；两者是替代选项，不叠加。

四文件仍是：

- `include/nvn_native/texture_volume_shadow.h`
- `src/texture_volume_shadow.c`
- `tests/d2r_bridge/texture_volume_shadow.c`
- `tools/check_texture_volume_shadow.py`

运行时实质变化仅 `source()` owner admission 加入0x29；共享正常volume kernel已经按真实bytes_per_block工作。43-format metadata可成功不等于这些格式全部开放GPU owner，本模块仍只接受RGBA8、RGBA16F、BC1 RGB/RGBA、BC2、BC3。

## 所有权与实际接入路径审阅

`nn_volume_shadow_plan(source,baseMip,count,...)` 使用 `nn_fdl_volume` 返回的 **TYPE3D descriptor、每mip native offset/pitch/slice_stride、实际min_layer_size/required_span**，每个源mip调用真实 `nn_volume_copy_plan`。源 `.target=3D/.layers=1/.volume_depth` 及每mip BD/Z 保持，绝不把Z当array layer、也不以整个volume layer_stride当slice pitch。

`nn_volume_shadow_create` 需要同device的正常volume-kernel image，以及强持有原canonical纹理/pool/Device的调用者和完整range。每次创建fresh independent native storage、共享code及每mip不可变UBO/IB：单mip4个私有BO，N mips为2+2N；`resources()` 还返回借用canonical。source/native像素不map/cache/初始化。`ready` 只表示metadata/jobs可用，不表示GPU已转换完成。

实际D2R未来接入要补的明确边界：

1. Device已有 `nn_nvn_register_volume_copy` 和独立 `device.volume_copy` 正常kernel注册，不能误用2D block image。模块不自行注册、不建queue。
2. public registration产生的实际source snapshot/pool range必须按值保留，入队pin原Texture/pool/Device；submit前用实际版本/别名/写入依赖准入。不能从archive预期资源倒推真实绑定。
3. 在原producer已排序之后提交所有mip的真实jobs，全部canonical+private BO加入refs；借用canonical不由模块销毁。需正常compute completion/invalidate→实际consumer的显式顺序。未知submit/Finish保留整个owner及原pins，不能因idle getter当前返回0就回收。
4. 现在 `pipeline_resource_queries.c` 仍拒depth!=1/levels!=1，只解析2D/BUFFER；`nvn_pipeline_draw_prepare.inc` 仍限定actual2D；`pipeline.h` 的 `nn_pipeline_image/nn_fdl_image_view` 与 `sampled_fdl[16]` 是2D合同。`pipeline.c` 也按2D descriptor验证。本包**不**把volume view强转为它们，也不将height/depth伪装为array。未来需显式typed volume alternative，使用48B volume desc/768B view、真实16DWORD descriptor和selected view_depth/level metadata。
5. compiler normal frontend目前接受的typed texture specialization尚不包括Texture3D；实际3D shader/resource queries和descriptor consumption需独立新增与资格，不能继承2D实验。此模块metadata/owner成功不代表已具备3D采样shader。
6. **IMAGE=4只为源存储permission。** 本模块始终 `usage=NN_FDL_SAMPLED`，没有ImageLoad/Store绑定、storage_descriptor、3D MRT或GPU写回方向；不能把本shadow拿来宣称IMAGE执行支持。当前owner单向canonical→native sampled，只适合读consumer；未来会写native的consumer必须另闭合回写/alias版本，不可直接沿用本单向结果。

正常CMake当前不会自动链接此未消费前置模块。未来typed3D consumer接入时应显式加入 `src/texture_volume_shadow.c` 并链接已核验FDL archive，持有完整owner；不能仅为让现有API返回成功而接空路径。

## 新验证及共存证据

最终proof：`tree/.local/check-root-fdl-v3/check.json`，直接核验root新正常43-format记录及现有正常volume-kernel记录，不编辑或重签旧record。之前依赖候选43库的v1/v2记录保留为历史，最终采用root正式新库。

- GCC ASan/UBSan和Clang function各通过98真实FDL plans、20 owner lifetimes、3持久cleanup失败owner，live=0；5,659,983 assertions主要来自guard/readonly字节，不是百万项GPU能力。
- 其中新增RGBA16F：6种动态形状/mip子视图；3条普通owner路径，另实际32³ flags=IMAGE4下1/3mip两条owner路径。flags0/4 canonical payload及整个materialization plan一致，保持sampled descriptor；bytes_per_block=8/block1×1；伪改为4B source metadata拒绝。
- 新格式继续覆盖实际UBO/SSBO descriptor、dispatch/owner ledger、source与native不被CPU map/cache、完整原始字节guard、prefix/ceil4 owner范围、busy/no-premature-free、rollback保留和容量无partial-result。
- 原格式与失败路径同fixture继续通过。RGBA32F仍明确拒绝；43 metadata成功不扩大owner。
- NDK ARM64 API29/O2/Wall/Wextra/Werror/`--no-undefined`：真实normal FDL archive + RAM fixture严格链接，另真实KGSL/backend+module+normal FDL shared library严格链接。
- 同一root43-format库原正常record已具有实际Mesa直接对照：原volume390例+新增volume624例，共1014；2D/BUFFER也有各自直接对照。这里只重用其已锁定source/config/library证明，不把HOST或Android ELF编译当Android执行。

复现（先合格式43及本四文件）：

```sh
python3 nvn-native/tools/check_texture_volume_shadow.py --root "$PWD" \
 --runtime-fdl-check "$PWD/.local/nvn-native/runtime-fdl-color-formats-integrated-v1/check.json" \
 --kernel-check "$PWD/.local/nvn-native/volume-copy-kernel-integrated-v1/check.json" \
 --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
 --out "$PWD/.local/nvn-native/volume-shadow-rgba16f-integrated-host-v1"
```

下一GPU资格应单独验证实际canonicalBD/Z→真实FDL每mip/slice_stride的native字节、padding/neighbor guards、两次source更新/replay和unknown-owner保留；未来采样再增加真实typed3D shader/queries。此前raw volume copy、2D BC和metadata资格都不等价于本native shadow或3D texture-unit的实际GPU通过。root负责排队；本包未运行ADB。
