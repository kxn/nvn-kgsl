> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# NVN 外部定义与原 SDK 语义证据索引

以下主体记录冻结时的独立审计结果。随后 root 已合入三项独立声明：已知 ABI 508、typed forward 507、未知 ABI 15；native runtime 入口仍为337，没有因此增加 GPU 能力。原 SDK 分析只用静态解析，没有CPU模拟或旧 renderer。完整逐项索引保留在工作目录的忽略路径 `.local/nvn-native/parallel/public-nvn-definition-index/index.json`，对原 loader 的523个名字记录声明来源、静态调用计数和原SDK入口。此处可提交的 `sanitized-summary.json` 保留计数、18项原输入指纹及方法位置/哈希，`integrated-summary.json` 记录接入后的计数。原NSO/反汇编不进入可提交文档。

## 计数与证据强度

- 原 main NSO：`55fd909bb7230b002357e5b0d8499c11f06888f345c49e69eb4734df44a27d0e`，原 SDK：`1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017`，实际 SDK53.314。采用原有 NSO 读入、三段哈希核验、MOD0/RELA/dictionary解析及 Capstone 静态反汇编；不执行ARM。
- 523是 loader 名称总数；216是原工具能归属的静态调用名字数。当前本地头覆盖505个名称ABI，另18个仍unknown。**216与18的交集为0**；不是“216项仍有18个ABI缺失”，也不能由静态0推断运行时绝不查到/调用。
- 下载的 upstream C `nvn.h` 原始SHA恰为历史vendored `4c2f96bd...6091`，直接覆盖当前523中的477名；公开C++显式 `*Func` typedef覆盖498名（216静态调用中的215名；缺 `TextureGetRawStorageClass`）。当前505包含此前本地原SDK静态恢复，不能把记录字段 `public_prototype_names` 理解成505项都有官方公开行为规范。
- 有声明只能证明其作者记录的类型。原SDK静态码能证明具体版本的参数消费、存储/命令生成；不自动证明完整非法参数、并发、生命周期或平台错误行为。发布版常量返回不能恢复未使用参数。

## 可核查外部源、版本与许可

|源|固定版本/定位|能提供什么；不能提供什么|许可边界|
|---|---|---|---|
|[open-ead/nnheaders](https://github.com/open-ead/nnheaders/tree/246c3b482d045934ed004f50c5ada37f3b10b703/include/nvn)|commit `246c3b482d045934ed004f50c5ada37f3b10b703`|用户独立RE头；C头自述为Darksiders II DWARF重建、部分53.105，不能等同D2R53.314完整SDK|C `nvn.h` 自带h1k421 2020 MIT；仓库metadata license=null、无顶层LICENSE。C++头无同等许可notice，不将C头MIT自动扩大到整个目录|
|[open-ead/nnsdk migrated NVN](https://github.com/open-ead/nnsdk/tree/3565f8cd6a4ddecd8206248bbc6fe76d46e13fa7/lib/nvn)|commit `3565f8cd6a4ddecd8206248bbc6fe76d46e13fa7`|迁移版本也保留同3项显式Func typedef及FormatClass；未发现其余15项显式PFN；不构成第二份独立RE来源|同样逐文件看notice；metadata license=null。本轮只记录事实并独立声明，不拷入整份C++头|
|[SMO Doxygen](https://smo.monsterdruide.one/docs/nvn__Cpp_8h.html)|网页无可证明的SDK精确版本；其项目当前submodule与网页生成时间可能不同|便于定位类型/成员，不把网页未显示方法当“API不存在”。本轮直接读固定commit raw文件，发现网页类列表漏掉若干实际头中的声明|网站生成器页不替代源文件许可|
|[SMO repository](https://github.com/MonsterDruide1/OdysseyDecomp/tree/696db2ff5ed558b053c753de420504b8682f24b0)|该commit `.gitmodules` + gitlink `097321c9...`|确认其NintendoSDK来源为open-ead/nnheaders；与同一来源的BOTW codebrowser不能计两份独立佐证|不借游戏项目总许可覆盖第三方文件|
|[NVIDIA Maxwell B hardware definitions](https://github.com/NVIDIA/open-gpu-doc/blob/master/classes/3d/clb197.h)|本轮下载commit/SHA见`nvidia-source.json`；Mesa pinned副本也记录SHA|原厂方法偏移、字段位宽、硬件枚举；能核对color reduction实际写入。它不定义NVN C返回类型、对象生命周期或Android实现合同|文件开头NVIDIA2020 permissive MIT-style notice|
|原D2R SDK53.314|原始NSO、已核段哈希、RELA与523字典；每18项单独body SHA|本版本静态行为事实；无官方源码/许可公开授予。原码仅ignored，脱敏报告可记偏移/哈希/结论|不分发原NSO/asm，不移植原SDK二进制|

完整URL、commit、文件路径和SHA在 `public-sources.json`、`migration-public-sources.json`、`nvidia-source.json`。本轮精确名称搜索及这些公开原始头未找到完整NVN行为手册；这不是声称世界上不存在授权SDK文档。未使用搜索返回的移植renderer文章、CUDA/NVSHMEM相似API或原Metal/Vulkan实现作为定义。

## 18项逐项结论

下表“可补ABI”仅表示可安全生成正确typed forward；底层未实现仍须明确fatal，不增加入口实现数/GPU能力数。偏移均原SDK。

|名称|外部直接定义/静态事实|确定缺口与行动|
|---|---|---|
|nvnQueueBuilderSetQueuePriority|公开C++成员与10/100/1000 enum；raw `*Func`缺失。SDK+4715f0把W1写builder+38，保留X0返回|void与返回pointer仍不能由store+ret区分；C++流式引用由wrapper自身返回。原SDK消费为NvRmGpuChannelSetTimeslice，不得等同KGSL priority；继续unknown|
|nvnWindowBuilderSetNumActiveTextures|公开检索头缺；+477640写W1到+14|count参数消费确定，返回型及合法active/total范围无完整独立声明；继续unknown|
|nvnWindowSetNumActiveTextures|+477ab0经Window内部平台对象vtable尾调用，保留W1|平台目标返回合同/合法状态未闭合；不能凭相似setter声明void|
|nvnProgramSetShadersExt|公开Program成员为boolean+count+ShaderDataExt指针，ShaderDataExt只forward声明；配套wrapper被注释。+46fe20按24B stride拆16B shader pair及第三个64位量到内部函数|第三字段类型/语义、raw PFN缺失；不能用普通16B ShaderData替代，也不能把class声明当raw PFN已核|
|nvnProgramSetSampleShading|公开class void成员+SampleShadingMode enum0/1/2；wrapper被注释；+470200只store W1到program+78|raw PFN未记录；对GPU状态影响及返回合同还需补证，不仅接受store便宣称sample shading|
|nvnCommandBufferSetCommandMemoryCallbackEnabled|+45ce10只消费W1低8位真值，改command+70 bit5|直接公开PFN/返回型缺；需继续定位callback使用点和新SDK声明，不以NVNboolean参数消费推断void|
|nvnCommandBufferSetColorReductionEnable|**可补ABI**：公开Cpp.h:2514明确Func typedef；CppFuncPtrImpl:1445–47实际用该类型解析同名NVN。+486560消费byte，写Maxwell方法D9C|独立声明 `void(command*,boolean)`；没有新增Adreno等价实现或启用优化|
|nvnCommandBufferSetColorReductionThresholds|**可补ABI**：Cpp.h:2515–16 Func为FormatClass::Enum+两个int；loader:1448–50；+4865d0四个W参数生成2DWORD|独立声明 `void(command*,formatClass,int,int)`，FormatClass为0..5，不是NVNformat/compareFunc。尚不实现Adreno规则|
|nvnSyncInitializeFromFencedGLSync|公开重建头缺；+475030消费X0/X1/X2/X3，向X3可选out写32位status，并有W0=0/1成功失败分支|GL对象参数真实类型/位宽合同、status enum缺。平台调用使用W2不证明外部GLsync是指针或int；不用void*猜原型|
|nvnSyncCreateGLSync|公开重建头缺；+475120根据sync种类调平台interop，消费X1 out，涉及64位返回|完整GLsync typedef、返回值及status enum缺，保持未知；不引入EGL/GL替代运行路线|
|nvnCommandBufferSetStencilCullCriteria|**可补ABI**：Cpp.h:2545–46 Func及loader:1495–97；+486680消费W1–3，func-1低8位、另两个int低8位分别进16/24|独立声明 `void(command*,stencilFunc,int,int)`。必须NVNstencilFunc，不用取值近似的NVNcompareFunc；Adreno stencil-cull行为未实现|
|nvnCommandBufferInsertCheckpointStatic|公开原始头未找到；+45cd00只返回W0=1|未读取参数，参数个数/指针类型完全不可恢复；不能做成功stub|
|nvnCommandBufferInsertCheckpointDynamic|+45cd10与上一项相同8B体|同上，不因Static/Dynamic名称猜字符串所有权|
|nvnDeviceSetAutomaticCheckpointsEnable|+45cd20同8B常量返回体|无法恢复enable实参/返回类型；保持unknown|
|nvnDeviceSetAutomaticCheckpointsSamplingInterval|+45cd30同8B体|interval位宽/单位及返回型未知；不是已实现采样器|
|nvnDeviceSetAutomaticCheckpointsByDebugGroup|+45cd40同8B体|group/enable参数与返回型未知；不能借debug group API类推|
|nvnDeviceSetCheckpointCallStacksEnable|+45cd50同8B体|参数未消费，保持unknown|
|nvnDeviceSetCheckpointMaxCallStacksDepth|+45cd60同8B体|深度类型/范围/返回型未知，保持unknown|

## 三项ABI的硬件交叉核对

NVIDIA `clb197.h` 的 `SET_REDUCE_COLOR_THRESHOLDS_ENABLE` 位0与SDK immediate方法0x367（字节D9C）吻合。SDK对enable的发布实现是低byte等于1，而不是任意非零；这属于观察，不扩大native合法参数域。

FormatClass六项enum0..5与SDK六entry method table逐项对应UNORM8/10/16/FP11/FP16/SRGB8（字节10CC、10E0、10E4、10E8、10EC、10F0）。原厂字段明确除FP11为6bit外其余8bit；SDK同样对两个有符号int先夹到0..63或0..255，W2放高16位、W3放低位。可据原厂字段分别解释为all-covered和all-covered/all-hit-once阈值，不能用名称随意调换参数次序。

StencilFunc公开枚举NEVER到ALWAYS为1..8，与SDK减1再掩8位吻合。当前C `NVNstencilFunc` 的LARGE sentinel和公开C++LARGE=-1不同，legal1..8及32位ABI吻合；本次不改历史sentinel，也不将sentinel当合法渲染值。CompareFunc即便数值1..8相同仍是不同类型，fixture编译期专门拒绝混用。

## 216实际静态名字：按家族可实施索引

完整名称/签名覆盖在上述忽略目录的 `index.json/catalog`，这张表只列实际语义查证应走的来源和缺口，不把所有入口都归为“必须黑盒”。

|家族|比仅C头更可核查的来源|仍须原SDK/硬件资格解决的定义|
|---|---|---|
|CommandBuffer recording、Copy/Call、callback|Cpp.h明确独立方法签名，FuncPtrImpl解析同名函数；原SDK静态存储/控制区链可继续追|声明不说明空录制、跨Begin/End内存使用计数、Copy与Call源修改可见性、callback重入、并发对象寿命；不能据旧renderer简化|
|Barrier、Fence/Wait、Event|公开BarrierBits/SyncCondition/SyncWaitResult/Event enums及精确指针/uint64/返回值；NVIDIA提供Maxwell原语定义|无完整公开NVN内存模型文字；不同条件只约束到哪一GPU阶段、cache范围、flush标志、timeout单位及跨context必须核原SDK消费与KGSL/Adreno实现。全Finish是当前保守subset，不冒充必要原语|
|WindowBuilder/Window/QueuePresent|公开强类型Window/Sync/result enum，实际SDK平台delegation；DeviceConstantsNX明确SMO实测MIN2/MAX4|2/3是本native资格subset，不是NVN全局最多3。active数量API比旧公开头新；acquire/release/retire与caller textures所有权仍需要SDK/platform事实。公开SMO常量不是D2R/Adreno设备能力|
|Program/UBO/shader|ShaderData16B、SetShaders boolean、各stage绑定签名；原SDK真实CB3..16与API UBO0..13映射已有单独原始证据|源码C1/supportbuffer/资源handle specialization不能从公开枚举反推；ShadersExt24B不可与ShaderData混用；合法ShaderData不代表已实现任意shader|
|纹理handle、descriptor pools|公开Combined/Image uint64与Separate aggregate、FnPtr原型；原SDK构造和scalar/plural命令差异；NVIDIA bindless/TIC/TSC原语|标记bit32/符号扩展不是GPU samplerid或GPUVA。pool注册快照生命周期、scalar保留highword与plural写两word必须分开；本原 SDK 成功 QueueInitialize 的图形路径已由另项审计证明默认 texture-CB2；其它未证明路径仍 strict 拒绝，实际资源类型仍由 typed 状态确定。deko CB编号不可替代NVN|
|资源layout/view/copy、采样/固定状态|公开typed Format/Target/View/Copy enums；C++getter指针返回交叉补正C头错误；SDK布局/默认值静态证据及真实FDL/XML|C头曾把MemoryPool*误写opaque-by-value，必须SDK X0与独立C++返回共同纠正。publicconstants的alignment/尺寸来自SMO实测，不能迁成Adreno FD layout或未经实际测量的当前能力|

### 可立即落地的结果与下一步

1. `abi-three-candidate/minimal-promote.patch` 仅新增三条准确PFN及独立FormatClass声明；normal original-NSO extractor将505/504/18变为508/507/15，不改names缓存、不加runtime ENTRY、不实现setter。
2. typed HOST测byte值完整转发、六format enum、八stencil enum、signed INT边界、非法enum原位转发而不伪装native接受、精确类型负例、missing implementation fatal、sticky-before fatal、其余unknown仍fatal。NDK只编译/链接，绝不说执行了ARM64或GPU。
3. 新ShaderDataExt/QueuePriority等优先找原SDK带类型信息的公开RE/DWARF或可靠raw PFN；没有证据时保留unknown。7个checkpoint发布版stub需其它可核对版本/调试信息，单靠当前body继续反汇编不会产生未使用的参数类型。
4. 许可/版本问题不妨碍自行记录和实现已证实的接口事实，但不整体复制无明确许可的C++重建头。本项目 third_party README 已区分历史原件与当前独立增补头。
