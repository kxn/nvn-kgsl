> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 生产 shader 接入最小闭包（只读计划）

本报告没有修改 tracked、构建新编译器、使用 ADB 或执行 shader。Root 已报告 A650 metadata12/14 各17次提交、合计557056通道通过；这证明已选原VS/FS的 qualifier 与软件 descriptor 角色，不能证明真实 D2R 已调用 ProgramSetShaders/执行游戏 shader。本报告不读取/复用项目 Vulkan 后端、Mesa Vulkan 驱动或 PC shader 实现。

## 结论与最短路线

先让**通用生产 cache provider 自动接入已验证离线产物**，用真实 D2R ProgramSetShaders 的完整输入去匹配；首个真实输入若匹配已有原CS，走既有 BindProgram→DispatchCompute→nn_cs_encode→KGSL，不必先把 C# 与 Mesa 编译器搬进 APK。输入、资源和执行状态不匹配则保留具体失败。首个真实 shader 不一定是现有四CS，不能推断顺序、伪造命中，或用测试程序主动调用代替游戏调用。

生产 loader 与 miss/编译请求接口先通用化，D2R 产物/原始输入身份只在 adapter 的生成 manifest 内。不要把4个hash、游戏地址、测试oracle、相机/世界判断写进 nvn.c。初始只加载已审核输入是**有限生产离线缓存接入**；它比人工 qualifier 注册前进一步，但仍不是离线全集或运行期新 shader 编译。

Graphics 不能只解除 cache miss：当前 native artifact 与 runtime 明确只支持64x64目标、固定2/3属性、固定CB映射，以及2x2单纹理 sampled profile。真实图形调用的状态若不同，还必须按实际输入扩通用 emitter/resource/state，不能改游戏分辨率/布局或套用 qualifier 的矩阵、纹理、blend默认值。

## 当前事实与可复用部分

| 模块 | 可复用事实/位置 | 仍缺的生产连接 |
| --- | --- | --- |
| 严格 Maxwell 前端 | shader/maxwell/Program.cs:24-74；GetCode、C1读严格边界；binding/reflection；:163-216做CS/VS/FS翻译并输出SPIR-V/查询记录 | 现在只接受fixture文件，不读取真实NVN程序/设备状态；missing query会失败，不能填测试默认值 |
| 前端依赖闭包 | tools/build_maxwell.py:48-78，仅maxwell/Graphics.Shader/Common/Spv.Generator四项目；Maxwell.csproj net10.0 | 已验证为host .NET工具；没有已验证Android runtime/AOT构建或可调用C ABI |
| common SPIR-V→NIR | shader/nir_bridge.h:166-232：spirv_to_nir、buffer IO lowering、nir_lower_samplers、严格标量set0/binding<16、稀疏sampler bitset | 文件CLI需提炼有界内存API和返回错误；现仅普通floating2D tex、无arrays/images/dynamic索引，CS shared/private等仍受限 |
| IR3编译/链接 | shader/spirv_ir3.c:16起；graphics_ir3.c:13-40/117起，真实variant metadata与ir3_link_shaders | 可重用编译算法，不能把目前main/文件输出/abort接口直接挂到生产线程 |
| native CS artifact/执行 | compute_artifact.cc:14-26/62起；src/compute.c；nvn.c:1436-1474按reflection绑定UBO/SSBO和真正group数 | 只覆盖已审核private/shared/sampler/const等subset；未知kernel先明确失败，不能截断UBO/强改local size |
| native graphics artifact | graphics_artifact.cc:20-114；src/nvn.c:890-990；src/graphics.c:65-99 | emitter本身与registration/Draw均有固定qualification形状；普通新IR3机器码通过≠任意D2R Draw可执行 |
| 生产ProgramSetShaders | src/nvn.c:998-1062：完整code/control比较、count1/2、stage pair任意顺序、分配真实native code BO，busy拒绝 | 没有编译/miss provider；device cache只由nn_nvn_register_*显式填充，host_bridge未注册测试缓存 |
| 离线包验证 | graphics_api_bundle.py、texture_api_bundle.py、shader_fixture.py:18-75及正常check/build工具 | 当前生成`nn_test_graphics`/`test_programs`，compute_bundle还混有4kernel oracle/白名单；生产包必须去除测试逻辑，保留正常输入与编译指纹验证 |

前端TargetApi枚举和common NIR的SPIR-V环境枚举只是既有中间格式/descriptor约定，不实例化任何Vulkan设备、驱动或运行时。沿用已验证的这层格式约定，不引入驱动代码。

原Metal的generic ProgramSetShaders仅复制shaderData并更新serial（dm/gfx/nvn/hle_nvn.c:578-584）；nvn_mtl.m:2429-2454在消费时按control摘要找离线原Switch shader。该事实说明原路线已有离线cache/延迟消费，不构成native后端只按control短hash接受代码、容忍miss后丢draw或继承Metal实现的依据。这里不复用Metal shader/PC代码。

## 首个真实 ProgramSetShaders 必需观测，先标量再有界输入

优先在既有typed adapter加一次/首miss诊断，不另写计时器/采样器，也不扫描整个游戏：

1. program对象、count、每个shader.data的GPUVA、control的CPU指针；按当前`address_pool`解析source pool/flags/offset/可用长度。日志不是guest PC；只有模块映射能证明时才报module+offset。
2. control指针的可读区域证明（先复用loader/module/已知资源映射；不能直接对未知指针memcmp/dump1MiB）。先限定已验证control profile；只存忽略目录的首个必要code/control/C1快照及哈希、原模块/资源build哈希。
3. 现提取器已验证0x880 control magic、+0x700 code_end/+0x704 total；graphics container magic、+0x30 SPH stage；CS +0x748 local sizes/+0x754 shared（extract_embedded_cs.py:62-89，graphics/textures提取器）。这是已选原记录的profile，**未证明所有runtime shaderData.data都指向相同container起点**。真实GPUVA与original container/entry关系、C1实际存放位置必须观测/静态SDK证据确认，不可把control-total当任意GPUVA。
4. code/control/C1长短、stage、C1是否为空、local/private/shared要求。当前`local_bytes=0`提取器明确写成测试验证选择，不是通用控制字段。C1不是NVN普通uniform数据，不能清零或省略key。
5. 首次BindProgram/Dispatch/Draw的实际resource与state：NVN uniform/storage slots、地址/范围、descriptor domain/API selector/当前table版本、vertex格式/stride、RT格式/尺寸/pitch/layout、primitive/count/viewport/scissor、device origin/depth与其他fixed state。ProgramSetShaders可能早于纹理绑定；不可拿2x2/nearest/clamp当生产事实。

CPU_NO_ACCESS以及GPU_UNCACHED source的当前ProgramSetShaders门禁保持。即使未来从CPU翻译原shader在逻辑上可建独立“编译源”角色，也必须另行资格，不能让descriptor CPU角色传染给代码源。

## 查询和binding来源，不能混域

- `QueryHost*`来自已审核backend能力profile/真实chip/compiler条件。明确不支持的能力可显式false；依赖未实现fallback的true不能沿fixture复制。请求到没有来源的能力继续报具体query错误。
- compute local/shared从已核control schema；private/local bytes要有独立证明。graphics类型与需要specialization的state来自真实shader header和已记录NVN状态，不从测试attribute_types全float推断。
- ordinary sampled texture的sampler type/normalized semantics来自实际注册texture/sampler和已核NVN语义；host RGBA8/sRGB/FDL layout属于资源描述符。`QueryTextureFormat`是shader typed/image语义，不能填Adreno硬件format代号（当前明确拒绝typed image）。
- 复用前端反射cbuf slot→SPIR-V binding、sbuf(cbufslot,word)→native binding；native CS现将Maxwell c3..16映射NVNUniform0..13，将driverCB0 0x310+16*slot映射storage slots（nvn.c:1446-1463）。只对已验证profile使用这些规则，CB0不是Uniform0。
- texture CB2字偏移、NVN combined/separate域、API selector、pool ID、native binding五者独立。当前采样例native1/1来源T10/S0；它是已选程序的reflection，不是通用固定binding。继续使用已通过的按录制值copy、enqueue解析注册entry、不可变private job、实际texture backing强ref方案。未知动态索引/array/image路径保留拒绝。

## 可执行任务顺序

### P0：真实首输入诊断/小范围重现（最先）

扩现typed observer并记录首个ProgramSetShaders cache hit/miss/具体拒绝阶段；只捕获经过范围验证的必要输入。产物为正常fixture envelope、实际state/query来源清单和完整hash。验收：原D2R执行真实call；没有把测试specialization写成captured frame。此步不声称shader已执行。

### P1：生产离线 provider 自动接入（最短第一shader路径）

从正常check产物生成**无oracle/测试白名单依赖**的通用cache pack和immutable目录。先可用生成C数据由正常build/pack记录封装，避免马上新增不受校验的二进制文件parser；或实现严格pointer-free序列化loader。genericcore只暴露provider/cache接口，adapter提供原始输入身份/产物，Device初始化时自动配置。复用现register validation/full-byte matching；客体不调用fixture registration。

缓存身份至少包括完整code/control/C1、control/translator/compiler/ABI schema、实际target chip及编译相关state/query、所有工具/规则指纹；graphics native pipeline还要匹配实际格式/IO/layout/fixed state。短control hash只能索引，不能替代完整匹配。C asset/动态目录必须拥有所有source/reflection/image数组，活到所有program/handle/inflight版本与device释放；不能引用临时stack/文件mapping。

首个真实shader若是已覆盖CS：自动命中→真实ProgramSetShaders成功→原Bind/Dispatch/Submit/Finish，使用真实caller buffers/groups，保留UCHE invalidate/资源refs/guards。若不是已覆盖input或state则导出有界miss并明确失败，再用同一host流水线编译真实快照、正常重建原始输入产物后重跑。不能把反复离线补包称动态runtime compiler，不能改guest code/参数或跳过shader。

验收区分：①真实游戏cache hit与code BO加载；②原游戏实际非零dispatch/draw由硬件执行并完成；③输出语义核验/画面正确性。仅日志hit或空command submit不满足②；①/②也不能冒充完整游戏画面/玩法里程碑。

### P2：按实际首个miss补最小通用native capability

先让Maxwell→SPIR-V→NIR→IR3保留原输入完整语义，记录所有真实拒绝；原工具可输出合法机器码而artifact被拒时，扩native emitter/registration/Draw的对应状态，而非改shader成测试形状。graphics优先解开程序native code/IR3元数据与目标64x64、固定vertex/UBO/texture形状的耦合：真实IO/linkage、NVN vertex state、FDL texture/RT描述、常量range/table和fixed-state支持各自受检。复制硬件编码参考可以来自允许的Mesa IR3/FDL/common/registers/Gallium硬件源，不带入Gallium runtime。

“不改shader”也包括不删C1、纹理/kill、sampler、RT写、depth或atomic效果。暂未实现feature具体失败，不为命中cache放宽profile。现4CS/native图形的限制逐条由真实用例触发才扩，不提前穷举全部格式。

### P3：device-side miss编译（完整通用runtime缺口）

提炼Maxwell前端的内存request/result、common NIR/IR3的可恢复错误C ABI。现C# net10.0 host程序、StrictAccessor的interface reflection、System.Text.Json与Common包依赖尚未完成Android部署/AOT审计；不能声称现DLL可直接在Android生产运行。现Mesa builder明确host-only，也没有已验证NDK compiler library。可独立验证native ARM64/AOT shader编译器与NDK commonNIR/IR3；不引入任何guest ARM64 CPU解释/JIT/DBT，不自行写新的Maxwell translator替代现工具。

先保留同步owner-thread miss路径/明确错误；编译器worker若后续引入，不能跨线程直接操作现全局NVN objects/last_error/BO refs。provider结果在owner线程原子安装，失败保留旧program/refs，旧inflight版本不可变。磁盘cache用完整schema/指纹和有界section/offset验证、原子写入与损坏拒绝；不能反序列化C指针或跨Mesa版本直接复用NIR。

## 缓存拆层和必要负例

前端key=原始三blob+显式queries/语义state+前端规则；IR3 stage key=SPIR-V+chip/compiler/options；linked pipeline key=stage/linkage+实际vertex/RT/fixed-state ABI；job=enqueue时资源地址与descriptor版本。普通UBO内容、texels、pool IDs/GPUVAs不应变成代码身份；只有真正编译期读取的C1/状态才入specialization。GPU资源仍按真实BO身份去重和保活，不能按slot或两阶段两个UBO盲算数量（本轮actual shared-UBO observer错误已修复并真实GPU通过）。

最低回归：单字节code/control/C1变化不能错命中；同地址更新重设program和不同地址相同输入；stage array调换；chip/GMEM/schema/编译器指纹错误；缺query、未实现feature、坏cache section；source/allocator/compile失败不替换旧program；compile期间/record/inflight寿命；已记录handle保持旧variant；同UBO BO多range/多stage正确去重；descriptor late replacement与private硬件表不指向caller CPU table。复用现host sanitizer、spirv-val、IR3 asmroundtrip、正常build记录和实际A650原shader pixel检查，不写CPU shader解释器。

## 当前仍未拿到的关键输入

真实第一次ProgramSetShaders是否已发生、count/stages、code/control/C1边界、code GPUVA到container的关系、source flags、实际compute/graphics state、实际首次nativeconsumer/支持feature集合，均没有本报告新取证。不能把metadata table的15256/1256 counts当shader sampler array长度；不能把API53.105兼容profile当控制blob格式或NVIDIA实测编译器版本；不能把metadata12/14通过扩大为raw shader/texture data GPU_UNCACHED资格。
