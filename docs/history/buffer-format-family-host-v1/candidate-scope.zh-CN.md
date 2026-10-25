> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

本包修复普通BUFFER资源CPU元数据被两种native GPU格式限制的问题；没有开通新的BUFFER GPU消费者、读取SDK CPU/原shader、运行设备或游戏。原candidate/records均保留。

11文件最小补丁独立于typed archive/已冻结raw3D资格。rebased public proof使用root ComputeCall+BC14+typed17实际源码；最小patch按显式owned文件生成，不覆盖这些已合功能。

公共资源支持48种有独立元素大小事实的普通颜色格式：R/RG 8/16/32(1..24)、RGB32F/UI/I(34..36)、RGBA8/16/32(37..48)、8种packed颜色(57..64)、BGRA8(154)。RGB8/16、sRGB、depth/stencil、BC/ASTC、UI2F/未知枚举仍ENOTSUP。这是当前可证metadata范围，既不宣称所有合法NVN格式已穷尽，也不宣称拒绝域在NVN规范中一概非法。

原SDK53.314已有冻结事实为BUFFER width(texels)*format cpp，非GOB/height1图像；GetAlignment先bit6=128、再bit4=32，否则本backend声明的common-kindfe政策取cpp，12B特殊4。此轮仅读既有事实报告，不读取原SDK CPU/字节/ASM。独立deko固定373bfd的连续BUFFER/12->4规则与FormatTraits原始普通/packed元素大小逐项交叉；public NVN枚举与vendored shader TextureFormat只当各自域定义。source-facts.json含精确URL/版本/许可边界/源与事实行哈希，不含原SDK字节。

新静态inline texture_buffer_format.h提供元素大小与明确存在的translator格式对应，不新增库/runtime依赖。GetSize做checked width*cpp并保留当前public int返回的INT_MAX byte政策。flags只0/LINEAR/LINEAR_RENDER_TARGET单独三值；后者的128对齐不授予BUFFER作为RT的权限。Initialized source.kind仍0/class100；Builder沿声明common-kindfe/class1fe，RawClass对两种LINEAR标志为1，其余为本backend普通颜色兼容分类2。

SetStorage/Initialize/Register/View继续保留原pool/BO/GPUVA/offset；source_layout=NULL，没有shadow、重新绑定、像素映射或CPU转换。登记保存完整typed snapshot并持原pool，在TextureFinalize后仍有效；替换/错误rollback/表Finalize沿现有真实所有权。非identity view/reinterpret/mip/layer仍拒绝。现native BUFFER recipe/binder仍RGBA8/R32F两格式；Draw/RT/copy/texels/Window等原BUFFER消费者拒绝保持。CPU登记成功不等于GPU支持。

QuerySamplerType只根据实际registered BUFFER返回维度，跨度按实际element bytes校验；QueryTextureFormat对vendored枚举明确表示的39种格式返回真实值，缺项拒绝，不会将RGBA32F或其它格式落入旧else错误当作R32F。当前ordinary2D多mip准入保留root BC14代码，没有回退。

runtime FDL buffer元数据40格式=上述48减packed8；真实fdl6_buffer_view_init按已审format映射生成16DWORD，RGB32专有分支不扩image/volume format映射。packed8已有byte大小事实，但本轮未审其native位/通道格式，因此FDL显式拒绝；其CPU资源元数据仍独立可用。depth/compressed/sRGB也拒绝。普通RGBA8/R32F descriptor矩阵与旧实际FDL逐DWORD对照保持。

FDL严格检查whole-element length、真实WIDTH15+HEIGHT15容量、49bit地址与64B aligned-down prefix可表示性。特别RGB32 public alignment4并不保证每个4B offset可用原FDL STARTOFFSETTEXELS表示；prefix必须整除12，否则FDL元数据返回EINVAL，绝不挪动caller地址。这个native限制不反过来拒绝合法CPU BUFFER构造。任何未来实际native view仍须证明完整prefix/bytes归原owner且保持生命周期/依赖，不能用纯metadata代替BO证明。

验证：
- public-buffer-rebased-final-v1/proof.json：GCC ASan+UBSan、Clang function各23955 HOST检查（其中新family23175）；48格式×宽度/三种flags/完整views与登记、原pool/地址、Buffer alias、整数/float query、typed span错误、OOM/cache rollback、悬空builder/descriptor持有、CPU_NOACCESS、原双handle GPU拒绝无分配/缓存/submit；实际v14 shape65536×16B和offset18002624。NDK29当前core+真实KGSL/Android --no-undefined链接通过。fixture旧私有fake Draw缺target，现改为真实public Initialize的LINEAR RGBA8 target，未修改生产代码掩盖无效fixture。
- runtime-fdl-final-v1/check.json：两套HOST直接原FDL对照新增40格式760 descriptors/3935checks；原47image+volume范围完整回归。Android独立Meson新config/7原Mesa单元，69文件最小source closure复制后再次NDK严格重编；exports仍五个元数据函数，DT_NEEDED仍m/dl/c，未引入shader compiler/GL/Galliumdriver/Vulkan或像素转换代码。没有Android执行/GPU资格。
- legacy-buffer-views-v1 +public checker foundation：原两recipe、50原FDL描述符、432×两san仍通过；生产nn_tex_buffer_bind及其finite native消费者未改。
- source-facts.json同时审计48元素大小与当前shader enum：39种可表示QueryTextureFormat，RGB32/部分packed/BGRA没有该namespace的等价项则拒绝。构造器、FDL与shader query三个能力域不混同。

正常复现（root合入后，fresh out，不改历史record）：

    python3 nvn-native/tools/build_runtime_fdl.py --root "$PWD" --ir3 "$PWD/.local/nvn-native/ir3-volume-types-integrated-v1" --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 --out "$PWD/.local/nvn-native/runtime-fdl-buffer-formats-integrated-v1" --jobs 8
    python3 nvn-native/tools/texture_buffer_views.py --root "$PWD" --ir3 "$PWD/.local/nvn-native/ir3-volume-types-integrated-v1" --out "$PWD/.local/nvn-native/texture-buffer-legacy-views-integrated-v1"
    python3 nvn-native/tools/check_texture_buffer_resources.py --root "$PWD" --views-check "$PWD/.local/nvn-native/texture-buffer-legacy-views-integrated-v1/check.json" --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 --out "$PWD/.local/nvn-native/texture-buffer-family-host-integrated-v1"

本包不改IR3/compiler/stage/pipeline schema，无需为此重编IR3或kernel。runtime_fdl.cc/h和正常builder的源指纹变更，FDL记录应按第一条重建；依赖其记录的normal host/APK需正常刷新。公共front-end7文件与FDL4文件另分patch，root可串行审阅；合全部时以minimal-promote.patch为准。当前真实D2R仅有v14 metadata/SetStorage原生失败观察，尚无新构造成功或BUFFER GPU/game画面验证。
