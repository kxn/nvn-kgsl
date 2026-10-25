> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

独立 NVN 首批实施进度
====================

独立代码位于仓库根部 `nvn-native/`，在 `codex/nvn-turnip-study-20261023` 分支推进。当前交付打通普通 Android 应用到 Adreno 735 的直接 KGSL 命令提交，并建立 shader 输入合同。D2R 仍是后续输入与验证环境；这批库没有链接现有游戏渲染器。

已实现的工作：

- C11 静态 KGSL 基础验证库和独立 NativeActivity 测试 APK，独立 CMake 入口、NDK r28c、ARM64/API 29。
- 一个 4096 字节 uncached BO，CPU 映射、精确芯片资格检查、context 创建、Packet-7 命令构造、提交和完成等待。
- GPU 将目标从零写为 `0x4e564e31`，CPU 在完成后读回一致；正常回收无错误。提交返回错误时记录 acceptance unknown，不自动重发；未确认完成不进行应用层 BO 回收或复用。
- 构建记录包含源码、工具和产物 hash。运行工具校验产物与已安装 APK 的 hash，删除旧结果，采集新结果，测试后停止并检查本测试进程。
- shader fixture 工具验证代码/control/C1 的输入边界和哈希、显式状态及编译器/profile，生成与文件路径无关的请求键。它是输入封装层，尚不是完整 NVN control 解析器或严格 compiler accessor。

真实设备执行结果：普通应用 UID `10357`，芯片 ID `0x43030b00`，页大小 4096；提交 accepted、timestamp 1、completed true、读回匹配、cleanup errno 0。最终构建的共享库仅依赖 `libandroid`、`libm`、`libdl`、`libc`。完整结果、构建命令及产物指纹在 [kgsl-bringup.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `kgsl-bringup.json`；未随公开仓库提供）。

bring-up 过程中解决了两处真实差异：初次把 chip ID 高字节当代际的检查主动拒绝了设备，核对公开 Freedreno 设备表后改为精确 FD735 ID；精简 context flags 被此机拒绝，改用现成 KGSL replay 的 SAVE_GMEM / NO_GMEM_ALLOC / PREAMBLE / NO_FAULT_TOLERANCE 组合后成功。这两个失败结果同样保存在证据记录中。

验证包括 6 项 shader fixture 主机测试，覆盖哈希篡改、越界、C1/状态/编译器/profile 更新时请求键失效、缺少 compute 元数据拒绝及路径改变不导致重编。项目原有两个文本检查也在修改前后通过。Android 库构建、APK 签名检查和实际普通 UID 执行均通过。两台连接手机的 D2R 进程均未运行，测试 APK 进程已停止。

首个 CP 测试只执行新增命令处理器测试代码，没有 GPU shader、原项目游戏代码或真实游戏模块覆盖，因此不能算游戏原生执行的新里程碑。它只验证 command processor 的内存写入，不覆盖缓存模式扩展、并发队列、复用分配器、长时间资源退休、渲染和出图。后续 shader 执行结果另记于下文。

下一批工作按原 T03/T05 继续：整理纯公共编译器构建闭包，将一个受控 CS 经 NIR/IR3 编成目标机器码，补齐其资源和程序元数据，在当前已验证的 KGSL 入口执行并核对 buffer 输出。然后连接离屏 VS/FS。现有 shader 翻译库可复用，但 fixture 的资源/host/precision 字段还需要接入严格 accessor 和明确的 lowering；未识别输入不得沿用旧包装器的缺省值。

继续实施后，T03 的受控 NIR → IR3 编译已经完成：从固定 SHA256 的 Mesa 26.2.5 原始源码包独立构建主机编译器，关闭全部图形 API 驱动。两个单缓冲 CS 变体分别写入 `0x4e564e32` 和 `0x12345678`，均产生 128 字节目标机器码，并通过现有 IR3 的 asmroundtrip；两份机器码哈希不同。输出包括 NIR、反汇编、SSBO 数量、线程组、寄存器、常量、private/shared memory 和 preamble 信息。完整记录见 [ir3-compiler-check.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `ir3-compiler-check.json`；未随公开仓库提供）。

独立构建所需的变更已固化在构建工具中：启用显式工具构建下的完整 NIR，避免上游默认选中 `nir_stub.c`；指定现有 Bison/Flex/m4 工具及数据目录。源码包、共享源码树均不修改。主机编译器实际动态依赖为 zlib、zstd、libm、libgcc_s 和 libc。现有 shader fixture 六项检查及原有两项文本检查继续通过。

当时的编译验证没有真机 shader 执行或 Maxwell/NVN shader 输入覆盖。随后继续实施了下述 CS 真机验证；前端、通用资源语义、NVN 接口和 D2R 接入仍属于完整目标中的未完成工作。

受控 compute shader 的原生 KGSL 执行现已通过。程序状态沿用 Computerator 的公开硬件顺序，寄存器取生成头，R32_UINT SSBO 描述符直接调用 FDL。编译器输出机器码、状态命令、描述符和相对地址重定位表，Android 放入实际分配的 16 KiB BO 后重定位，提交一组单线程 compute 工作并执行完成/cache 协议。没有链接原项目渲染器。

两个独立 APK 产物分别嵌入写入 `0x4e564e32` 和 `0x12345678` 的 shader。普通应用 UID `10357` 两次均 accepted、completed，SSBO 从零变为对应数值，清理 errno 0，测试后进程退出。命令处理器写入的是单独的完成位置；目标 SSBO 值由 IR3 生成的 shader 写入。完整编译、生成头、APK 与实际设备证据见 [kgsl-cs-check.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `kgsl-cs-check.json`；未随公开仓库提供）。默认 CP-only 模式也重新构建成功，现有六项输入合同检查和两项项目文本检查通过。

该验证限定为单缓冲、单线程、无额外常量/private/shared memory 的自编 CS。它证明硬件链路已接通，不能代替真实 NVN shader 或真实游戏执行。接下来需要把这条路径抽成可复用的程序/资源/提交接口，接入真实 shader 输入与 NVN 对象/命令语义，并补离屏 VS/FS、窗口和最终 D2R 验证。

继续实施后，KGSL 已抽成独立 Device / BO / Queue 接口，支持调用者页面导入、缓存范围操作、异步提交及完成后释放在途引用。原来的 CS 用例也改用该接口，真机读回仍为 `0x4e564e32`。新库没有复用旧渲染后端。

首批 NVN 资源接口已接入 resolver，公开重建头提供 ABI 类型，核心实现使用独立 side table。真机测试经 NVN 函数指针创建 Device、Queue、MemoryPool、Buffer 和 CommandBuffer，直接导入调用者缓存内存，创建两个重叠 Buffer 视图，录制两份 ClearBuffer 命令并异步提交。提前释放句柄、Buffer 和录制对象后，后端仍保留在途 BO；数据池和命令池的提前销毁均被拒绝，QueueFinish 后正确回收。读回逐字检查重叠顺序及整页其余哨兵，均通过。

该测试还通过了对象允许写入范围、command/control 实际用量，以及控制内存不足时的拒绝检查。首次页面导入得到 EFAULT，核对 [Qualcomm 的用户页面映射路径](https://github.com/qualcomm-linux/kgsl/blob/gfx-kernel.le.0.0/kgsl.c) 后，对 UAPI 内嵌整数地址去掉 ARM64 顶字节标签，保留 CPU 原指针身份，真机导入和 GPU 写入随后通过。原始失败、最终 NVN 结果和共享后端 CS 回归见 [nvn-resource-check.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `nvn-resource-check.json`；未随公开仓库提供）。

当前入口表的 43 个名称是已提供且通过签名检查的函数，不是 NVN 合规通过数量。资格只覆盖串行调用、单 command/control arena、CPU_CACHED | GPU_CACHED 页面，以及有限 ClearBuffer；调用者自编测试证明这条实现链路，不能证明原 NVN 未知语义。多线程、跨池物理别名归一、内存回调、公共对象延迟销毁、Sync/Event、Program、绘制、上屏和 D2R 接入仍未完成。后续需要继续连接真实 shader 与命令语义，并用项目黑盒和真实 ARM64 游戏模块逐步验收，完整目标保持进行中。

原始 Maxwell 输入的离线编译链现已接通。新增 D2R 适配工具只从 SHA256 为 `55fd909bb7230b002357e5b0d8499c11f06888f345c49e69eb4734df44a27d0e` 的未改写 94012 main NSO 提取四个内嵌 CS，先用原项目 NSO 解压工具并校验三段哈希，再核验 control 的身份。没有修改游戏模块，也没有用现有手写等价 shader 或游戏渲染实现替代原始输入。原始字节和所有衍生 shader 均留在忽略的 `.local/` 内。

新增严格 Maxwell accessor 对现有 `IGpuAccessor` 的全部方法显式实现，新增接口默认方法也会被反射检查拒绝。代码和 C1 的读取有边界；所有实际查询从 fixture 显式状态取值，缺失即失败；未知资源和翻译器诊断拒绝编译。前端构建闭包只有 Maxwell shader 库、SPIR-V emitter 和公共主机工具依赖。没有接入模拟器的 CPU 执行、系统服务或图形后端。

新的独立 `nvn-spirv-ir3` 通过公共 VTN 导入 SPIR-V，进行 NIR 系统值、控制流和 buffer 显式 IO lowering，将 set 0 的标量 UBO/SSBO binding 作为各自硬件表索引，再由 IR3 生成目标 Adreno 机器码。枚举名称 `TargetApi.Vulkan` / `NIR_SPIRV_VULKAN` 只表示中间格式的描述符约定；没有创建相应 API 设备或对象，独立 Mesa 构建继续关闭全部图形 API 驱动。

四个原始 control 哈希前缀 `d0f769bb916bbba9`、`16479cc6d75c2b28`、`0687e2ef95133577`、`4df5909e6dabee05` 均通过完整链路。两个生成 256 字节机器码，两个生成 384 字节机器码，四份哈希不同；工作组均为 32×1×1。反射给出两个 SSBO 与一个 c3 常量缓冲；硬件 binding 为 1、2 和 3，表中的空位保留。IR3 元数据进一步保留了 workgroup/local ID 的实际输入寄存器、常量分配、driver parameters 与 UBO push ranges，供后续程序上传使用。

四个输出均通过现有 `spirv-val` 和 IR3 `asmroundtrip`。缺少实际使用的查询、代码哈希错误、过大输入范围、有效哈希但被截断的代码、未获资格的精度模式均被拒绝；将一个有效 SPIR-V 的 descriptor set 改为 1，经现成 SPIR-V 工具重新组装并校验后，也被我们的资源布局检查主动拒绝。构建与输入指纹、查询记录、资源/程序元数据及拒绝原因见 [maxwell-ir3-check.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `maxwell-ir3-check.json`；未随公开仓库提供）。旧受控 IR3 两变体检查和项目文本检查仍通过。本轮没有运行游戏或测试 APK，两台手机的游戏进程及 Adreno 测试进程均确认未运行。

这一结果只证明真实 shader 输入可以编成目标机器码及编码检查通过，尚未证明完整 Maxwell 语义等价、原 NVN 行为或这些 CS 的硬件执行。此次容器只覆盖固定已核验 control profile，C1 为空；local memory 零和 host 查询是明确的翻译 profile 选择，不能推广为设备能力。下一步原生 Program/Dispatch 要按编译器元数据上传常量区、设置系统输入寄存器并绑定多缓冲，再把四个原始 CS 放到 KGSL 执行并检查结果。原先单缓冲、无常量的受控 emitter 不覆盖这些要求，不能直接作为接入完成的证据。随后仍需离屏 VS/FS、纹理、同步、窗口及真实 D2R 调用接入。

继续推进后，四个原始 CS 已经通过新 NVN Program/Dispatch 入口在 Adreno 735 上真正执行。新增 ProgramInitialize/SetShaders/Finalize、BindProgram、BindUniformBuffer 和 DispatchCompute，入口继续做公开重建 ABI 的编译签名检查。ProgramSetShaders 用完整原始 code/control 匹配显式注册的离线编译缓存，未命中返回 ENOTSUP；这不是运行期 Maxwell 编译。缓存身份、测试输入及四个内嵌核的结果算法都位于 D2R 测试适配层，驱动没有游戏哈希分支或手写等价 shader。

通用 compute emitter 使用 IR3 的常量、寄存器和工作组元数据生成程序模板；运行端填实际 KGSL GPUVA、多缓冲描述符、Dispatch 维度和 driver parameters。固定 R32 描述符来自现有 FDL，地址、extent 和子 64 字节偏移的动态字段由生成的寄存器定义提供；每个程序在主机用 20 组尺寸和地址对照完整 FDL。UBO push ranges 使用 GPU 间接常量加载，测试在录制结束后修改 c3，执行时正确消费新值。每份 Dispatch 使用独立不可变的 8 KiB 命令/描述符/完成存储；句柄释放后在途存储延迟到 QueueFinish 回收，重复提交会先重置完成位置。

首轮硬件执行揭示了复用前端的一处通用错误：两个差值核在进位/借位边界的高 32 位不符。NIR 中已出现错误加法，因此没有把它当作 Adreno 或原游戏问题规避。原始指令是低位 `IADD.CC -a,b` 加高位 `IADD.X -a,b`；固定 Mesa 源码包的 `nak/sm50.rs` 中 OpIAdd2 使用 INeg，OpIAdd2X 使用 BNot，`hw_tests.rs` 也分别测试这两种语义。现有前端对二者都先做普通 INegate，并漏掉初始减法 +1 对 carry 的贡献。

修复通过独立编译器的 MSBuild source replacement 落地，原共享 vendored 文件不改动。覆盖层锁定原源码 SHA，按补码加法的两段 carry 生成结果、CF/VF 和链式 ZF/NF；`.PO` 不在本 profile 内，明确拒绝。编译器输出 `maxwell-iadd-carry-v1` 标记，fixture/compiler key 纳入覆盖层源码指纹。修复后保持同一原始游戏 code/control 输入，重新从头翻译、验证、生成机器码和 APK。初次失败与最终通过的完整构建链保存在 [nvn-compute-check.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `nvn-compute-check.json`；未随公开仓库提供），当前主机编译证据同步更新到 [maxwell-ir3-check.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `maxwell-ir3-check.json`；未随公开仓库提供）。

最终普通 UID `10357` 的 12 组主用例全部通过：四个原始核分别处理 1、33、65 项，覆盖多个工作组、尾线程、64 位零/最大值/进位借位输入，以及相对 64 字节边界偏移 4 字节的源和目标。每组逐字节检查完整 16 KiB 输入/输出/control/code 存储及所有未写哨兵。另有 4 次保留命令句柄的复用：销毁录制对象后修改源数据和 c3、清空输出、再次提交同一句柄，并核对新的结果与边界；均通过。在途 Program 销毁被拒绝，完成后 Program/Pool/Queue/Device 清理无错误，间接验证 Dispatch BO 无遗留。原 ClearBuffer 的导入、重叠视图、对象/arena 哨兵及在途寿命测试也重新构建并在同一设备通过。

验收边界仍明确：CPU 执行的是新增 ARM64 调用者与独立驱动；原项目实际执行的主机部分是 NSO 解压工具及复用 shader 前端，GPU 执行的是从原始 D2R 内嵌 Maxwell 核翻译出的 Adreno 代码。本轮没有执行真实 ARM64 游戏模块，也没有修改/部署游戏二进制或接入原渲染器。测试包每次结束均停止，Mali 游戏未启动。

当前 compute profile 只覆盖 64 线程 wave 配置、R32 SSBO、全部可 push 的 UBO、无 sampler/private/shared/额外常量数据和空 C1。全局缓冲描述符为绑定 c0 中的 u64 地址/u32 尺寸/u32 reserved，当前由 CPU 在录制时解析，须保持不变；GPU 动态描述符及一般 NVN 消费时点仍未获资格。公共对象销毁采取显式 busy 拒绝，不能推广为原 NVN 的未知寿命语义。下一步转向 VS/FS 与离屏绘制，同时继续补运行期 shader miss、资源/同步语义和真正的 D2R 调用适配；完整目标继续进行。

本轮核对图形资源时修正了上一轮调用者与接口实现中的槽位混淆：此前把原始 Maxwell c0/c3 直接当成 NVN Uniform 槽位，虽然验证了 GPU 执行与资源寿命，不能据此证明 NVN 的资源绑定语义。仓库已有的 Maxwell 资源引用记录以及原 NVN 调用/Metal 绑定证据显示，NVN Uniform r 对应 Maxwell c[3+r]；compute c0[0x310+16*r] 是驱动根据 NVN Storage r 生成的表项。这里只复核该接口事实，没有引入旧渲染器实现。

现已将独立实现改为 NVN Uniform 0..13 → c3..c16，新增 BindStorageBuffer(s) 和 BindUniformBuffers。全局访存反射直接连接 NVN Storage 槽位，不再要求调用者构造或绑定 c0，也不再从调用者的 c0 内存中读取描述符。批量绑定先验证全部区间再提交；零地址/零尺寸解除绑定。当前仍只开放 compute 阶段及已获资格的 c0 Storage 表布局，其它驱动 bank/全局寻址方案明确拒绝。

修改后的原始 CS 调用者只绑定 Uniform 0 和 Storage 0/1，所有 12 组结果与 4 次句柄复用再次通过；原来的 c0 区域现在保持完整哨兵。三项新增拒绝验证覆盖 Uniform 越界、后一个 bulk range 无效时前一个绑定不得被部分修改，以及 Storage 解绑后 Dispatch 不得发出任何命令。旧 ClearBuffer/别名/寿命测试也通过。最新构建和普通 UID 证据见 [nvn-buffer-slots-check.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `nvn-buffer-slots-check.json`；未随公开仓库提供），旧 compute 记录标明其绑定 profile 已被取代。

图形前端也已实际落地：同一份原始、段哈希已核验的 NSO 提取 VS control `8ac93c54cfb3cbe3` 与 FS control `0d721a0acc36c0bf`，保留 NVN 0x30 前缀、SPH、原始机器码和空 C1。严格 accessor 显式构造完整 GpuGraphicsState 并记录所有查询，校验 fixture 阶段与 SPH，未提供的状态不补默认值；没有引入 PC shader 或其反射。该 untextured profile 配置 float 属性/输出、三角形、viewport/depth/alpha 和原生格式交换，是明确的编译配置，尚非 BGRA 等硬件功能资格声明。

新增独立 `nvn-graphics-ir3` 与 CS 共用 VTN/buffer lowering，拆开 SPIR-V 的 per-member gl_PerVertex 结构，再做阶段 IO lowering、IR3 变体编译和 VPC 连接。原始 VS/FS 分别产生 384/128 字节机器码，通过 SPIR-V validator 和 IR3 asmroundtrip。VS 消费 float2 位置和 float4 颜色，经一个 64 字节矩阵 UBO 变换位置；FS 经 Location 1 消费四分量颜色。静态连接导出实际寄存器与常量区，VPC varying 在位置 0..3，position 在 4..7，point size 在 8，总 stride 为 9 dword。元数据明确记录 VS 的 early preamble，不能按无前导程序的简单 shader 上传。缺失图形状态、SPH 阶段不符、有效 SPIR-V 请求不存在的 VS 输出均被拒绝。

KGSL 原有 device-info 查询现在保留 GMEM 尺寸，普通 UID 真机返回 1,572,864 字节。图形主机工具直接复用公共 `fd6_calc_gmem_cache_offsets`，对该精确芯片和实测尺寸导出 3 个 CCU、sysmem depth offset 0、color offset 786,432；尺寸不足会在调用布局函数前拒绝。设备读取和主机图形编译/连接的完整记录见 [graphics-compiler-check.json](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `graphics-compiler-check.json`；未随公开仓库提供）。共享 NIR 重构后，四个 CS 的机器码及原生状态模板 hash 与已执行版本一致；最新编译器和后端重新构建的 NVN 槽位测试也全部通过。

本轮图形部分仍没有提交 VS/FS 绘制或读回像素，也没有执行真实 ARM64 游戏模块。下一步需要把这些实际 shader 元数据、VFD 输入、VPC exports、preamble/常量和 CCU 布局接到原生图形状态与 Draw，先完成离屏像素验证，再补纹理、深度和 NVN 图形入口。固定源码包中的公共硬件实现已定位到 `fd6_program.cc` 的 shader/VPC/FS 输入输出、`fd6_emit.cc` 的静态与 cache 状态、`fd6_gmem.cc` 的 sysmem/MRT 设置，后续只取硬件接口与编码，不引入图形 API runtime。完整目标继续进行，测试包及两台手机的游戏进程均确认未运行。

继续实施后，原始 VS/FS 的真实离屏绘制已通过。按用户要求，三个子代理分别推进硬件状态、纹理编译和 D2R 接入；主代理整合跟踪代码并独占设备操作。新增 `graphics_artifact.cc` 从公共硬件编码和实际 IR3 元数据生成 A735 的完整 restore、sysmem/CCU、MRT、VFD/VPC、VS/FS、常量、光栅/混合/无深度状态、Draw 和 cache clean 序列。没有构建或链接 Gallium/图形 API runtime。FDL 给出 64×64 linear RGBA8 的 pitch 256、大小 16,384 字节；模板为 698 dword、8 个带范围的 GPUVA64 补丁。

独立 `graphics.c` 校验芯片、实测 GMEM、资源尺寸和对齐，将调用者 BO 地址填入新的不可变命令 BO。原始 VS/FS 机器码不修改；矩阵通过 GPU 间接常量加载。普通应用 UID `10357` 四次提交同一份 IB 均完成：全屏红色、矩阵平移后右半绿色、完全移出视口保持背景、RGB 三顶点插值。共检查 65,536 个通道和 98,304 个边界字节，纯色精确匹配，渐变只允许一个 UNORM 量化单位；清理 errno 为零。真实 RGBA 结果和无像素修改的 PNG 转换预览已保存，完整构建/运行记录见 [原生图形验证](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `kgsl-graphics-check.json`；未随公开仓库提供）。

这次执行的 CPU 代码仍是新增 ARM64 调用者和独立后端，GPU 执行的是原始 D2R 内嵌 VS/FS 翻译后的机器码；没有执行真实 ARM64 游戏模块，也还没有经 NVN 图形函数录制这份 Draw。它只完成 T05 中的无纹理、单样本、单 UBO 离屏部分，不能宣布 T05 全部完成。深度、纹理、窗口、一般图形状态和 KGSL 图形抢占健壮性尚未获资格。设备测试后已停止本测试进程；Mali 游戏未启动。

纹理线同时完成了严格 accessor 的资源注册、显式 normalized 2D 查询和 texture/sampler 分域反射。新增提取器从同一原始、三段哈希已核验的 NSO 提取真正匹配的 VS `96757bb36a024b91` / FS `a7672f1d3bfeb7d0`：前者输出 UV Location 1 和 color Location 2，不能拿之前只有颜色输出的 VS 替代。纹理维度和归一化坐标来自显式测试 specialization，不是捕获到的原游戏 draw 状态；typed image、buffer、数组等尚未支持。原始句柄来源反射为 texture CB2 word110、sampler CB2 word346，不能把这些数当成 NVN 描述符池索引。

公共 NIR `nir_opt_deref` / `nir_lower_samplers` 提供了主要机制，新增桥接只严格验证合法的 combined sampler 纯类型拆分，并保留 set0 的静态稀疏 binding。检查揭示并修复了资源元数据缺口：IR3 取 `textures_used` 的最高位计算表长，声明数量不足以描述稀疏 binding，须按真实 texture/sampler 指令重建两张位集。原始 binding1 正确需要 2 slots，binding15 对照需要 16 slots；没有压紧或偷偷重编号。

真实纹理 FS 又触发了现成 IR3 汇编工具的词法缺口：ISA 反汇编输出合法的 `hp0/hup0`，lexer 却只接受 `p0/up0`。新增锁定原始源码 SHA 的两规则覆盖层，保留既有 Flex 构建和 `asmroundtrip`；没有关闭检查、降低精度或改 shader。隔离的原始/修复 lexer 对照、16 个有效寄存器编码、4 个语法拒绝和源码 SHA 拒绝见 [parser 验证](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `ir3-half-predicate-parser-check.json`；未随公开仓库提供）。

最终纹理 pair 经严格前端、`spirv-val`、IR3 汇编往返逐位比较和 VPC 连接全部通过，VS/FS 分别为 384/640 字节，实际采样指令为 `sam s#1,t#1`。本 pair 的 prefetch count 为零，描述工具仍导出 prefetch 全部字段，不能推广为所有 shader 都不需要预取状态。五项拒绝覆盖缺少实际查询、非零 descriptor set、越界 binding 和资源数组。完整离线证据见 [纹理编译验证](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `texture-compiler-check.json`；未随公开仓库提供）；它不是 GPU 采样结果。

接入线新增薄的 `host_bridge.c`，通过原 `HLE_FORCE("nvnBootstrapLoader")` 注册将后续 lookup 留在独立 resolver。游戏的加载表不检查 NULL；因此适配层对已知未实现名字返回明确终止的具名函数，调用时打印名称并退出78，不能假成功。公共头中 477 个原型生成真实签名并检查赋值；其余 46 个只作为未知 ABI 的终止边界。通用 driver 的 NULL 契约及 52 个实现入口保持不变。原游戏最早的 `DeviceBuilderSetFlags` 缺口仍未实现，其 `0x100/0x115` 没有被空接受。

新增 `build_d2r_bridge.py` 复用原 NSO/加载名工具，生成后重新核验输入、段哈希和全部名字产物，只构建带原 HLE 注册的 ARM64 OBJECT 与独立驱动归档。ELF64/AArch64、24 字节 `swforce` 保留段、主机 resolver 路由、真实签名及具名失败检查和缓存篡改拒绝均通过，见 [适配层检查](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `d2r-host-bridge-check.json`；未随公开仓库提供）、[构建记录](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `d2r-bridge-build-check.json`；未随公开仓库提供） 和 [缓存拒绝](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `d2r-bridge-cache-rejection.json`；未随公开仓库提供）。它没有修改原 host 的构建选择，也没有连接或执行真实游戏。

合并后六个旧 shader 的 SPIR-V、四个 CS 的 Adreno 机器码/原生命令模板、无纹理 VS/FS 和图形模板均保持相同。当前 NIR/lexer 修复后的完整主机回归和 Android 执行源码对应关系见 [合并回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `integrated-shader-regression.json`；未随公开仓库提供）；原始输出身份相同，因此没有为同一二进制重复跑 GPU。Android probe 与驱动的严格警告构建、六项 fixture 检查和原项目两项文本检查通过。下一步将硬件纹理描述符接到已验证 Draw，同时完成 NVN 图形对象/状态/录制，再推进 Sync、窗口与真实 D2R 调用；完整目标继续进行。
