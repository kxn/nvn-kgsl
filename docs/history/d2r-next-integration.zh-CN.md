> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# D2R native KGSL 接入：真实执行进度与后续实施

最新实施已通过真实flags0x100、原生设备初始化及部分ABI版本查询，当前实际边界推进到DeviceSetWindowOriginMode。完整分阶段证据见 [设备启动增量](device-bootstrap-progress.zh-CN.md)；下面保留此前阶段记录，不能把历史SetFlags缺口误读为最新状态。

工作树 `${LEGACY_D2R_WORKTREE}`，2026-10-23。本文保留最初只读审计并追加真实实施结果。最初审计仅检查原 host 的隔离边界；随后的构建和真机执行记录见下节。未读取旧 Vulkan 渲染实现或 runtime。下列历史行号相对此工作树当时源码。

后续实施增量：本审计要求的 typed 调用错误截断层已实现并单独编译验证，见 [调用桥构建记录](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `d2r-typed-bridge-build.json`；未随公开仓库提供） 与 [主机行为检查](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `d2r-typed-bridge-check.json`；未随公开仓库提供）。476个已声明入口使用真实PFN签名转发，1个resolver始终留在adapter，46个未知ABI保持fatal；当前154个driver入口全部有typed包装。调用前后的sticky错误带API/errno/phase退出78，成功返回值原样保留，不能把合法false/zero当错误。原HLE_FORCE的24B swforce记录仍保留。后续已引用原36份host/platform/HLE源，连同typed桥和5份独立驱动源组成42编译单元的CLI HOST_ABI1目标。其已在新机普通UID实际完成SDK初始化、构造器，并从原nnMain进入独立NVN，首个具名缺口为DeviceBuilderSetFlags/Exit78；未产生真实游戏帧。下文保留当时只读审计，当前实施与运行证据见紧随的增量记录。DeviceFlags、版本、GPU-only pool、queue等语义，以及真实host串行所有权和调用点诊断仍待补全。

## 2026-10-23 实施与真实执行增量

- `nvn-native/adapters/d2r/host/CMakeLists.txt` 直接引用原loader、ARM64入口/宿主门、线程/TLS、系统服务HLE和Android平台源；排除旧graphics boundary、旧NVN实现及图形renderer。没有复制原loader，也没有CPU模拟器。实际编译、link closure、AArch64 PIE、原swforce、16KiB LOAD及仅libc/m/dl依赖已核验，见 [构建记录](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `d2r-host-build.json`；未随公开仓库提供）、[链接审计](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `d2r-host-link-audit.json`；未随公开仓库提供） 与 [缓存核验](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `d2r-host-verification.json`；未随公开仓库提供）。
- 三个原始NSO经同一 `rewrite-native.py --abi elf --rules native --private-gates 1` 入口生成并verify；原始输入/段/工具/规则/上游/选项和全部产物均有哈希。没有在加工binary上叠补丁。CLI保持HOST_ABI1，不能和后续APK的TLSDESC模块混用。提交改变checkout后，后续部署必须通过正常构建入口刷新canonical provenance，再verify；不可手改manifest。
- APK原样携带已核验CLI PIE；安装后的APK、nativeLibrary文件、所有部署模块均在设备再次核SHA。通过 `run-as org.nvn.nativeprobe` 在APP_UID/SELinux `runas_app` 中执行，**这是普通UID命令行上下文，尚非原D2R Activity/JNI接入**。不把它与前面的独立窗口资格调用者混成游戏已出图。

| 实际阶段 | 新机观察 | 证据 |
| --- | --- | --- |
| SDK初始化 | 原6段SDK初始化调用均返回，host exit0 | [SDK](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-sdk-init-v2.json`；未随公开仓库提供） |
| 构造器 | 原DT_INIT/init_array流程完成，host exit0 | [构造器](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-constructors.json`；未随公开仓库提供） |
| nnMain，空RomFS诊断 | 实际进入后，TACT缺配置，正常返回host0；不是游戏启动成功 | [原始记录](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-run.json`；未随公开仓库提供）、[不可变日志重新分类](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-empty-romfs-analysis.json`；未随公开仓库提供） |
| nnMain，完整原RomFS | BUILD94012/CONFIGRelease，完成资源初始化并实际调用独立NVN；DeviceBuilderSetFlags具名截断，exit78，首帧false | [实际NVN边界](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-original-romfs.json`；未随公开仓库提供）、[完整日志](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-original-romfs-stderr.log`；未随公开仓库提供） |

原RomFS共46文件、8973547258字节。为避免无线重复传8.4GiB，只读复用了同机旧app的 `files/runtime/RomFS`，逐文件与本机原始目录SHA一致后才运行；没有修改或停止源app，也没有复制它的游戏binary、存档或玩家状态。目标资源与诊断存档在本probe私有 `files/d2r-host` 下独立隔离。所有本次host PID均已确认退出，无需发信号杀游戏。

运行工具修复了两项实际发现：含 `=` 的Android安装路径不能由toybox env直接解释为命令（改为shell赋值前缀）；host正常返回0也可能是guest资源初始化失败（单独分类并拒绝标游戏成功）。普通UID安装目录的legacyNativeLibraryDir形式、SHA完整性、只终止本次PID、源读取失败即阻止启动等另有可重跑 [host mock合同检查](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `d2r-host-runner-host-check.json`；未随公开仓库提供）；这些不是新增真机或游戏通过数量。

下一步先给typed边界补必要标量/原module+offset观察，再实质实现DeviceFlags受限合同，不mask 0x115为0x100，不假接受未知debug能力。`CONFIGRelease`不能代替实际w1参数证据。API版本0/1尚无充分依据，不能直接把原HLE未核的53.311作为完成证明。随后按真实调用顺序处理GPU-only/uncached pool、queue预算/所有权、shader cache和图形状态。受限flags、原调用点及版本依据边界已整理为 [设备启动实施笔记](device-bootstrap-next.md)。

屏幕AHB链路及2轮生命周期已另行实测通过，见 [显示进度](texture-progress.zh-CN.md)。但普通NVN texture当前来自caller WB pool，尚非AHB；不能把资格用AHB直接冒充任意游戏pool背后的presentable资源。后续要验证合法AHB-backed创建/注册或native GPU copy/blit及2/3buffer Acquire/Present回收，详见 [窗口下一步合同](nvn-window-next.md)。

## 结论

下一步应先补 **真实 host 构建接入 + 对所有已实现入口的 typed fail-closed 观察层**，然后按原 ARM64 boot 中已确定的依赖补通设备/资源/queue。当前 bridge 只解决名字缺失，不能安全处理已实现函数的参数缺口。即使补入 `DeviceBuilderSetFlags(0x100)`，真实 boot 随后的 `DeviceGetInteger(0/1)` 仍会 `ENOTSUP`、不写输出；game 不检查本 driver 的 sticky error。再越过设备初始化后，首个真实 memory pool 直接请求 `0xa1`，也超出现有 pool 资格。这些均发生在首帧/完整 shader cache/窗口之前。

新图形/纹理 probe 的真实硬件证据已由 root 产生：`evidence/a650-nvn-graphics.json` 和 `evidence/a650-nvn-texture.json`，后者普通 APP_UID、chip0x06050002、4主case/5提交/65536通道/cleanup0/passed。它们执行的是原创 NVN caller 和原始 shader 翻译结果，**没有执行真实 ARM64 游戏模块**。本轮研究也没有新增真实游戏执行。

## 1. 名称集合与原始输入证据

当前 `src/nvn_entries.inc` 154名字；原94012 loader 523加载名字；`nvn_slots.tsv` 216名字至少一个静态槽使用候选。交集99，静态未提供117；154全部属于523，55个实现名字不在216集合中。以上是静态名称集合，不是API语义覆盖、动态调用数或首帧必需数量。原 `dm/gfx/tools/nvn_slots.py:43` 起近似回溯每个槽的读取，不能把 first_use 地址当时间序列。

原始 main `${D2R_REFERENCE_ROOT}/work/data/switch/3.3.94012/Update/ExeFS/main`：SHA256 `55fd909bb7230b002357e5b0d8499c11f06888f345c49e69eb4734df44a27d0e`，build ID `db0507f36b84200ac05f75d76ed5026f903e6c90000000000000000000000000`。本轮调用现有 `extract_nvn_names.py --verify`，完整重提取/三段哈希/名字与静态槽/工具及产物核验通过：`parallel/d2r-next-names-verify.json`。没有保留 raw binary。

原段 SHA：text `3f6001efbd7412c5a48c27a2b9514dc9178c3b69dd01007edd0d7456e09819a5`，ro `e913be45ae39360093c823bb670e116461debef8592b61588c7f9ee8bc71ab0c`，data `6ebecf913070869f14f80f2908270b4891ddf969604507fa32e588fb78e93fd8`。另外用原 `dm/tools/nso.py` 在临时目录解压并以 Capstone 只读核查下述短函数/常量表，临时原始镜像已自动删除；这是静态分析，不执行/解释 ARM64 指令。

## 2. 真实启动的确定序列，及每项剩余工作

下表 PC 为原始 main offset，非本轮运行捕获。loader `0x1739290` 连续加载全部523名字，不检查NULL；`0x173cb44` 从 `nvnBootstrapLoader` 得到 GetProcAddress，再 `0x173cb50` 用 null device 加载第一张表。

| 原始调用 PC | 确定参数/作用 | 当前状态及最小合同 |
|---|---|---|
| 0x173cb74 | DeviceBuilderSetDefaults | 已有 |
| 0x173cb88 | DeviceBuilderSetFlags，w1=0x100或0x115 | **名字缺失，仍是第一确定阻塞**。须保存/读取flags，按已支持功能验证；不能无条件接受debug位 |
| 0x173cb9c | DeviceInitialize | 已有KGSL实现；其返回值在本段没有检查。open失败不能返回game后继续使用无效device |
| 0x173cbac | DeviceGetProcAddress重新加载523项 | adapter必须再次接管，不能泄漏raw resolver绕过诊断 |
| 0x173cbd0（条件） | DeviceInstallDebugCallback，cb=main+0x174936c、user=x21、enable=1 | 当前缺失；与下面相同w20配置位控制。回调ABI在public头1116；不能仅存指针冒充validation已实现 |
| 0x173cbe8 / 0x173cbfc | DeviceGetInteger(0/1)，out为game栈局部 | **函数有名字但参数未支持**。`nvn.c:348..359`只支持descriptor大小/reserved/maxpool六项；0/1为API major/minor。未支持时sticky ENOTSUP且不写out；当前bridge将让game继续，须先加typed截断层 |
| 0x173cc10 | DeviceSetWindowOriginMode(1) | 缺失；public头383..391确认UPPER_LEFT/NEAR_IS_ZERO。现有offline profile已选择upper-left/zero-depth；只能在验证与实际pipeline一致后接这两个setter，模式0尚不能假接受 |
| 0x173cc24 | DeviceSetDepthMode(1) | 同上 |
| 0x173cc3c 起 | 6次进入内存池 helper0x1749728 | **首个type0→flags0xa1，本来就会被现有MemoryPoolInitialize拒绝**；见第4节 |
| 0x173cdf4 / 0x173ce90 | TexturePoolInitialize / SamplerPoolInitialize | 新对象基础已实现，32B槽/256reserved、真实snapshot与storage引用；须先让底层真实pool flags/arena成立 |
| 0x173cec4 | DeviceGetImageHandle(0xfffff) | 当前名字缺失；本段仅生成unused image sentinel，后续BindImages尚无实现；不能把独立纹理采样通过写成image load/store支持 |
| 0x173d3cc..0x173d4a0 | QueueBuilder/QueueInitialize链 | 多个builder接口仍缺，见第5节；不是设备setter之后即可首帧 |

`0x173cb54` 从配置对象+0x2c读取w20；同一个w20在 `0x173cb64` 选择flags，又在 `0x173cbb0` 决定是否安装debug callback。w20=0→0x100且跳过callback；非0→0x115且安装。**实际用户配置值没有本轮运行证据，不能改game配置内存/补地址强迫走0x100路径。**

## 3. Flags 0x100/0x115 每一位

public重建头 `third_party/nvn/nvn.h:335..346`：

| bit | 名称/意义证据 | 当前能力与下一步 |
|---|---|---|
| 0x100 | ENABLE_SEPARATE_SAMPLER_TEXTURE_SUPPORT | 已有 separate handle、各自独立的stage-local绑定域、typed CB2源映射、pool identity和enqueue版本解析；原FS SeparateTexture10/Sampler0已通过NVN真机probe。这使精确0x100分支具备实质基础；仍限已资格格式/映射/selector0..31，不等于所有separate资源能力 |
| 0x1 | DEBUG_ENABLE，别名DEBUG_ENABLE_LEVEL_2 | 尚无完整debug database/回调/验证级别语义；拒绝或有界诊断，不能声称支持 |
| 0x4 | DEBUG_DRAW_VALIDATION，别名LEVEL_3 | 当前严格profile参数检查只是自身资格门禁，不证明原厂该级别完整行为 |
| 0x10 | DEBUG_DRAW_VALIDATION_HEAVY，别名LEVEL_4 | 同上；未核其它heavy检查及callback触发契约 |

0x115正好是0x100|1|4|0x10，**没有未知bit**；未知的是debug语义充分性。原 `hle_nvn.c:195`只保存flags，不能作为debug功能已完成的证据。合理的下一实现是明确保存builder/device flags、拒绝其它位，并先只承诺0/0x100的已资格profile；0x115需独立实施/验证，或在初次真实调用中明确退出。SetFlags是void，单写last_error不够（第6节）。本轮未修改这些接口。

API版本0/1也不能照搬原HLE `g_devinfo[0]=53,[1]=311`：`hle_nvn.c:203..206`已标注该版本未核。版本声明应与选定ABI兼容/profile合同一起定义；不能为越过boot写任意常数，不能让未知out继续流入game。

## 4. 比名字更早挡住真实启动的 memory flags

原 helper `0x1749728` 把w3保存到w20，在 `0x1749804..0x174980c` 查ro常量表 `main+0x1d7ba60`，再 `0x1749824` 调用MemoryPoolBuilderSetFlags，`:9838` 调用Initialize。该8-word原始表本轮已直接读取：

| helper type | 实际flags | public枚举解码 | boot直接调用 |
|---|---|---|---|
| 0 | 0xa1 | CPU_NO_ACCESS | GPU_CACHED | COMPRESSIBLE | **0x173cc3c，第一个** |
| 1 | 0x22 | CPU_UNCACHED | GPU_CACHED | 后面的0x17493c4 helper亦使用type1 |
| 2 | 0x64 | CPU_CACHED | GPU_CACHED | SHADER_CODE | 0x173cc54，当前flags profile可接受 |
| 3 | 0 | 特殊CPU-only分支 | 两个helper都显式跳过NVN pool创建；不把它误记为game需要flags0的NVN池 |
| 4 | 0x21 | CPU_NO_ACCESS | GPU_CACHED | 0x173cca0 |
| 5 | 0x22 | CPU_UNCACHED | GPU_CACHED | 0x173cc6c |
| 6 | 0x14 | CPU_CACHED | GPU_UNCACHED | 0x173cc84 |
| 7 | 0x12 | CPU_UNCACHED | GPU_UNCACHED | 0x173ccbc |

当前 `nvn.c:173..185` 只接0x24及可加SHADER_CODE0x40，故即使resolver名字全齐，0xa1/0x22/0x21/0x14/0x12仍失败。下一资源工作必须确定原caller真实storage地址/范围/CPU写入/alias和GPU缓存合同；实现CPU Map限制及正确cache模式，不能把各种flags全当cached import。COMPRESSIBLE是否仅许可、是否要求额外布局/metadata，目前公共名字与原helper不能确定；先保留ENOTSUP，不宣称“用linear禁UBWC即可兼容”。KGSL已验证的WC修复不等于这些NVN flags已全部资格。

另外，helper `0x1749728` 通过原game allocator按0x100000对齐分配backing，而desc子区按32B槽计数；新driver不能再为方便改变game分配或把API32B槽改成Adreno64B。

## 5. Queue及第一批后续接口

确定queue链：SetDefaults→SetFlags(0)→SetDevice→ComputeMemorySize(0x40000)→CommandMemorySize(0x10000)→ControlMemorySize(0x4000)→FlushThreshold(0x10000)→GetQueueMemorySize→原allocator4096对齐分配→SetQueueMemory(ptr,size)→QueueInitialize。各call PC已在第2节范围内，准确slot见 `dm/gfx/nvn/nvn_slots.tsv:38..46`；原始`:d408/:d41c/:d430/:d444/:d454/:d48c/:d4a0`。

原非Vulkan `hle_nvn.c:654..673` 保存这些字段，GetQueueMemorySize将三块之和4KiB取整，此输入为0x54000；注释明确原Metal host不使用这块内存。**不能复刻“不使用”当native合同**。可以为KGSL后端定义并实际使用有界caller arena/CPU bookkeeping和容量检查，或以证据证明哪些参数是可选预算；必须明确alignment、ownership、overflow、Finalize以及flush阈值作用，不能setter全保存但无消费者。

之后仍有MemoryCallback/arena链、同步/事件、format/block-linear/packaged layout与view、资源copy/update、完整shadercache miss、fixed state、DrawElements、Window/Present。静态117缺名字可作索引，不能当完整首帧顺序。当前输入资格只有固定原始CS和两个VS/FS pair，不允许用这几个程序冒充完整game shader集。

## 6. 必须先修的 bridge 观测空洞：已实现API也可能不支持参数

当前 `adapters/d2r/host_bridge.c:41..52`：已知名→driver函数非NULL则直接返回raw函数；只对缺名给具名fatal thunk。`nvn.c:80..83` 的sticky error仅由测试caller读取，game不知道 `nn_nvn_error`。真实DeviceInitialize返回值在本段也未检查。因此name coverage并不是安全接入的门禁。

**首选最薄方案：只在D2R adapter生成typed forwarding wrappers，不改这次已GPUqualified的driver。**

1. 从现有公开PFN解析/已验证523名字metadata及154 ENTRY集合生成包装器；154都有public真实签名。forward实参必须显式从参数列表生成，void/标量/函数指针/小aggregate返回各按真实C类型；保留 `check_wrapper : PFN = wrapper` 编译赋值检查。不把所有函数转成`void(*)(void)`再调用，不搬Apple NVNboolean宽化。
2. adapter lookup对已实现名字返回wrapper；每次wrapper调用前检查是否有旧sticky错误，若有即诊断并退出，不能盲目clear掩盖来源。调用raw PFN后立即读取error，非0时打印函数名/errno/phase并Exit78，不把未初始化out参数返回game。只有成功时原样返回值。driver自己的NULL和sticky契约不变。
3. 禁止“所有NVNboolean false都fatal”：合法查询将来可返回false。只按已知Initialize/SetShaders等返回合同补bool失败诊断；本阶段以sticky error作为统一失败证据。
4. 始终特殊包住GetProcAddress，含第一次NULLdevice；任何路径不得把raw GetProcAddress泄漏到game。未知名字/缺实现继续现有具名边界。
5. wrapper带host TLS的调用名/调用序号/TID/嵌套深度；入口取 `__builtin_return_address(0)`，用原 `module_of`（`dm.h:53`, `loader.c:133`）解析module+offset，失败可对应main+0x173cbe8等。不要为日志解引用未知CPU/GPU指针；仅打印已声明的标量（如DeviceFlags、what、pool flags/size）与对象稳定ID。
6. 初次接入强制串行owner；若多个game线程并发调用全局裸object/handle表，要先适配串行锁/明确重入拒绝，不能把probe单线程资格推广到game多线程。builder side table的stack地址重用/跨kind冲突、opaque对象按值复制也是独立ABI待修项。

替代/后续最小hook：driver设置可选observer，`error(e)`第一次置sticky时发出诊断；adapter TLS提供当前typed入口名。若observer `_Exit` 可在API内部立即停下；若只返回，则仍需post-call检查。observer必须不能重入NVN/锁/分配重试，且公开注册/注销和host TLS寿命。**单有callback(e)而没有入口上下文、或单有调用名而不检查error，都不足。** 这需要下一轮明确修改core后重新资格，本轮不动frozen三文件。

未声明原型的46个加载名仍不能伪造成功；其中静态集合仅 `nvnTextureGetRawStorageClass` 命中，后续要独立重建ABI，不能以generic fatal边界声称已实现该API。

建议故障记录：schema/version、adapter/driver hash、original module hash/buildID、host ABI、stage、call sequence、function、module+offset、TID、errno、已知标量参数、whether GPU submit attempted/accepted/unknown；每条flush后显式退出。没有新增计时器/采样器，不替代现成profile工具。

## 7. 原loader路线与可复现构建入口

- `dm/host/hle.h:40` 的HLE_FORCE将24B记录放swforce；`loader.c:149`初始化并拒重复，`:162`按名命中，`:285`在SDK定义前优先接管，再写JUMP_SLOT/ABS64/GLOB_DAT。保持原loader/离线改写/x18/TLS/SVC宿主门，绝不新起CPU执行器或Horizon服务。
- 现成 `host_bridge.c:75` 已提供HLE_FORCE；`nvn-native/CMakeLists.txt:28..33` 是OBJECT target且 NN_D2R_HOST_HLE=1，适合最终host直接链接保留swforce。它本身**尚未连接真实host**。排除 `dm/host/android/graphics_boundary.c:6` 和旧 `dm/gfx/nvn/hle_nvn.c` 的同名hook，避免loader拒绝重复。
- 从 `dm/host/android/CMakeLists.txt:20..29` 的NATIVE_CORE和dmhost_native原HLE源开始组独立 `dmhost_native_kgsl`，替换单个graphics boundary为bridge OBJECT，链接nvn_kgsl。不要继承旧图形目标/整个JNI；现有host CMake后半无条件配置另一后端，应以可选区段隔离或独立CMake入口**引用原源文件**，不是复制loader。`build-android-native.sh`目前cmake --build全部target，不适合直接当“只nativeKGSL”的构建证明。
- 命令行原入口 `dm/host/android/main.c:58..107` 已提供 `--sdk-init`、`--constructors`、`--run`：load三模块→guest_thread_init→svc main线程→原SDK初始化→构造→nnMain。实际执行由ARM64硬件完成；不能用自编NVN probe替代这些阶段。
- 原NSO生成统一入口 `scripts/dev/rewrite-native.py:51`：`--source ORIGINAL_EXEFS --output IGNORED_OUTPUT --abi elf|elfdesc|elfdesc-gate --rules native|touch-run-v4`，再同选项 `--verify-only`。`:80..123`覆盖pinned upstream、原始NSO/hash/buildID、当前规则/工具、选项及全部产物；`:125..151`缓存不完整时从原NSO重建；共享manifest也核验，不能在patched.bin上继续patch。
- 上游pin是 `scripts/dev/upstream-source.json` revision `afad65c895386a96dfb863f2317e41d61222a5a9`，upstream rewriter SHA `c9a0c7574547ab945e4648bd6664e86fa4a77de12253a55e7b464232b2cdf274`。本轮只读取，未调用rewrite/audit，因为该入口内部执行git查询且本任务禁git；未来由root按授权执行，不绕过它自行拼产物。
- APK须与原动态TLSDESC/HOST_ABI2或3一致（`loader.c:213..222`）；CLI IE须HOST_ABI1。host真实TLS和Android x18保持原门约束（`entry.S:139..155`）。不可把IE产物直接塞TLSDESC宿主，不能调用SDK原bootstrap进入Switch GPU驱动。
- `build_d2r_bridge.py --nso/--ndk/--out` 可独立重建/verify桥对象+driver归档，检查AArch64与swforce，记录original input/source/tool/options/artifacts；该工具明确不建host/game/APK。历史bridge-cmake记录旧driver，不能把旧归档当154入口；要用新输出重新生成，不篡改旧记录。

## 8. 最小可观测验收阶梯与未确认项

1. 先完成typed观察层和独立host链接；构建记录必须包含bridge OBJECT、core/host源指纹、rewrite-manifest与原NSO全部身份。静态检查只链接原host+nativeKGSL，没有旧renderer/runtime/CPU模拟。
2. 在root选定应用UID和A650上，保留原 `--sdk-init` / `--constructors` 分段；`--run`沿真实nnMain，首次可以明确停在 `DeviceBuilderSetFlags` 的具名边界，并记录实际0x100/0x115。这一条才是**独立后端真实ARM64 game caller到达入口**的证据，不是首帧。
3. 实质补设备flags/profile、版本属性、origin/depth后，再受控推进至第一个 `MemoryPoolInitialize(flags0xa1)` 并完整记录storage/size/owner；随后才按资源flags、queue arena及后续观察到的实际需要推进。不要先运行到随机NULL或让sticky错误之后game继续。
4. GPU first-submit、离屏正确像素、Android buffer/window/present、首个真实game frame、后台存档/Surface恢复分别验收。固定fixture GPU通过、154名字、构造器完成，都不是真实首帧替代品。

未确认：真实release/debug配置值；原NVN debug validation/callback全合同与版本号；真实进程多线程和builder/texture按值复制/Finalize用法；0xa1 COMPRESSIBLE许可程度与布局；完整game真实shader输入及C1/其它stages；第一帧动态API/format/attachment序列；VI token与ANativeWindow/Android buffer及fence映射。继续复用既有原host日志/静态元数据/编译器资源反射；不足仅补稳定参数/资源范围诊断，不新造计时/profile工具，也不读旧Vulkan实现来填答案。

冻结源码核验（本报告不修改）：
- `nvn-native/src/nvn.c` SHA256 `a06e3b829c597ea7f0547de7b1e1edfdfd1eaee4b0537007f690536626893bf9`
- `nvn-native/src/nvn_entries.inc` SHA256 `37d2dca141693bdfd27fe17a712d8f3f9c568cbd468e6349cbe159c58a3efef3`
- `nvn-native/include/nvn_native/nvn.h` SHA256 `7c14b6b0560af82f95fdb132cb38a0524acb640712661a348956d9739580b7b4`

静态集合重新计算：{"loaded_names": 523, "implemented_names": 154, "static_candidates": 216, "intersection": 99, "static_missing": 117, "implemented_without_static_use": 55}。
