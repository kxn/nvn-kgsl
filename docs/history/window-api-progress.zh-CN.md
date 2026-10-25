> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 标准 NVN Window 与 Android 窗口生命周期真机验收

2026-10-23，独立分支 codex/nvn-turnip-study-20261023，仅新机 DEVICE_SERIAL，pipa / Adreno650。

标准 WindowBuilder / Window / WindowAcquireTexture / QueueAcquireTexture / QueuePresentTexture 以及 Acquire availability Sync 已在新机通过。双缓冲和三缓冲各完成两代真实 SurfaceView 窗口：显示、HOME 退后台、窗口销毁、恢复重建、再次显示和销毁。总计 40 次标准 NVN Draw 提交与呈现、655360 个颜色通道读回检查，四次最终屏幕图像通过独立原 shader 数学参考的 16 个内点检查。无 CPU 目标写入、无图像复制、无绕过 NVN 的 raw Draw。测试完成后 probe 进程已停止。

证据为 a650-nvn-window-n2.json、a650-nvn-window-n3.json 及各自两代 RGBA 与 screencap。每槽四个 case（红、半屏绿、全裁剪、RGB 渐变）；同槽复用命令 handle。两个 Acquire 路径交替使用，每代各 2N 次。回读只能在重新 Acquire 到已释放槽后或 WindowFinalize 安全退休之后执行；CURRENT 不允许 CPU 锁读。GPU 完成和合成器释放分别证明，成功后才释放 caller texture/pool 与 AHB owner。

两组第二代第一次截图均短暂未匹配，运行器在 150ms 后重试通过；没有删除失败截图或放宽容差。源 RGBA 四类全通道检查始终通过（前三类 exact、渐变每通道容差1）；屏幕采样仅证明所测16个内点、容差8，并非全屏逐像素截屏比较。已目视检查第二代成功截图，显示完整平滑渐变。截图前后的 held generation、目录、geometry 一致。所有尝试、样点、文件哈希均保留。

平台注册 exact VI token 身份再绑定真实 ANativeWindow，不对 token 强制转型或解引用。该次 Window probe 使用专门的宿主测试 token；后述 Activity 实测已接真实 D2R VI token，但尚未建立真实游戏 Window。独立 dma-buf 身份来自实际像素 FD 的 fstat，重复底层 backing 的重叠范围拒绝，即使 BO/GPUVA/owner 指针不同。票据 serial 进程单调，防止同一 window generation 重建 session 后旧 ticket 碰撞。

UI 只发关闭通知，不执行 NVN/GPU 或无界 join。单一 NVN owner 管理 GPU Finish 和 core 引用，独立 compositor worker 只碰平台会话；退出共享最多1500ms总 deadline，超时保留上下文和全部 pin，不能在回调或 GPU 使用中提前释放。producer Queue 在 Window 生命周期内被 pin。多 handle 提交先完整检查目标是否仍为 ACQUIRED；已入队、CURRENT、未 Acquire 或 closing 目标拒绝写入。availability Sync 仅表示 CPU 已证明可复用的 Acquire，不支持任意队列 fence/event。

实现新增22个标准PFN，项目现共有182个 typed native入口；这仍是声明的部分 ABI 53.105，不是原 NVIDIA 驱动版本或完整 NVN 兼容声明。当前 Window 资格仅 A650、2/3个完整64×64 LINEAR RGBA8外部AHB目标、interval1/full crop。普通游戏 DISPLAY pool、真实分辨率/格式、任意 shader/depth/状态和完整 fence 还需后续实现。测试帧不是 D2R 首帧。

主机证据分开保存：core282 checks、external owner218 checks、平台13类状态/并发/故障及真实fstat39 checks；Clang函数类型、ASan/UBSan与ARM64严格链接通过。独立运行器46个合成协议用例覆盖新鲜度、身份、generation、UID、SC snapshot、错误清理、像素和文件清单篡改；这些不算GPU通过数量。既有mode90/116与GPU_UNCACHED1075策略检查回归通过。生产Android始终链接真实session/platform；RAM fixture集中使用明确的失败后端，不进入运行时。

可复现入口：build_android_probe.py --nvn-window-check <schema3 graphics check> --window-slots 2 或3；run_android_probe.py 使用含Pillow的 ${D2R_REFERENCE_ROOT}/.local/venv/bin/python，锁定完整source/tool/APK指纹，写新nonce目录并验证安装包哈希。host核心与平台检查分别为 check_window_core.py、check_window_platform.py；运行器协议检查为 tests/window_lifecycle_protocol.py，所有输出选新.local目录。

真实 D2R 在 ece7c84 已推进到 pool-exact12-progress.zh-CN.md 所述的 TexturePoolInitialize：原始 ARM64 SDK/构造器/nnMain 已运行，完整原始 RomFS 验证；0x12池建立成功，真实描述符参数为 offset0/count15256。尚无真实游戏Draw。不能把CLI进程里的token/ANativeWindow数值跨进程传递，不能修改游戏逻辑绕过。

同进程宿主的第一步已经落地：独立 shared target 直接编译原36个 loader/HLE/platform 源文件、原宿主门和 typed native KGSL bridge，生成真正的 libdmhost_activity_kgsl.so。使用原始NSO经统一 rewrite-native.py --abi elfdesc --rules native --private-gates 1 生成 HOST_ABI2 模块。全新构建和正常 --verify 通过，167个源码输入、85项产物完整记录，原 swforce、原生ELF TLS和6项TLSDESC重定位保留；无旧renderer、CPU模拟后端或第二份core。该次构建是描述符角色补丁接入之前的构建证据，不能直接用于更新后部署。

原项目只增加 host-only VI token getter，以及仅 shared target 启用的 main 返回宏；四个原 VI HLE 入口与默认CLI退出行为保持。五个导出函数提供 exact token、attach/close、bootstrap检查及原SDK/构造器/nnMain入口。UI close只通知平台，不执行GPU退休；启动后重绑Surface明确返回ENOTSUP。41项RAM边界检查、两项原main宏分支检查通过，两种sanitizer独立复核通过。这些早期检查本身只证明构建/主机边界。现薄NativeActivity控制器已 dlopen 这一份shared host，并按后述真机记录在同一进程attach真实窗口、执行原模块；真实DISPLAY和游戏生命周期恢复仍未证明。

薄控制器与正常APK/执行工具现已接入正式路径 android/d2r_activity 和 tools/build_d2r_activity_apk.py、run_d2r_activity.py。控制器不静态链接另一份core，只dlopen已核验的shared host；单一detached owner等待真实resumed Surface，UI不执行GPU或join。UI/owner分离引用与ANW引用覆盖destroy并发；状态由fresh文件fsync/close后无覆盖原子发布，记录nonce、真实pid/uid/SELinux和process start ticks。最新42组实际控制器RAM生命周期/故障检查、26项runner分类/来源及读取协议检查通过；这些主机检查与后述Activity真机结果分别记录。

执行工具区分原入口返回与进程退出：Activity没有waitpid状态，process_exit_code始终未知；只有终止诊断和原进程死亡同时成立才标记unsupported边界，不从自己的force-stop推导exit78。run阶段须核对完整原始RomFS，模块必须canonical ABI2，与CLI ABI1分开部署。每次运行后停止本package并确认退出。源码或HEAD变化后必须正常重建shared host/APK，历史候选APK不能部署。本轮真实 Activity 分阶段结果如下；入口执行和测试帧、游戏首帧分别记账。

2026-10-23 同进程 Activity 真实执行：`a650-activity-sdk-v4.json` 与 `a650-activity-ctors.json` 均 `passed=true`、`stage_completed=true`、`original_stage_return_code=0`，运行后本 package 已停止并核实无进程。两次均为真实 `u:r:untrusted_app:s0:c234,c256,c512,c768`、APP_UID，分别记录独立 PID/start ticks/nonce；exact VI token 同进程 attach，generation1。原 ARM64 `_init_libc0`、`nnosInitialize`、`_init_libc1`、`nninitInitializeSdkModule`、`nninitStartup`、`_init_libc2` 均进入并返回；构造器阶段继续执行三个原模块 DT_INIT/init_array（main852、sdk115、subsdk0 44项），最终原入口返回。Activity 无 waitpid 退出码，`process_exit_code=null`；这些结果不等于真实游戏初始化、Draw 或恢复成功。

`a650-activity-nnmain.json` 随后验证完整原始 RomFS46文件、8,973,547,258字节及全清单/逐文件 SHA，真实同进程 SDK/构造器/`nnMain` 已执行。`nvnDeviceGetImageHandle(id=1048575)` 原样返回 `0xfffff`，下一处具名终止为 `nvnProgramSetShaders; errno=95; phase=after`。终止诊断和 force-stop 前原 PID/start identity 已死亡共同构成 `unsupported_boundary=true`，`passed=false`、`stage_completed=false`、`first_frame=false`；不把诊断中的 exit78 伪装成 Activity 已观测退出码。当前只确定 ProgramSetShaders 明确拒绝；具体 count/stage/code/control 与离线注册资格的关系，仍须真实参数观察和原资源元数据核验，不能猜成功。尚无真实 D2R Draw、Window 出图或游戏首帧。

三阶段使用统一原始 ExeFS → `rewrite-native.py --abi elfdesc --rules native --private-gates 1` 生成的 HOST_ABI2 模块，以及 fresh 正常构建/核验的 shared host 和独立薄 controller APK。宿主记录为 `.local/nvn-native/d2r-activity-9f67-host/build-record.json`，原模块来源 checkout `9f67bb9d07669e0d11f03948a8d67e097973fc4c`；修复后 APK 记录为 `.local/nvn-native/d2r-activity-9f67-apk/build-record.json`、SHA256 `b8670b3b99bbc7afdc357c2c27ed403dc3c1dac815b4281deed8568e9e363f3c`。每份执行证据内嵌完整记录及安装/模块/源码哈希，禁止用本路径的历史缓存覆盖历史证据。只链接36个原host/HLE/platform源、原loader/gates、typed bridge和一份native core，无旧renderer、CPU模拟或wrap/preload。当前182个PFN仅表示已有typed解析入口，不代表任意参数或全部NVN能力；unsupported参数仍立即具名截断。

失败尝试均保留：首次 `a650-activity-sdk.json` 在日志创建后报 configuration errno13，未产生 start/attached/result，也未执行原模块；旧 `adb exec-out cat` 对不存在文件返回0且错误文本在stdout，导致JSON解析错误，不能当模块失败或成功。改为 shell-v2 `shell -T`，要求真实 remote exit status，只有精确ENOENT是未就绪；权限/传输/部分读取均失败，不把错误文本保存成JSON。v2仅因ADB features每行一项而被逗号解析误拒，尚未部署/launch；改为兼容逗号及空白的feature列表。该历史证据的 `adb_invoked=false` 标记不准确，实际做过只读features查询；后续修正调用标记，保留历史原文。

v3在新版细分日志中精确到 `configuration phase=start-publication errno=13`：真实context读取与identity validation已经通过，start仍未发布，原模块未进入，超时不算通过。旧硬链接发布是可能原因，但无AVC证明，不能写成已确证SELinux规则。状态发布现使用 fresh O_EXCL临时文件、fflush/fsync/close、真正 `syscall(SYS_renameat2,...,RENAME_NOREPLACE)` 原子无覆盖发布；无普通rename覆盖、存在性检查竞争或失败fallback。v4实际通过证明本机当前流程可用。context保留原厂当前线程 `/proc/thread-self/attr/current` 读取，仅ENOENT允许task/gettid回退，权限错误不伪造身份；配置失败另记录publication-open/fsync/rename等具体阶段。

生命周期边界继续保持：当前 pause/destroy 只关闭compositor registry并唤醒Acquire，不请求原游戏存档、不证明原游戏线程/GPU已静止；启动后Surface重建明确ENOTSUP。同进程SDK/构造器及`nnMain`到具名shader阻塞的通过，不能当成D2R进后台存档、重新绑定真实DISPLAY、恢复世界或恢复帧的资格。后续必须接原HLE存档/焦点顺序与真实GPU退休、同一游戏Window的generation重建合同，不能只清除stop/started标志或重跑nnMain。

普通caller backing至AHB的独立GPU copy资格已通过 a650-copy-qualification.json：保持原CPU_NO_ACCESS pool、offset4096、GPUVA和backing，标准NVN Draw后由同一KGSL队列执行原Mesa Gallium来源的CP_MEM_TO_MEM。两种顺序（同队列直接接续／producer Finish后）×四case共8次NVN提交和8次copy，131072个RGBA通道、786592字节只读命令及32608字节命令guard检查通过。源色缓存清理由已验证原graphics尾部在CP读之前完成；copy自身invalidate/wait及KGSL Finish后仅READ锁AHB。源CPU读/导入后写、目标CPU写、raw draw均为0，owner pin/unpin各8次，cleanup0，进程已停止。8份RGBA和完整fingerprint/APK/输入来源均归档。

该copy尚未成为Window生产路径，也未呈现屏幕或执行游戏模块。资格专用import/submit wrapper只观察并原样转发，不进入真实host；CPU原匿名页与新AHB的backing来源证明不别名，不以指针/GPUVA不同代替物理证明。当前每4字节一包、每64²帧98324字节命令，不是可推广的全分辨率高效方案；只限A650/64×64/pitch256/LINEAR RGBA8，源GPU边界guard与padded stride未资格。后续生产接入须用core内部正式view/pin，保留原游戏pool/offset/GPUVA，并独立实现Window提交和合成器退休。

下一轮源码补强已接入：shared host增加第六个typed notify_focus接口，直接调用本来已编译的原hle_oe.c::dm_focus_set，controller的实际resume/pause向原状态及消息15队列投递，仍由原game HLE查询/消费；没有新队列、guest调用或额外原模块闭包。builder严格核验六个版本化导出，旧五接口host不能冒充新控制器。65项真实原HLE前端检查（两种instrumentation）、44项controller状态/故障检查、4项导出正负检查及NDK通过；stop后的resume不发送假focus1。该通知不代表存档、停绘或恢复ack，pause仍沿目前terminal close；真机新构建待执行。

首次ProgramSetShaders增加typed signed count及原pointer身份日志；生产host复用原platform_elf.c::dm_self_read，全长安全复制最多6项NVNshaderData，仅观察已注册program所属live pool的flags/policy/offset/remaining（remaining不充当代码长度）。control magic匹配原98761234 profile后，只有固定0x880完整安全读且两次magic一致才记录原始metadata/hex；不读取code或C1，不改参数、返回、errno或sticky，不注册fixture。默认独立core不依赖dm reader。OFF11、ON23、core118项各两种instrumentation及NDK严格编译/链接通过。真实gamebytes只保留ignored私有日志，git只归档脱敏metadata与完整原证据哈希；该观测不声称编译/执行成功。下一轮统一正常重建后用原nnMain获取输入，决定是否复用已验证离线CS并保持完整code/control匹配。

acfd0fc后的首次输入采集 a650-activity-shader-input.json 虽已核实完整46个RomFS文件，但在原模块入口之前 generation0/host_resident=false 时被controller以ECANCELED125取消，没有shader观察，不能算新的游戏执行。现将首次owner启动前的pause与初始/无关Surface销毁保留为等待状态；owner已启动后的真实关闭仍终止，不伪装游戏恢复。新增初始Surface替换和无关旧Surface销毁检查，24组×两种instrumentation通过；正常APK重建后的 a650-activity-preentry-sdk.json 真机原SDK再次通过并严格核实退出。首次取消的确切Android回调来源未直接观测，不能把成功重跑当成已证明某个OEM原因。

有限离线compute cache provider现加入正式构建路径 tools/offline_compute_cache.py；Activity/CLI normal builder可显式 --compute-cache-check，默认不启用。它复用已验证Maxwell/IR3产物，校验完整source/tool/compiler/native机器码、query/reflection/control/C1empty/chip/schema及实际编译依赖，生成只在ignored产物中的immutable表；原DeviceInitialize成功后注册。无四个ID白名单、游戏地址、oracles、raw GPU调用或动态compiler，ProgramSetShaders仍完整code/control匹配，未知输入保留ENOTSUP。正式HOST双instrumentation通过2个device生命周期、16次完整匹配、40次miss及初始化/注册失败、缓存/机器码篡改等负例，NDK严格链接通过。这是生产缓存接线实现，不是实际game shader通过；后续将在真机用显式四个已编译原CS的check同时验证真实命中与原调用推进。

实际部署前纠正一处关键输入范围：原main的container v5声明并由1741174路径分配/复制0x898字节caller control blob（2200B），旧0x880只是截断前缀。90914c9的prefix provider仅构建，未部署；当前provider明确拒绝旧profile和把截断输入只改标签的伪完整profile。已从同一原始NSO重新验证三段、容器signed-relative范围及四个完整SHA，正常生成real-shaders-full898与a650-full898-maxwell-check。四项.bin/.spv/.resources.json/.native.json/.native.h与既有资格产物逐字节相同，无需重写翻译器或声称新GPU结果。完整匹配的尾24字节每字节变化均拒绝；正式HOST每种instrumentation552项（16matches/232miss）及NDK通过。观察器按2200B完整安全读取，最后chunk24B，仍不把caller copy span当成已知NVIDIA driver内部读取长度；短读无hex。矛盾的stage return0与NVN终止诊断也不能标passed，runner29项检查通过。下一次真机使用新的完整check，真实shader是否成功仍待执行。
