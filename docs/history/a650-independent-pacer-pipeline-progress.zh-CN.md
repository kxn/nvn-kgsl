> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 独立 NVN 语义、窗口调度与动态管线（2026-10-24）

本轮保持 `codex/nvn-turnip-study-20261023` 独立分支和 worktree。接口定义采用公开声明、独立逆向及原始 SDK53.314 的字典/重定位/指令证据，执行端直接使用 KGSL/PM4/IR3/FDL 与 Android AHB/SurfaceControl。没有以原 Metal 的对象、渲染组织或返回值充当规范，也没有接回旧渲染器。

## 已验证能力

- 新动态管线实际执行原始颜色 VS/FS：64×64、127×73、2880×1620，三种顶点布局，直接、索引、间接及计数间接绘制，各重放两次；36 个作业、72 次真实提交/完成。449,180,832 个活动通道、756,576 个保护字节、2,267,736 个只读字节均通过；最大颜色误差 0，84,222,720 个重放保留区域通道保持原 GPU 数据。672 次在途所有权检查通过，最终进程退出。`a650-raw-pipeline-integrated-v1.json` 与独立 NumPy oracle 保留证据。它明确提供 native UBO1，不声称已经验证 NVN API UBO 映射或真实游戏 Draw。
- 新 Choreographer/Looper/FrameTimeline 调度接入标准 NVN Window。N2/N3 各两代窗口，共 40 次真实 Present/GOB转换/Finish，746,496,000 个活动通道正确；四张真实屏幕截图各16采样点误差0；原池地址/转换 IB 重放不变，后台与重建的所有权完整退出。当前真机仅覆盖 interval1；interval0..5 语义边界在 HOST 受控回调中验证，完整真机矩阵另行推进。设备最大间隔属性尚不宣传为5。
- 默认禁用的 source-only Program provider：完整代码/C1/control 身份匹配 immutable catalogue，使用原 host 的 bounded reader；不以 archive 第一项猜测变体。启用435/禁用12项 HOST 检查及 actual Android 链接通过，新增 typed interval observer 各44项通过。当前非空通用 NVN Draw 仍在提交/保留资源之前明确拒绝；实际 Draw 接线另行实现。
- 句柄按原 SDK 的64位 carrier 修正：SeparateTexture/TexelFetch 的 bit32、Combined 的32位OR后符号扩展、SeparateSampler 的64位移位相互区分。记录保留完整 guest carrier 及 scalar/plural 对高字的不同来源；normalized32 仅供 descriptor lookup，不作为完整 driver CB2 仿真。275项 carrier 与212项 late Draw/descriptor HOST 检查及真实后端严格链接通过。新增 `GetTexelFetchHandle` 只是1个CPU构造入口，当前337个入口不代表337项完整GPU语义。

## 混合绘制阻塞的实证边界

无诊断的 mixed consumer v4/v5 仍在第一次 Finish 返回 EDEADLK，未知所有权保留到进程退出。CS_UAV 位和 reset 后 WAIT_MEM_WRITES 已经修正并由原4CS通过验证，但不是该死锁已经解决的证据。

同一正常构建的 diagnostic APK，mask0、255和8个单bit共10个独立进程均完成4次 Clear→sampled Draw→partial Clear→canonical Buffer alias 的提交/回写/读回，全部通过且严格退出。mask0 的所有阶段标记均为等长 CP_NOP，没有阶段 CP_MEM_WRITE。因此标记写入不是成功必要条件；命令长度、布局、scratch位置、预取或执行时序仍需隔离。不能把这些诊断通过当作普通路径修复。40个实际读回与已归档4个 payload 完全相同，逐 SHA 复核并用 manifest 引用，未重复存储。

## 语义证据与剩余缺口

原 SDK graphics UBO 的 API0..13→sourceCB3..16 由标量/批量 method encoding 独立证明；其他 Maxwell API 的 `+2` 规则不能照搬。详见 `uniform-binding-sdk-research.zh-CN.md`。sourceCB、用户API槽及编译器native binding是三个编号空间；编译器注入的 support_buffer0 也不能叫 sourceCB0。

C1 shader 的110/112是纹理常量缓冲 word offset，尚未证明默认纹理 CB selector 初始化为2；112的原始指令是1D取样，已有2D实验 specialization不能当原shader真实资源定义。未知 driver payload 和纹理来源继续拒绝，不用零缓冲、固定池ID或首项变体顶替。

HOST/RAM fixture、原创资格调用、原始 GPU shader 通过均不替代 D2R 原生执行里程碑。真实 D2R 仍须从原始 NSO 通过正常规则重建、核对 manifest 后运行，并单独记录当前第一个不支持接口和是否发生真实游戏 Draw/Present。

## 可复现入口

- `tools/check_window_platform.py --sanitizers --ndk NDK --out FRESH`：正常/ASan/UBSan、受控 OS 回调、实际 API29 严格链接与 API33 函数类型。
- `tools/check_pipeline_source.py --root ROOT --bundle PIPELINE_CHECK --names NAMES --ndk NDK --out FRESH`：启用/禁用 provider、typed observer、真实 NDK。
- `tools/check_samplers.py`：真实 XML pack、采样状态与275项句柄检查，独立三套 sanitizer。
- `tools/pipeline_qualification.py` 只接受正常 producer 的 immutable recipe/FDL 产物；`build_android_probe.py --native-pipeline-check CHECK`；`run_android_probe.py` 在 ADB 前完整校验输入并在最后严格停止。
- mixed diagnostics 只通过 `--texture-bridge-diagnostics` 明确启用；`--texture-bridge-diagnostic-mask 0..255` 同APK各次 fresh nonce/进程，默认生产禁用。

失败检查也保留：首次合入 source patch 曾将窗口间隔恢复旧限制，normal fixture 在第172项拒绝，恢复最小窗口 hunk 后通过；typed bridge 的旧测试将已能表示的 GPU_UNCACHED 无storage输入误期望 ENOTSUP，观察到 EINVAL 后按当前参数前置检查修正测试。没有为通过测试放宽生产校验。

## 真实 D2R 本轮结果

从原始3个NSO正常重建、核对产物后，在普通UID的Activity中实际执行原ARM64 host/HLE与游戏模块，进入原 nnMain。实际三次TextureInitialize均成功。首次准确观察到游戏请求的WindowBuilder/Window PresentInterval均为2，已越过上一轮 ENOTSUP。新第一错误为 `nvnVertexStreamStateSetStride` 的 ENOTSUP；不能把这写成游戏绘制或首帧成功。NVN 接口按原 SDK 的16个属性/流绑定槽实现，底层编码器保留高通的32槽能力；实际Draw映射继续独立实现，避免为这个具体游戏stride加特例。进程已经严格停止；原始 stderr 含 shader control 字节，留在ignored，只提交脱敏摘要及正常构建记录。详见 `a650-activity-pacer-source-v3-summary.json`。

## 普通混合路径本轮闭环

后续最小修正将 nn_tex_shadow_chain 的 completionseq 放在实际设备页对齐后的独立数据页，保持可执行命令长度/顺序与所有资源引用不变；提交范围不包括页间空隙，不增加NOP或阶段MEM_WRITE。HOST迁移逐包比较只允许自身seq/progress地址变化，覆盖页边界与未知完成所有权；原normalchecker和真实Android双模式严格链接均通过。

正常构建的**未启用diagnostic** APK在3个独立fresh进程中全部通过：共12次Submit/Finish，活动通道、alias读回、保护区、只读输入正确，三次严格退出。普通路径此前EDEADLK现已在这个64×64/2×2混合资格profile闭环；仍不是通用NVN Draw/真实D2R画面通过，也没有公开硬件依据证明原故障机制。首次典型wrapper命令仍18716B，数据移到20480，BO由20480扩至24576；新增成本为每个wrapper至多一个数据页，在途ledger按原规则保留。不能将其称为IB长度对齐规定或已取得帧率收益。

新增depth01 catalogue正常编译10个stage变体、12个native pair；source-only检查通过。普通pipeline HOST检查最初发现测试oracle把prepared source首个identity的requirements误当运行态，修正为显式独立desired state后全部通过；生产resolver未放宽，仍拒绝未知和歧义。失败证明与成功证明均归档。

## Window interval 批次

正常同APK运行时配置已覆盖N2/N3的全部interval0..5：12个独立fresh进程、24代窗口、240次实际Present与GOB→AHB转换，4,478,976,000个活动通道均通过；552次interval getter检查、240次实际平台show参数快照观察与完整后台/重建/回收通过。240个实际raw payload全部逐SHA匹配既有lossless zip中4种模式，24张真实屏幕截图另外保存，不重复存储4.48GB相同内容。N2请求0的getter保留0、平台有效FIFO1，是当前后端政策；不将底层libnx NWindow要求冒称通用NVN规范。设备上的enqueue/set交错未强制，确定交错由HOST fixture验证。

同时使用设备实际支持的原生Perfetto `android.surfaceflinger.frametimeline`，没有自写计时器。raw系统trace留ignored，仅3.6MB哈希/路径与工具运行记录归档。Trace Processor报告220个negative_timestamp_dropped和77个frame_timeline_event_parser_errors，不能据此声称完整干净的cadence测量；N2每child实际16次Present，N3每child24次；N3 interval0的24次不设置FrameTimeline，因此其余预期216次与native带token帧216条在总数上吻合。计数吻合不能抹去trace health错误或证明逐child cadence。工具正常TERM后的首次pid检查仍看见退出中的进程，后续独立检查已明确该owned PID与app PID均不存在，再拉取trace。完整帧时序含义继续用原工具SQL核查，参数计数与截图不冒充时序结论。MAX_PRESENT_INTERVAL设备属性仍未实现，不写假值。
