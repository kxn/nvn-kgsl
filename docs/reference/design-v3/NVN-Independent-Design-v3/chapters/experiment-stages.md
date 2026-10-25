# 独立验证项目的实验阶段：Turnip/KGSL 窄闭环

本文件是单独开线的验证项目设计，不是现有项目的改造、PR、合并或实施授权。当前只修订设计文档；没有开始驱动开发或设备执行。若用户以后决定实施，应另行确认实验环境和范围。本文件替代第 08 章的阶段顺序，以及 revision2/turnip-strategy.md 的 T0–T2 含混边界。这里的路径和 `nvnn_*` 符号是**拟新增实现**，不是已存在或已验证的驱动接口。现存源码锚点以本包固定 revision 为准。此次只进行静态审查和设计，不提交驱动补丁、不运行设备。

## 1. 必须先纠正的计划错误

1. 已找到公开 host import/native image 补丁，但其与 f830 产物的对应及设备资格仍阻断的是“既有 guest CPU 页面直接 import/native image”路线，不阻断普通 KGSL BO、host 状态机、离屏 clear/draw。不能把取得自定义驱动源设成所有工作的 T0。
2. P1 同时要求 import、shader、descriptor、draw、compute、copy、fence、present，实际是一组独立风险，不能作为一个阶段。尤其 present 不应阻断离屏像素验收。
3. “全部 523 调用都有行”不是 loadable frontend，更不是调用实现。未知返回 ABI 不能统一返回零；没有契约的入口不能被标为可执行。
4. 首版不先建带 `bind_gpuva` 的通用后端虚表，不先抽取 Turnip/PanVK 共同调度器。KGSL 的 native BO/submit、Adreno shader/root、资源可见性直接实现一条路径；只共享已有 NVN 不变量和 host 测试。
5. 高级画质/场景 hack 不能替代运行必需的同步或 query。首先需要不带这些 hack 的窄语义闭环；将其设置为默认开关会令后续结果无法归因。

## 2. 四个里程碑，不混称“能跑了”

| 层级 | 严格含义 | 不代表 |
|---|---|---|
| L0 可加载前端 | ARM64 library 可被独立 harness 加载；已实现 resolver ABI、对象生命周期和录制测试通过；未知入口受控报错 | D2R 可以启动；任何 GPU 命令已正确 |
| L1 离屏像素闭环 | 同一原生 device/context 的 BO、提交、完成、读回通过；先 clear，再单 program triangle，像素参考正确 | 已处理 NVN shader 二进制、GOB、游戏窗口 |
| L2 D2R 小核心 | 一份明确列出的实际调用/资源/shader 序列能重放；逐项满足实际 NVN 输入语义，输出、时间/事件和 CPU 可见内存正确 | 一个游戏的全部场景，更不是整个 NVN |
| L3 D2R 场景闭环 | 冷启动、流送、战斗、菜单、区域切换、退出恢复、surface 重建在冻结条件下通过；动态实际调用无缺项 | 所有 523 名称的通用兼容 |

L0/L1 的测试程序必须标明 synthetic。只有消费真实 D2R NVN shader/调用/资源输入的测试才可标 L2。用手写 NIR triangle 不得标成“NVN shader 已完成”。完整 NVN 是独立持续兼容工作，不设置“一次补全 523 个入口”研究范围。

## 3. 独立实验工作树、产物与拥有权

未来验证方案采用从固定 revision 建立的独立实验 Mesa 工作树，不改日常构建、主分支或稳定安装。所有产物带 experimental 身份，存入独立目录，由独立 native client/harness 加载。稳定 D2R 仅提供工作负载、输入资料和 reference，不能作为本计划的修改目标。实验树内的 `src/freedreno/vulkan/nvn/`（测试/前端/core）和同级 `tu_nvn_bridge.{h,cc}`、`tu_nvn_emit.cc`（现有 libtu 内部桥），暂保留上游 Turnip 冷启动和 vk_runtime 内部依赖；这是 native 消费路径，不以删除所有 `vk_` 类型为目标。独立 client 只调用实验 bridge，不在稳定 D2R 的 Vulkan backend 中混合执行。bridge 只导出一个版本化 C ABI `tu_nvn_get_interface_v1`，同 DSO 复用现有 Turnip 初始化。最终构建/extraction 细节以 `bootstrap-experiment-plan.md` 为准。

拟新增目录：
- `nvn/frontend/`：resolver、对象 sidecar、builder、command storage adapter；不得存 KGSL 指针到 guest ABI 对象未核实的字节。
- `nvn/core/`：logical span、publication、recipe、retirement，采用 `memory-contract.md` 的规范结构，不再发明第二套 epoch。
- 同级 `tu_nvn_bridge/tu_nvn_memory/tu_nvn_submit/tu_nvn_emit`：cold bootstrap、普通 BO、KGSL submit adapter、native descriptor/state/shader；不另外建立一套重名 `nvn/tu/` wrapper。
- `nvn/tests/`：host、mock KMD、device probe、replay。仅新测试程序能执行设备 probe，默认 host CI 不打开 GPU。
- 独立实验 client 的 `clients/d2r_replay/input_adapter.c/.h`：读取有界 replay 包，不修改稳定 D2R 的 loader、构建或运行配置。未来如需游戏实时接入，另行设计实验副本 adapter，不由本设计承诺。

已有源码修改应限制在需要抽取 helper 的位置。新 `nvnn_*` 函数名是本清单的目标接口；上游 helper 的签名必须按固定源码调用，不能根据文档猜调用参数。公共 ABI、内存和 GPU emission 的详细结构分别由同目录专项合同定义。

## 4. 假设未来实施时的依赖 DAG 与无设备验证范围

```text
V01 manifests/build skeleton
  ├─ V02 ABI sidecars ─ V03 command recording ─┐
  ├─ V04 memory/publication host model ────────┼─ V11 native draw state ─ V12 bounded replay
  └─ V05 cold bootstrap ─ V06 ordinary BO ─ V07 submission/retire ─ V08 clear/readback
                                             └─ V09 resource descriptors ─ V10 one-program compile ─┘
```

细化：V09 同时依赖 V04、V06、V08（共用 emit 文件；其 host 编码测试可先并行准备）；V10 依赖 V05、V09；V11 依赖 V03、04、07、08、09、10。V12 只在前项已通过时做录制/资产闭环，不能绕过 shader/内存阻断。

V01–04 的设计、代码及 host CI 在未来获准实施后，无设备即可开展。V05–12 未来获准实施时可在独立实验树内编译/mock 验证，保持与稳定产物完全隔离，但其 device acceptance 不能先标通过。设备 probe 必须使用用户明确批准的设备运行安排，本设计没有替用户执行 probe。ADDR import、VBO、native image、Android WSI 都不是 V01–11 的前置。

### 与 bootstrap 实验主题的对应

该文的 B0–B6 是技术主题工作包，不是另一条实施顺序，本清单 V01–12 是更细的验证阶段，不是生产开发任务或另一条落地顺序。B0 对应 V01/05；B1 对应 V06/07；B2 对应 V09；B3 对应 V08/10/11；B4 的真实 NVN 接入对应 V12 及后续实际调用子集，GMEM 是独立后置项；B5 WSI、B6 import 在本批之后。若目标是实际游戏窗口闭环，WSI及 sysmem replay 资格优先于 GMEM；不能为遵守 B 编号而先做 GMEM。新增符号和 Meson/version-script 约定直接采用 bootstrap-experiment-plan，不另定义 DSO ABI。内部文档 header 的 contract version 3、DSO bridge API v1、shader resource ABI_1 是三个独立版本域，不能互相比较或用同一数字暗示兼容。

## 5. 12 个设计验证阶段

每个阶段只能增加一项可观察能力。不得把后续条件不足变成当前阶段被误判成功。

### V01 固定源及最小构建目标
- 新：`nvn/meson.build`、`nvn/source_manifest.json`、`nvn/tests/manifest_test.py`、`nvn/tests/host_main.c`；现有：`src/freedreno/vulkan/meson.build` 增加默认关闭 experimental target。
- 函数/入口：新 `nvnn_get_build_manifest()`；链接目标与编译选项参照 bootstrap-experiment-plan，不手写替代 Mesa codegen。
- 输入/输出：四仓 pin、构建工具链、生成头版本 → 可定位 build-id 的空库及 host test。
- 不做：设备探测、自动下载/刷写驱动、默认替换应用渲染器。
- 第一 fixture：改变一个 pinned header hash，构建资格检查必须失败；host 目标不链接 Android-only entry。
- Done：host 与 Android cross-build 的产物/依赖清单明确；只编译成功不写 device pass。
- 停止/失败处理：删除独立实验产物即可；不涉及稳定 Vulkan/Metal 路径或应用二进制。

### V02 可加载 NVN ABI 与对象 sidecar
- 新：`frontend/dispatch.c`、`frontend/abi_profile.c/.h`、`frontend/object_registry.c`、`frontend/abi_manifest.json`、`tests/abi_canary.c`。
- 函数：`nvnn_lookup()`、`nvnn_object_create/find/destroy()`；对照现有 `hle_nvn.c::nvn_lookup/nvn_init_slots` 和 `nvn_protos.h`，不拷贝 red-stub 的万能返回 ABI。
- 输入/输出：名称、已核实参数/对象地址 → 精确签名 wrapper、带 generation 的 sidecar。首个可执行集合仅 DeviceBuilder defaults/初始化、Device 初始化/销毁及 GetProcAddress 已核实路径；此处 Device 成功只属于私有 host-mock profile，生产 GPU Device 要到 V05，能力返回不能声称图形可用。
- 不做：523 个入口语义、未核实 QueueErrorInfo/RawStorageClass 的伪实现。
- fixture：对象前后 canary、同地址销毁重建、double-finalize、resolver 未知名称；ARM64 ABI harness 检查 pointer/integer/float 返回类别。
- Done：L0 中 ABI 子门通过，支持清单分 known-signature/unimplemented/implemented，不仅有名字。
- 失败：未知调用保留名称/调用点诊断，终止此次受测会话；不可继续返回零制造后续野指针。停止实验 harness，不改变原有 loader。

### V03 录制存储与参数消费时间
- 新：`frontend/command_storage.c`、`frontend/recording.c`、`frontend/bytecode.c`、`frontend/command_cache.c`、`tests/command_storage_test.c`。
- 函数：`nvnn_cmd_begin/end()`、`nvnn_cmd_append()`、`nvnn_cmd_add_storage()`、`nvnn_cmd_call/copy()`；对接 CommandMemory/ControlMemory 双 arena 与 AddCommandMemory/AddControlMemory/Get*MemoryUsed/回调的明确 subset，按 recording-abi-contract 实施。
- 输入/输出：guest 存储范围和调用 → 权威存储 generation + 派生顺序 IR；不得只产一份私有 vector 后假装 guest 存储仍有效。
- 不做：承诺任意 Maxwell raw command 编码、LM3 私有 patch profile、GPU 提交。
- fixture：不足空间同步扩容回调、浅 Copy 与嵌套 Call 的不同生命周期、同地址 handle 重用；API 数组在录制后改写的 consumption-point 测试。
- Done：所有支持的录制函数列明 copy/reference/执行读取、内存用量及失败原子性。未知 guest patch 模式明确拒绝而非继续解码旧 IR。typed-v1 的记录长度/收费只属于私有测试 profile；D2R是否可用该 profile 必须另经 inline 访问/运行写集资格测试。
- 停止/失败处理：整条未完成记录标 invalid，不提交前缀半条命令；旧 reference session 不受影响。

### V04 backing/span/publication 与 retired refs
- 新：`core/memory/identity_registry.c`、`mapping_snapshot.c`、`range_ledger.c`、`submit_journal.c`、`host_access_lease.c`、`tests/memory_model_test.c`；规范以 memory-contract 为唯一状态定义。
- 函数：`nvnn_resolve_backing_span()`、`nvnn_publish_range()`、`nvnn_collect_dependencies()`、`nvnn_retire_refs()`。
- 输入/输出：pool/map generation、canonical backing/range、生产者点 → 当前依赖集合与引用所有权，不输出虚假的“上一帧 ready”。
- 不做：mprotect signal handler、userptr import、cache 操作、native tiling。WaitEvent poll 不加入普通 reader frontier，避免 SignalEvent 反向等待造成死锁；tentative reservation 不提前 publish，取消计划会让下游重规划。初版只支持受控 backing 与显式 host lease，未知 alias 来源不自动判独立。
- fixture：两 pool 同 backing 的交叠写、两个 producer 的部分范围发布、MapEpoch ABA、仅一 queue 完成仍不可回收、range overflow、read frontier、accepted/unknown submit 前缀、过时转换 V 不得覆盖 V+1。
- Done：区间和 partial-order 模型测试通过；失败 publish 不改变已提交版本，GPU failure 不能算完成可见。
- 停止/失败处理：分配失败返回整次计划失败，不丢依赖；host-only 模块可独立停用。

### V05 原生冷启动而非重造 device
- 新：同级 `tu_nvn_bridge.h/.cc`、`nvn/tests/bootstrap_probe.cc`；现有可能改：`tu_device.cc`、`tu_knl_kgsl.cc` 中显式 internal helper 暴露，具体见 bootstrap-experiment-plan。
- 函数：新 `nvnn_tu_open/close()`；冷路径复用完整 `tu_CreateInstance/tu_CreateDevice` 初始化及其 `tu_knl_kgsl_load` 下层，不在 hot draw 上调用 Vulkan entry。`tu_knl_kgsl.cc` 不能独立抽走；它依赖完整 `tu_device` 状态。
- 输入/输出：显式 KGSL fd/配置、已支持 chip → 原生实例/device/context handle、能力/失败清单。
- 不做：ADDR import、shader、present、兼容所有 A6xx/A7xx/A8xx。
- fixture：mock 逐项注入 init 失败验证逆序析构；Meson `libtu_files` 和 version-script 必须明确加入唯一导出；device probe 仅 open/close、芯片/内核识别。
- Done：真实 probe 成功才能把该设备列为 bootstrap qualified；构建成功只能标 compile-qualified。
- 停止/失败处理：关闭所有已创建 fd/context/BO；失败则终止实验 session；reference 单独运行，不中途混 device。

### V06 普通 BO 的稳定地址与映射
- 新：同级 `tu_nvn_memory.cc/.h`、`tests/bo_probe.cc`；复用 `tu_knl.cc` 的 `tu_bo_init_new/tu_bo_map` 路径及 KGSL 实现。
- 函数：`nvnn_tu_bo_alloc/map/destroy()`，将 owner/id/iova/size/cache mode 绑定为不可混用对象。
- 输入/输出：大小、用途 → 具有生命周期内稳定 IOVA 的普通 BO 与可选 CPU map。
- 不做：VBO、任意 malloc 地址 import、跨 window 拼接、UBWC host 写。
- fixture：alignment/overflow/分配失败、两个 BO 不混身份、映射写后读取 CPU sentinel；CPU sentinel 不等同 GPU coherent proof。
- Done：BO 生命周期与 byte bounds 测试通过；对象仍在飞行时 destroy 只转 deferred，不能释放。
- 停止/失败处理：新增 BO 全回收；不会改变 guest pool 的 backing 归属。

### V07 提交、完成与退休
- 新：同级 `tu_nvn_submit.cc/.h`、`tests/submit_mock_test.cc`、`tests/submit_probe.cc`；复用 `tu_submit_create/tu_submit_add_entries/tu_queue_submit` 及 `tu_knl_kgsl.cc` 的 submit/wait/timestamp/fence 逻辑，不用 DRM syncobj 代替。
- 函数：`nvnn_tu_submit()`、`nvnn_tu_wait()`、`nvnn_tu_poll_completed()`；内部 serial 与 KGSL context timestamp 映射。
- 输入/输出：IB/refs/dependencies → kernel accepted serial + completion ticket；timeout/fault 与完成严格区分。
- 不做：多 context 调度、任意 NVN Event 条件、Query、空 GPU_COMMAND 伪进度。
- fixture：32-bit wrap、wait-only submit、等待超时、ioctl 拒绝与 accepted 后设备 fault、pending refs 在完成前不回收。
- Done：GPU 写 sentinel 在 fence 后可见；mock 覆盖错误路径；单纯 timestamp 变化不足以通过。
- 停止/失败处理：拒绝提交回收未提交资源，已接受后的错误按 lost context 管理，不能重放可能已执行的 draw。

### V08 不依赖 shader 的离屏 clear/readback
- 新：同级 `tu_nvn_emit.cc/.h` 的 offscreen 部分、`tests/clear_probe.cc`；参考 `tu_clear_blit.cc`、`tu_image.cc`、fdl layout 与现有清理/缓存 emission。
- 函数：`nvnn_tu_create_test_surface()`、`nvnn_tu_emit_clear()`、`nvnn_tu_readback_test_surface()`。
- 输入/输出：固定 RGBA8、单 mip/layer/sample、已知 clear value → readback bytes。
- 不做：guest GOB、NVN texture API 完整性、UBWC、GMEM、WSI。
- fixture：非零 row stride、边缘尺寸、两次不同 clear、GPU→CPU readback 缓存路径。
- Done：原生 command→真实 completion→内容读回闭环；只报告 L1-clear，不报告 triangle/D2R。
- 停止/失败处理：surface 私有且可销毁；内容不正确时禁止推进渲染器资格，不添加跳过 readback 的选项。

### V09 descriptor identity 与一套最小 root 布局
- 修改：V08 新建的同级 `tu_nvn_emit.cc/.h` 的 descriptor 部分、`nvn/resource_abi.h`、`tests/descriptor_test.cc`；现有：`tu_descriptor_set.cc` 中 `write_buffer_descriptor_addr/write_image_descriptor/write_sampler_descriptor` 如需抽成内部 header，保留原 Vulkan 使用者回归。
- 函数：`nvnn_tu_encode_buffer/texture/sampler()`、`nvnn_tu_make_root()`；GPU bank/layout 以 gpu-execution-contract 为准。首版 NIR adapter 使用固定私有 `tu_pipeline_layout` schema，root 走 bank3 index0 UBO，不手算 constfile offset、不创建 Vulkan descriptor objects。
- 输入/输出：已解析 span/view/map generation → native slot/version 和被引用根页；内容更新不进入 identity key。
- 不做：全 NVN bindless slot 上限承诺、统一 Mali root、多级全功能 descriptor 虚拟机。
- fixture：CPU 只改 payload 不重编码；view/remap 必重编码；in-flight slot overwrite 不改变既定消费版本；动态 handle 越界受控。
- Done：byte 编码 snapshot 和 lifetime 测试；与 shader lowering 同版 ABI hash；尚未 GPU 消费不能报告该 ABI 完成。
- 停止/失败处理：保守重编码并保留旧版本直到 retire，不能回退成改写在途页。

### V10 一个明确 program 的 compiler 闭环
- 新：同级 `tu_nvn_program.cc/.h`、`nvn/nvn_resource_lowering.cc`、`tests/program_probe.cc`；参考 `tu_shader.cc`、`tu_pipeline.cc` 和 ir3。
- 函数：`nvnn_tu_compile_program()`、`nvnn_lower_resource_abi()`；先仅 VS/FS、固定 stage IO、一个 UBO/texture 的 fixture。
- 输入/输出：带 source-kind 的 program manifest/NIR → ir3 shader variants、stage linkage 与 resource ABI hash。
- 不做：宣称 NVIDIA binary 可由 ir3 直接接受、泛用 Maxwell decoder、全部 tess/GS/XFB。
- fixture：同 shader 的 synthetic NIR 版本与真实 NVN 经现有 translator 导出的中间形式分开；仅前者存在时完成标 synthetic-only。
- Done：NIR validation、compiler metadata、descriptor ABI 一致；先 `tu_compile_shaders` 中 `tu_link_shaders`，后各 stage `tu_shader_create` 的 lowering/finalize，保留 `tu6_emit_program_config` 对 varying/VFD/SP 的完整配置；真实 NVN shader bridge 未定不阻断 synthetic 编译，但阻断 L2。
- 停止/失败处理：编译失败保持 program invalid，绝不替换成 dummy shader；缓存键包含 compiler/source/ABI/options。

### V11 单一 sysmem draw 与变更驱动 state
- 修改：V08 新建的同级 `tu_nvn_emit.cc/.h` 的 draw 部分、`core/binding_recipe.c`、`tests/triangle_probe.cc`；复用 Turnip 状态 emit、tu_cs 和 chip workarounds，不手抄精简寄存器清单。
- 函数：`nvnn_build_binding_recipe()`、`nvnn_tu_emit_draw()`、`nvnn_tu_invalidate_meta_state()`。
- 输入/输出：V03 typed draw、V04 fresh deps、V09 root、V10 program → sysmem IB + 完整 refs。首版明确关闭 A7 concurrent binning；仅选择 SYSMEM 不够，`tu6_sysmem_render_begin` 本身也可能开启它。
- 不做：GMEM/binning、feedback、query/event 虚拟完成、indirect-count、多 queue。
- fixture：三角形 golden；只改 UBO payload 的第二 draw、改 IBO offset、改 root/view、meta clear 后 draw，每项只失效应失效的内容；保存编码次数而非逐 draw 计时。
- Done：像素正确；命中路径不重新搜索全部 pool、不再构造 VkWriteDescriptorSet；每 draw 仍检查当前 publication，不缓存同步结论。
- 停止/失败处理：实验中可开关强制重发所有 state 做 reference；两模式内容相同。保留上游必要尾 flush，不与这批优化一起删。

### V12 有界 replay 包与独立实验客户端
- 拟新增于独立实验树：`nvn/tests/replay/manifest.schema.json`、`clients/d2r_replay/replay_main.cc`、`clients/d2r_replay/input_adapter.c/.h`、`replay_fixture_01/`。稳定 D2R 的 `hle_nvn.c` 与 loader 仅作为阅读参考，不作修改。
- 函数：`nvnn_replay_validate/load/run()`、`nvnn_experiment_client_open()`。
- 输入/输出：具有资源初值、命令消费点、shader 来源、外部观察点和 golden 的包 → L2 接受/拒绝及首个缺口；捕获没有这些信息只能是 workload trace，不能做正确性 oracle。
- 不做：默认接管完整 D2R、热切 native/Vulkan、启动 PanVK 实现。
- fixture：自包含单 clear+draw 序列，然后真实 D2R 最小序列；删一个未捕获的依赖/资源必须拒绝，不猜上一帧值。
- Done：真实 NVN program + 真调用包才可标 L2；程序 loader 只是准备工作。实验 client 与稳定应用为独立产物；本阶段不提供稳定应用接入或默认切换。
- 停止/失败处理：实验能力不足即拒绝该测试；需要 reference 时另跑原有未修改参考程序。运行中报精确 first-unsupported 并结束实验 session，不跨后端接力。

## 6. 首批之后的必需关卡，不伪装成“小修”

- **真实 D2R shader 集合**：资源 ABI、stage IO、specialization、转换器来源、shader 缓存失效完整；V10 单 program 不代表全集。
- **NVN 图像/内存**：至少覆盖实际格式/mip/layer 的 GOB/raw 语义、buffer-image alias、CPU stale read、CPU 部分写；native asset 快路是独立 qualified profile。共享 import 不能替代这些合同。
- **同步/查询**：实际触发的 fence 条件、Event、timestamp/query readback 必须完成，不能用单队列顺序替代全部对外可观察语义。
- **WSI**：Android surface、acquire/release fence、外部 BO 所有权、resize/suspend 独立的后续实验组；不借一个 Vulkan device 偷接 native 输出而忽略外部同步。
- **真实 D2R 支持集合**：从受控 trace 输出实际 API ×参数模式×格式×shader 特性矩阵，不以 216 个静态出现的函数名为完整运行清单。每个缺项形成有 fixture 的小实验阶段，数量不能预先假装 12 个即可覆盖。
- **GMEM**：sysmem oracle 稳定后才开启；首次仅无 shader 副作用、无 feedback、无 tess 的封闭附件段，binning/once-only side effects 和 spill 测试先行。

## 7. 最优策略与 hack 启动门

先证明 CPU 绑定热路径的工作真正减少，再考虑 GPU 侧融合；否则同时重做编译器、tiling、同步和画质，任何收益/错误都难定位。

| 项目 | 默认 | 启用前具体证据 | 必须独立统计/回退 |
|---|---|---|---|
| 稳定资源 recipe、descriptor/state | 开启但有强制重建 reference | generation/alias/消费时点负控通过 | resolve 次数、slot 编码次数、fresh hazard 次数 |
| coherent guest ADDR import | 关闭 | 精确补丁/设备 tuple，双向内容、cache、生命周期/别名 probe | imported bytes/pages、fallback bytes；普通 BO 路线不阻断 |
| qualified native assets | 关闭 | asset/layout/format/mip/hash，原始字节保真或明确只读，完整 source-of-truth | raw 观察触发 generic materialize 或明确 unsupported；BC→ASTC 质量另算 |
| 场景无 store | 禁止按场景名启动 | 显式 discard 或完整 overwrite-before-any-observer 的可执行证明 | proof id/reason；无证明保留 store，上一帧没读不是证明 |
| tile-local feedback | 关闭 | compiler 当前像素/样本证明 + attachment/capacity + 顺序 | shader proof hash，任何条件变更退回 sysmem/materialize |
| 合并 submit/尾 flush | 关闭 | 无外部/CPU 观察边界，wait-only 活性/retire/fault 全过 | latency/in-flight refs，独立测量，禁止只看 ioctl 数 |
| VBO | 关闭 | 跨窗确有热点，VBO+import child 组合资格通过 | VM bind 成本/驻留；不用于掩盖 pool 地址模型错误 |
| 删 query/event、跳 draw、FP16/分辨率 | 不属于正确后端 | 仅单独用户选择的 Q profile，不能修补基础语义 | 独立画质/结果报告，不计等价加速 |

### 根表复制预算：正确基线不等于性能最优

GPU 合同的完整 bank3 snapshot 约 18.25 KiB。若每个 changed draw 都复制整表，2300 draw/frame 约 41 MiB/frame（约 43 MB），60 fps 仅 payload 写入即约 2.4 GiB/s，尚不含 cacheline 回写、读取与分配开销。这是条件上界算例，不是实测 D2R draw变化率或带宽。首版保守快照是正确性基线，不能预先称为最高性能方案。

V11 必须记录 changed-root 次数、root/table copy bytes、dirty stage/slot 分布和 CPU encode time。只要正常稳定绑定轨迹仍按 draw 复制完整表，视为未达到计划复用目标，应先检查 recipe/key 误失效；若确由合法参数高频变化造成，普通 KGSL BO 路径仍必须分配新的连续 bank/table 版本，可用模板和经证明的实际可达范围减少重建/复制工作，但不能把逻辑 stage 分页说成免费物理页 COW。进一步缩小表需要显式的新 ABI，例如按 program 压缩/rebanking，并同时修改 shader lowering、边界检查及缓存键；真正页级 COW 需要独立 VA/VBO 映射能力及组合资格，不是首版默认能力。所有版本保留 generation 和退休引用，不原地修改在途表。只有复制下降且 golden/消费时点负控不变才启用；引入间接层须同时衡量 shader dependent load 的 GPU 代价，不能只以 CPU copy bytes 判赢。

## 8. 设计复核与实验资格规则

每阶段的评审应列出：变更文件/函数、support manifest diff、错误路径、生命周期图、第一 fixture、host/compile/device/replay 四种证据的真实状态。不存在设备数据就写 pending；不存在 oracle 就写 unknown，而不是“预计通过”。禁止把这 12 个实验阶段理解为完整 D2R 的工期承诺。

专项合同优先级：`recording-abi-contract.md`、`memory-contract.md`、`gpu-execution-contract.md`、`bootstrap-experiment-plan.md` 的具体状态/接口约束优先于旧章节草图。若专项审计发现上述 fixture 仍不足，应新增失败用例，不以保持阶段数量为理由删掉门槛。


## CPU优先级补充

参见cpu-validation-priority.md。native语义/输出正确只是前置，CPU成本验证须独立记录资源解析、版本检查、root/table拷贝、命令编码与必要依赖的完整成本；随后才评价GMEM。固定全表snapshot仅是正确性基线，不具备性能方案资格。全部阶段仍是未来独立实验的设计，不构成执行授权。
