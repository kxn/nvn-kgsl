> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# NVN 原生后端：改为按调用族批量实施

工作分支为 `codex/nvn-turnip-study-20261023`，工作树为 `.local/worktrees/nvn-turnip-study-20261023`。实现仍为 NVN → 原生 Adreno 命令、IR3 和 KGSL，没有接回原项目 Vulkan 渲染器。原 ARM64 装载、离线改写、宿主门和 HLE 路线不变。

## 方法与定义来源

以已经识别的 216 个静态调用名为第一批接口范围，按队列/同步/命令管理、资源/视图/拷贝、渲染状态、绘制/计算四组并行实现。先完成各组真实的状态、资源归属和硬件操作，再合并进行主机检查、统一 GPU 测试和原游戏测试；不再为每个新入口单独部署游戏。

`third_party/nvn/nvn.h` 明确来自 Darksiders II DWARF 逆向恢复，覆盖类型、枚举和函数原型，并非已获得完整官方 SDK 行为规范。公开的 deko3d/NVN 逆向和 Switchbrew 硬件资料作为语义对照；Adreno 字段取自已经固定版本的 Mesa 公共 XML、FDL、Gallium、NIR/IR3。接口定义足以并行开工，行为和资源布局的缺口继续通过原调用代码及黑盒结果验证。

## 已合并的第一批

- 绘制与派发：索引/实例绘制、普通和带计数的间接绘制、间接计算派发；间接参数在 GPU 执行时读取。计算着色器的工作组常量也由 GPU 拷入每份私有参数区，未偷读调用方的间接参数。图像与 scratch 配置按值/资源绑定保留；现有离线着色器配置不支持图像访问或非零原生 private memory，这两类着色器仍拒绝注册。
- 渲染状态：75 个 typed 状态入口，按值复制的小对象、Bind 快照、每次 Draw 的独立状态；在模板固定状态之后、Draw 之前发出 A650 寄存器片段。编译器产物 schema 保持独立，运行时状态未混入原着色器头或重新解释控制块。
- 资源：明确的 LINEAR 1D/2D 单层单 mip 布局、按值 TextureView、四种方向的真实 GPU 拷贝、纹理清除、UpdateUniformBuffer 录制时快照。ReadTexels 通过合法页导入、GPU 拷贝、等待及缓存处理写调用方输出，要求源 BO 已退休；不从另一个队列假定生产者已经完成，也没有 CPU 图像拷贝。
- 队列和同步：26 个新增入口、实际预算/借用工作区、不可变提交父 IB、内存回调与多段 arena、KGSL context timestamp 及依赖、CPU/GPU Event 子集、Barrier 和调试回调/标记。旧 builder 栈地址可在不同 builder 类型之间安全重置，live 资源不允许因此被覆盖。
- ClearColor/整数清除和 ClearDepthStencil 的已知参数处理已接到实际 RT；颜色清除当前要求已知完整 scissor 和全分量掩码，深度/模板附件尚未实现，不能把请求清除深度的调用当成成功。

入口数量不等于全部参数或 GPU 功能已经完成。RawStorageClass 既无已恢复公开 ABI，也没有可靠存储类别语义，继续留在原有未知 ABI 边界，未凭推测注册。Downsample/MSAA resolve、平铺/压缩纹理、深度附件和更多着色器仍是实质能力缺口。CopyCommands 和计数器在独立并行候选中继续实现。

## 实际游戏进度和证据边界

已经在新机 `DEVICE_SERIAL` 的 Activity 内执行原 SDK 初始化、构造函数和 `nnMain`；完整 RomFS 46 文件、8,973,547,258 字节已核验。四次原游戏 `nvnProgramSetShaders` 均返回 1，四份完整 0x898 控制块与原 NSO 提取件逐字节一致。

该轮随后在 `nvnQueueBuilderSetDefaults` 报 EEXIST。builder 地址复用是待整批游戏复测确认的根因判断，不能当作已经观测到旧对象类型。此轮没有实际 D2R GPU dispatch、Draw 或游戏画面。Activity 没有 waitpid 退出码，日志所写 exit 78 不冒充已观察的进程退出码；停止已核实成功。

归档 `a650-activity-full898-cache-summary.json` 只保留运行元数据、哈希、四次返回值和输入比较。包含原游戏控制块十六进制的完整日志、逐次二进制快照与生成着色器仍在忽略目录 `.local`，未写入 Git。

主机 sanitizer/ABI/NDK 结果只证明其声明的状态、命令编码和生存期范围。新增 GPU 状态、事件、跨队列依赖和间接绘制需要这次统一真机资格；既有测试三角形及其通过数量不算真实 D2R 出图。

## 统一绘制资格的当前结果

第一轮索引/实例真机提交与 Finish 成功，但首场景全黑，像素检查失败，释放与进程退出成功。该场景依赖尚未核实的 cull/front-face 极性，不能据此认定后端的具体根因。失败摘要与真实 16KiB RGBA 已保留；旧收集器把缺图错误文本列为图的七项已明确排除。

正式后续资格改成同一 handle 的正常索引→退化索引配对：正确读取新索引时第二次必须保持完整清除色；非索引替代或 CPU 捕获旧索引都会失败。这不更改后端 front-face 映射，也不把此轮标成通过。另已修复宽 MEM_WRITE 的 14-bit 长度与分块分配公式，主机验证覆盖完整 64KiB UBO、宽清除及拷贝边界；旧公式在同一检查中确实失败。窗口和 descriptor 旧主机回归也已通过，更新的是新增已实现接口与显式渲染状态后过时的测试预期。

## 后续统一真机结果与数量

HEAD `03468ab` 的资格 v2 已在 A650 通过：索引实例、数组实例、带计数的数组间接和索引间接绘制四种模式，各两次重放，共 8 次提交、8 张 64×64 图，131,072 个颜色通道、369,216 个 guard 字节和 230,432 个只读字节检查通过。GPU 将 count 从 CPU 初值 0 改为 1；同一 handle 重放时更新索引和间接参数，结果相应改变。实例数量由 ONE/ONE 混合强度区分。该结果仅覆盖当前固定 RGBA8/有限着色器配置，不验证通用 cull、深度或游戏画面。证据为 `a650-draw-batch-v2.json`、8 张 RGBA 和停止记录。

整批随后经正常入口从原始三个 NSO 重新构建，真实 D2R 再次执行原 SDK、构造函数和 nnMain。四次原游戏 ProgramSetShaders 均返回 1，QueueInitialize 返回 1，已越过此前的 QueueBuilderSetDefaults EEXIST。当前首个命名错误为 `nvnCommandBufferEndRecording` EINVAL 22，尚无真实游戏帧。runner 结束后 force-stop 成功。`a650-activity-api-batch-summary.json` 保留输入校验和结果；完整原游戏控制块仍只在忽略目录。检查发现后端拒绝合法的空录制/仅状态录制，原 Metal Begin/End 则允许；修复和回归不等于已经证明本轮游戏的具体触发条件，仍需下一批实际运行确认。

独立子代理在 HEAD `03468ab` 按集合重新核数：已合入 321 个入口，较 182 个基线增加 139；216 个静态实际调用名覆盖 212，尚缺 CopyCommands、ReportCounter、ResetCounter、TextureGetRawStorageClass 四个入口。全部 loader 523 名仍缺 202，其中 156 有已恢复 ABI、46 未恢复。Copy/Counter 的 ignored 候选未计入已合入数量；入口有参数限制或显式不支持分支，不能将这组数字解释为完整语义或 GPU 验证率。证据为 `api-entry-counts-03468ab.json`。

## 第二批：独立源语义核验与剩余入口

按用户新要求，本后端不再以原 Metal 实现作为语义规范或实现模板。接口声明/枚举、行为与宿主硬件实现分别取证，资料分层见 `nvn-public-sources.zh-CN.md`。原 SDK 静态代码证实合法 empty/state-only Begin/End，不再依赖先前查阅的其它后端；也查出本后端采样器默认 minFilter、minLOD 两处旧 HLE 遗留值不符，已修为 LINEAR 和 0。完整字段及精确版本依据见 `sdk-defaults-recording-research.zh-CN.md`。固定游戏 SDK 静态报告53.314；重建头/后端自报53.105 partial是另一层信息，本批未将其冒称53.314完整资格。

CopyCommands 与 Counter 已合并，复制是录制时独立快照，Draw/Counter metadata按逻辑偏移深拷贝，GPU私有子job按借用计数保活，不依赖源handle或源arena继续存在。根代理补获“源全部释放后直接取消最后一份未End目标”的空device崩溃，Begin现立即保留所属device身份；新回归通过，单独移除该赋值的负例确实重现 UBSan null pointer。Counter使用实际Adreno query/clock和GPU累加，尚待本批真机数值与上下文资格，未把HOST模型值当硬件结果。

RawStorageClass 的32位ABI、LINEAR类别和调用消费由原SDK方法表/RELA/callee及原main数据流恢复，再以公开独立枚举交叉核对；当前只对真实已支持LINEAR资源返回类别1，不为未知布局无条件返回常量。正式原名字工具重新生成/核验typed包装，详见 `raw-storage-sdk-research.zh-CN.md`。

当前工作源按集合核数为326个typed入口，216个静态调用名均有入口；523个loader名字还缺197，其中152有已知ABI、45未重建。这里只说明入口覆盖，仍有资源布局、shader/pipeline、深度/MSAA、窗口及参数范围等能力缺口，不能声称216个已经完整实现或实际游戏已经出图。两套sanitizer、真实ARM64完整链接及typed HLE转发检查已通过；可重跑工具为 `check_command_families.py`、`check_raw_storage.py`。下一步整批normal构建、原四CS indirect、独立NumWorkgroups消费、Counter实际数值和真实D2R运行。原字节均留忽略目录。

## 第二批真机完成结果，以及第三批语义分离

提交e0e1c30的正常重建已经完成。原四CS通过真实NVN间接派发与重放：8次提交/Finish，GPU把CPU初值0的参数改为1/3，输出分别32/65元素，数据/命令/guard/只读检查通过。这四份原CS不消费NumWorkgroups，另用明确原创qualification shader在production间接encoder中验证两次真实driverparams消费，不能把后者写成原游戏shader结果。compact结果分别为a650-compute-indirect-summary.json、a650-driverparams-summary.json，停止见a650-compute-batch-stop.json。

Counter真实12次提交/Finish成功，11个确定性sample、4个时钟、15个时间转换和10张RGBA全部吻合，资源释放成功。但同优先级两个context实际在同ring顺序执行，未观测抢占重叠，所以整体资格仍false/ENODATA，不能宣称跨context抢占隔离已验证。失败与全部实际图像已归档a650-counter-v1-summary.json。下一步应使用已核实的不同KGSL priority/ring与真实时间区间，不重复无意义增加同ring压力。

真实D2R越过此前EndRecording阻塞，四次ProgramSetShaders、两次QueueInitialize返回1，首个命名错误推进为TextureBuilderSetFlags ENOTSUP。该次没有观察到flags参数，不能猜成DISPLAY或LINEAR；仍无真实游戏画面，finally force-stop成功，见a650-activity-api-batch2-summary.json。

第三批按原SDK53.314独立语义将texture builder保存参数与资源布局资格分开：flags/target/尺寸/mip/sample/format/stride保存metadata，补齐Size3D和七个scalar getter；unsupported source/native layout仍在GetStorageSize/Alignment/Initialize拒绝，不创建空纹理。Defaults原尺寸0/0/0和默认flags0已纠正。完整typed参数/返回日志覆盖整个texture builder族，保留真实输入与首个失败阶段，不解引用guest storage。源NVN线性32B/RT128B和stride*height，与现FDL footprint不是同一合同；source-layout/GPU转换仍由下一批独立实现，不能把原FDL限定profile冒称通用NVN布局。

Sampler现在把已注册完整值快照转换为每次enqueue独立Adreno四DWORD描述符；min六档/mag两档、repeat/mirror/edge、有限LOD/bias及精确aniso档位有实际编码，旧job不可被后续重注册改写。当前资源仍只有单mip普通2D，尚不宣称多mip、shadow/border或aniso质量资格。目标字段依据固定Mesa XML/Gallium，不继承其它宿主渲染器。HOST原XML生成packer与真实core检查通过后，再运行独立GPU采样批次；描述符编码通过不是已完成采样GPU验证。

WindowBuilder原SDK在SetTextures借用caller数组，到Initialize才快照。已修正当前提前复制的差异；Initialize之后的liveWindow仍独立保有texture资源。Sampler/Window/Texture弱builder允许跨类型重用栈存储，但不抢占live资源；SamplerInitialize检查device仍存活，拒绝已Finalize弱指针。TextureView的presence/default/count0/内联值特性也已由SDK方法及真实消费者重新核实，详见texture-view-sdk-research.zh-CN.md；swizzle/mode及更多getter/Compare仍是缺口，没有仅通过改注释假称实现。

当前集合计数334个typed入口；216静态使用名有入口，loader523仍缺189，其中144已知ABI、45未知。新八个是metadata入口，不能用增加八个来冒称新增八项GPU功能；完整语义、源布局和实际D2R出图仍待推进。

第三批当前验证：真实current core与XML生成packer的三套sampler检查均通过（每套64849字段检查/32400组合、211 core/lifetime检查）；typed metadata和拒绝发布资源边界105项、RawStorage65622项，两套sanitizer均过；Window292项，Copy/Counter/Queue及SDK默认检查均过。真实NDK API29严格完整后端链接和HLE_FORCE检查通过。对应proof/runs已归档。这些是HOST/静态链接结果，后续GPU采样批次与实际D2R结果另记。

## 第三批真机结果与声明审计

b51c0af下，38场景sampler资格真机通过：18基础filter/wrap组合全部输出不同、4混合min/mag、6单mip滤波档位、4中心、4插值、2同handle重放；38次真实NVN submit/Finish、38张GPU读回、622592通道、1317536guard、5758976只读字节。RGB最大误差1在预先定义的1以内，alpha严格相等；两组重放及四组单mip档位结果逐字节相同。sampler进程停止确认。原着色器/producer/schema未改；不包括aniso质量、多mip、全尺寸、SC或真实游戏画面。见a650-sampler38-v1-summary.json及38张实际RGBA。

同HEAD正常入口从原三个NSO重新生成并完整核验，53任务实际native Activity host/typed bridge/KGSL链接后签名部署。真实D2R四个ProgramSetShaders与两个QueueInitialize返回true。当前观察到首纹理flags先8后9(COMPRESSIBLE|DISPLAY)，format37 RGBA8、target1 2D、2880x1620、depth1、levels1、samples0；此前没有SetStride观察。首错误为GetStorageSize ENOTSUP。该轮force-stop成功，无真实游戏帧。见a650-activity-texture-sampler-summary.json；原shader control十六进制仍仅在.local。

公开声明继续用原SDK callee、独立wrapper及ABI检查交叉核对：新增27getter声明，纠正3个MemoryPool getter的opaque-by-value错误为真实X0指针返回，生成器禁止再发布此类hidden-X8错误包装。正常原NSO生成/重复verify、30真实typed forward/182 ABI mock检查、ARM64严格HLE链接都过；这些不是新增27个backend实现。当前声明505、typed forwarding504、unknown18；实际backend入口仍334，loader缺189，其中171已知ABI、18未知。详见abi-recovery-sdk-research.zh-CN.md、api-entry-counts-abi-recovery.json与正常check_abi_forwarding.py。旧negative改用确实仍未知的QueueBuilderSetQueuePriority。

源码现含独立Generic16Bx2源payload计算与两个原创GPU copy kernel（GOB双向、LINEAR独立pitch），正常glslc/SPIRV验证/原IR3 asmroundtrip生成，不写自制shader compiler。当前HOST219521地址检查与8198真实encoder检查、双san/NDK严格链接通过，**尚未GPU执行这些kernel或接NVN资源消费者**。tiled alignment=0/public_extent_known=false明确防止把GOB512单位误用公开getter。实际flags9下一策略的独立证据见texture-compressible-sdk-research.zh-CN.md：原SDK允许压缩策略关闭且保留COMPRESSIBLE，但ChooseKind外部实现未知；native选择no-NVIDIA-compression/Genericfe必须声明为自己的immutable device policy，不能假称原SDK默认选择或直接忽略flag8。GPU转换、NVN canonicalpool与shadow别名回写、2880显示所有权继续分工实现。

## 第四批：独立源纹理存储与全尺寸显示基础设施

源资源采用明确的 immutable native policy：不生成 NVIDIA 压缩数据，选择 Generic16Bx2 kind0xfe。保留公开 flags8/9；不声称这是原 SDK 的默认 ChooseKind。匹配原 SDK kindfe 分支的公开 alignment512/no-big-page-round 后，2880×1620 RGBA8 单 mip 源 extent 为19,169,280字节。新增两项 StorageClass getter，已恢复 RawStorageClass2；绑定原内存池/offset并保留引用，不用 Adreno FDL 尺寸冒充源尺寸。尚未转换的采样、RT、copy、Window等消费者在改变数据或提交前明确拒绝；此批尚未通过实际游戏纹理初始化。正常 current HOST 两套sanitizer验证源资源5967项及原资源边界回归，NDK29真实后端完整严格链接通过。

全尺寸 Android 基础设施现支持经过实际 desc/pitch/FD身份/映射extent检查的有限 RGBA8 LINEAR形状、2/3槽独立所有权和真实crop尺寸。修正重新lock时同编号/同大小FD换inode的身份漏洞；负例移除此检查会失败。candidate HOST59次执行/2409项、Mesa FDL元数据和NDK严格链接通过，合并后的原Window292项双san回归通过。完整层stride按4096保守取整，可能拒绝分配更紧凑的合法AHB；未声称已经全尺寸GPU显示或游戏出图。

新增正常构建/运行入口的原创纹理转换资格批次：55布局×2重放，预期110次submit/Finish、612个实际CS dispatch、110份逐字节输出；独立逆sector oracle逐方向检查，CPU只初始化原始源和sentinel。当前完成正常FDL/源布局元数据与既有kernel编译闭包生成，实际GPU结果另记。真实游戏host明确关闭此模式，不链接资格shader。入口集合现336；静态使用名216/216有入口；loader523仍缺187，其中169已知ABI、18未知。入口覆盖不能当作完整NVN或游戏帧。

## 第四批实际GPU与D2R结果（HEAD7024853）

原创转换kernel在替换A650上实际通过55布局×2重放：110submit/Finish、612实际CSdispatch及sequenceword，110份raw共9,330,688字节逐字节零容差；活跃双向字节1,415,972，guards5,424,348，只读9,303,664，资源清理与最终停止均通过。GOB不同block-height/partial rows、cpp1/2/4/8/16、BC原始块、LINEAR32/128、5mip×3layer均在此批矩阵；不包含BC解压、公开NVN存储API、COMPRESSIBLE硬件压缩、全尺寸显示或游戏shader。摘要a650-conversion55-v1-summary.json；110份原创raw的无损归档a650-conversion55-v1-readbacks.zip。

随后正常原始三个NSO重建/verify，实际Activity ARM64 host54编译任务完成。真实D2R四次ProgramSetShaders、三次QueueInitialize均返回1。实际flags9 RGBA8 2880×1620源GetStorageSize返回19,169,280，Alignment512，原pool flags0xa1(CPU_NO_ACCESS|GPU_CACHED|COMPRESSIBLE) offset0；TextureInitialize返回1。首命名错误现为TexturePoolRegisterTexture ENOTSUP95/phaseafter，正是尚未接通的源纹理GPU消费者边界。无真实游戏帧；没有Activity waitpid退出码观察；finally_force_stopped=true。a650-activity-source-kind-summary.json保留原始输入指纹/运行结果/标量，包含原shadercontrol hex的原日志仍只留.local。下一整族为canonical源资源与native shadow的采样/RT/写回闭包、全尺寸独立AHB出图及通用shader桥接。

## 第五批：复用公共NIR/IR3，补齐纹理查询和组件元数据

正常build_ir3已从固定Mesa归档重建，实际Maxwell前端/SPIRV/NIR/IR3/asmroundtrip链编译5个独立原graphics stage；6个交叉pair严格3成功/3拒绝。新增C1 fragment保留原1792Bcode、256BC1及0x880control，实际FS native2432B；两显式offset能力specialization均通过，CB1保留并映射nativeUBO4，不猜用户CB1/T10/S0。其specialization仍是明确实验配置，未捕获真实游戏使用，不能自动默认选择。

此次复用IR3已有txs→GETSIZE、txf→ISAM/ISAML实现，修复桥接的sample/fetch/size资源和ConstOffset校验、semantic sampler mask及类型cast。原始SPIRV/编译优化顺序保持，组件分析在NIR clone上调用Mesa SSA/copy/constantfold工具；元数据显式may_write/store/direct_undef且all_paths_initialized_proven=false。packager移除自制SPIRV指针/LoadStore追踪，改用实际NIR接口和IR3消费compmask；仅组件覆盖必要条件，不声称证明所有路径初始化。原zw未写交叉pair现由compiler直接拒绝。

正常checker19个实际shadercase（5操作正例、12有效SPIRV负例、partial/conditional两例）、2原C1变体与5内存metadata破坏拒绝通过；archive15298匹配项双san和NDK29严格链接通过。source-only archive无RT/顶点/纹理尺寸模板，尚未接实际ProgramSetShaders或通用Draw。原64×64/2×2 artifact继续正确拒绝新增C1，不能冒充通用pipeline。image-query-normal-host-summary.json和graphics-archive-image-query-host-proof.json是HOST结果。

正常重建的原4CS与2原创转换kernel，bin/asm/spv/json/nativejson/nativeheader均实文件逐字节等同旧已测版本；compiler-fresh-artifact-equality.json记录6组对比。因此旧GPU资格保留原HEAD和原运行标签，未重复同一硬件测试，亦未把历史结果算作新C1资格。新的正常kernelcheck为texture-copy-image-query-v1，computecheck为a650-image-query-maxwell-check；后续build/cache须使用新闭包，禁止手改旧record。

## 全尺寸KGSL/AHB资格正常入口

正常builder新增--kernel-check/--fullsize-display-check与--fullsize-display-slots2/3，接受完整验证的新kernel闭包。每槽固定同IB四次重放、两代实际WindowSession/SC，source由CPU初始化、目标仅固定sentinel初始化；GPU写后等待Finish及slot重获/SC退休才CPU只读，保留实际AHB desc/pitch/FD/inode/offset/完整层extent、active/padding和code/UBO/job检查。截图在最后gradient CURRENT期间采集，HOME后确认释放，再读回。operation明确kgsl_ahb_linear_copy，非NVN Draw/游戏帧。HOST50负例、68协议/oracle检查、正常来源7测试及N2/N3实际NDK严格链接通过，root合并源与候选逐字节相同；游戏host清空该模式和header，未接资格shader。当前真机结果另记。

## 全尺寸显示实际通过（HEAD006472e）

A650上normal签名APK的N2与N3均通过两代窗口：总40次真实GPU submit/Finish和SC显示，746,496,000 active channel exact，code/UBO/IB/source只读及padding检查通过。四张CURRENT梯度实际截图各16点最大预设容差8通过；随后HOME释放，slot/SC retirement、destroy、AHB owner释放和最后严格pidof停止均成功。实际AHB2880×1620、pitch11520、完整FD/payload18,665,472字节、offset0，独立slot均有不同inode。N2/N3摘要分别a650-fullsize-n2-v2-summary.json和a650-fullsize-n3-v1-summary.json，四张原始screencap已归档。

40份18,662,400B tight RGBA均实际读取并逐hash核验，其内容恰为4种原创pattern。a650-fullsize-n2-n3-lossless-readbacks.zip按内容SHA无损去重保存四份，摘要把每次slot/generation/case映射到对应archive_member；不能把去重当作没有读回/用输入代替输出。本批是原创CS linear-copy+explicitAHB+实际SC出图，不是NVN Draw、不含真实游戏模块、不证明canonical GOB纹理的Window路径已经接通。

最初N2v1因系统Python缺PIL在collector开始即失败，finally严格停止成功；失败保留a650-fullsize-pillow-failure-summary.json，不改判。N2v2换用仓库现成venv/Pillow12.3重新运行完整两代，N3也用该环境。root补了Pillow前置检查，使缺依赖在ADB/安装/启动前失败；正常旧来源验证仍优先拒绝修改过的runner，测试须正常重建记录，不改旧records。

## 原 SDK 状态语义独立复核

直接核对本游戏 SDK53.314 的45个状态方法，恢复8族纯状态的默认值并纠正 multisample dither 默认应为1；补齐16个 packed sample locations、grid查询与Bind快照。grid取max(samples,rasterSamples)，0/1→4×4、2→2×4、4→2×2、8→1×2、16→1×1。实现/测试均不使用旧Metal作为规范或oracle。正常静态入口重提取45方法/6分支目标与探索结果一致；错误SDK指纹在创建输出前拒绝。原汇编只留.local，归档仅地址/哈希与标量解释。

HOST正常检查API450/color97/texture126各两套sanitizer及NDK29编译通过，48-word普通状态段断言未变。没有新增entry数量，也不宣称MSAA/位置绘制已可用；enabled路径仍明确ENOTSUP。动态command-state初始化未独立确认的值改标backend policy，advanced blend getter虽已读出但未实现。详见render-defaults-sdk-research.zh-CN.md与三份对应static/repro/host证明；无新GPU或真实游戏里程碑。


## NVN canonical 源消费者、标准 Window 与原游戏接入

此批把独立源资源转换接入 descriptor、Clear/Draw 消费和标准 NVN Window。canonical pool/offset/GPUVA 不变，目标GPU内部 shadow 与 AHB各自拥有 BO；每次消费者及 replay 读取当前 canonical，RT结果写回原池供 Buffer别名观察。CPU不做texel转换。完整提交引用、失败回滚及未知完成保活，见 source-consumer-window.zh-CN.md。

真实A650标准NVN Window两槽/三槽、各两代生命周期均通过：合计40次Present/GOB转换/Finish，746,496,000个active颜色通道与独立oracle严格相同，4张屏幕截图采样最大差0，guard/readonly/原源地址/重放/资源释放均通过。使用原创canonical GOB图样、NVNBufferMap/Flush更新，AHB仅READ，不是CPU填屏或此前linear raw-display探针，更不是实际游戏帧。实际40份readback保留lossless内容去重归档与原捕获SHA映射。各轮finally确认停止。

整合4case混合compute/graphics资格第一轮失败：真实submit1，Finish EDEADLK35，cleanup EBUSY16，资源按未知完成保留，无raw像素；不能据HOST通过视作GPU通过。首次Window资格的READ错误码接缝失败也保留，不覆盖；native READ现按真实session ticket合同校正，并排除未知转换。最终正常HOST检查 words32932/core8465/Window643各双sanitizer、真实Android完整backend/probe链接及负例通过；旧Window292回归单列历史指纹。

正常构建新增 --texture-bridge-check，真实DeviceInitialize成功后注册核验过的immutable kernel/FDL bundle。原3NSO从原输入正常重建及verify，57任务完整ARM64 Activity host+typed HLE+KGSL编译，实际依赖含consumer/window/AHB，ELF/源码审计排除旧renderer及CPU模拟器。真实D2R四次ProgramSetShaders、三次QueueInitialize和三次TextureInitialize均返回1，已越过此前TexturePoolRegisterTexture失败。最新第一个具名错误推进至 nvnWindowBuilderSetPresentInterval ENOTSUP95（after）；实参未有typed trace，不能猜成0。未证明真实游戏GPU绘制/派发/显示，游戏帧仍false，进程停止确认。见 a650-activity-source-consumer-v2-summary.json 与 source-consumer-normal-build-proof.json。

本批没有新增backend具名入口：仍336；入口覆盖不能代替完整参数语义。接下来并行推进：整族Window present interval的原SDK/Android语义；公开硬件源码支持的CS_UAV单状态位受控修复及混合消费者复测；shader-only archive与通用动态pipeline接入。Metal结构、固定尺寸、专用优化均不作为这些通用设计的依据。


## 通用动态pipeline与混合消费者的受控诊断

已落地独立shader-only recipe/device platform/dynamic encoder及完整源身份/query选择模块，正常fresh编译5stage/3pairs，HOST591720+942各双sanitizer、真实NDK链接通过；仍未接NVN Draw且无新pipeline GPU资格。详情dynamic-pipeline.zh-CN.md。不增加入口计数，Metal/Vulkan运行时和固定图像模板均未进入新路径。

CS_UAV漏位的正常6kernel重建证明code完全相同、PM4单字变化，但实际未加标记consumer仍失败。为填现有工具无法取得任意裸KGSL IB阶段的缺口，新增显式qualification-only离散CP进度标记；带标记v3完整4case像素/别名/guard验证通过，关闭标记v4同样EDEADLK失败，资源保留和停止分别记录。诊断成功不视为正式修复或真实游戏帧，不能靠有副作用的marker掩盖同步问题。下一步对照公开Gallium的MEM_WRITE→WAIT_MEM_WRITES→event/poll合同，定位短compute/graphics组合的排序差异；窗口interval整族及通用Program接线继续并行。


正常A6 producer再增加显式CP_WAIT_MEM_WRITES于completion sentinel重置之后，6kernel机器码/汇编不变、PM4各增加一个包；新产物原4CS间接派发8case全部通过且明确停止。uninstrumented mixed consumer v5仍Finish35，没有像素，不能视作该阻塞修好。独立SDK UBO审计与正常可复现工具也已入库，graphics API槽+3的直接方法/RELA/硬件字段证据取代fixture外推；compute高槽和reserved CB用途明确留未知。
