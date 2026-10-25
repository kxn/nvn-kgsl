# Turnip 原生执行契约 v3：可以开始实现的窄路径

状态：设计与固定源码静态核验；没有编译新 backend、没有 GPU 验证。本文覆盖 revision2 中“root + native heap + dirty bits”的未定义部分。所有名字以 `nvnn_` 开头的函数和结构都是**待新增接口**，不是已存在的 Mesa 导出。源码基准 Mesa `8e557130bda2adcb87535aabe23bca0d68ff77b6`。首版 A6xx/A7xx + KGSL；不得把本 ABI 直接套到 PanVK。

## 1. 实现边界与最小成功定义

首个可接受管线：一个 context、单有序队列、sysmem、关闭 concurrent binning、单 RGBA8 attachment、sample=1、VS+FS、随后独立 CS。先禁止 UBWC/LRZ 优化并使用适当的禁用寄存器状态，而不是跳过初始化。基本 depth/blend/多 attachment 是后续显式能力，不是默认忽略。TCS/TES/GS/XFB、multiview、arbitrary buffer pointer、稀疏 pool、跨线程 event forward wait、任意 framebuffer feedback 不在初始承诺内；遇到在用功能必须返回明确 unsupported 或切回完整旧 backend，不能混用半个原生 draw。

即使只选 sysmem，`tu6_sysmem_render_begin` 在 A7xx 仍可能启 concurrent binning。首版必须独立关闭此开关并核验实际 packet，不能只设 `SYSMEM`。后续 GMEM 是第二阶段，不阻止先测掉 descriptor/front-end 开销。

成功等级分开：clear-only 证明 BO/submit/readback；canonical NIR triangle 证明 compiler+linkage+program state；CS 写 buffer 证明 storage+visibility；NVN Program triangle 才证明 guest resource ABI。四者不可互相冒充。

## 2. 固定 ABI `NVNN_ADRENO_ABI_1`

### 2.1 Hardware bank 与 native descriptor

A6xx SP_GFX 和 SP_CS 各有 5 banks，A7xx 各有 8；VS/FS 不是每 stage 独占 5/8 banks。来自 `tu_descriptor_set.h:16-29`、`freedreno_devices.py:154,767`、`a6xx.xml:4220-4231,4317-4329`。本 ABI 只使用 bank 0..3，bank 4 不使用，不能擅作无限 root slot。

所有 bank base 实际分配对齐 64 bytes，编码 base OR `BINDLESS_DESCRIPTOR_64B`（值 3）；shader index 单位 64 bytes，**不是 bytes**。XML 的地址字段最低两位保留不是完整分配对齐保证；这里选更强 64B 约束。A6xx 同步写 SP 和 HLSQ mirror，A7xx 按上游只写 SP；graphics/compute 分开跟踪，SP_UPDATE_CNTL 按 chip 的 mask 更新。提取锚点 `tu_cmd_buffer.cc:4773-4860`。

| Bank | 内容 | 索引 | 可变性 |
|---|---|---|---|
| 0 | sampled texture native descriptors | guest texture slot + 1 | pool/view version |
| 1 | sampler native descriptors | guest sampler slot + 1 | sampler version |
| 2 | storage image native descriptors | guest texture slot + 1 | 同一 guest slot 可和 bank0 不同编码 |
| 3 | root UBO + 分 stage UBO/SSBO 描述符 | 以下固定布局 | 每次语义消费选择 snapshot |

0..2 的 native slot 0 是安全空 descriptor，guest slot 0 映射 1。索引转换与 count 边界先检查再加 1；不能把 overflow 或合法但未支持的 slot 截断成 16-bit。D2R 的 low20 texture/high12 sampler handle 拆解只属于其已识别 guest ABI profile，不硬编码到整个 NVN 实现。

Texture/storage descriptor 使用 `fdl6_view_init` 派生的 sampled/storage descriptor，不能互换。Sampler 拷贝 `sampler->descriptor` 实际 `A6XX_TEX_SAMP_DWORDS` words 后补零至 64B（`tu_descriptor_set.cc:1283`）。不使用 combined 128B texture+sampler 打包，避免一个 sampler 变化迫使 texture 重编码。

SSBO **不保证只有 64B**：`write_buffer_descriptor_addr:1129` 根据 `storage_16bit/has_isam_v/storage_8bit` 写一或两个连续 64B descriptor；其有效组合有断言。ABI 统一为每逻辑 SSBO 保留 128B，未用 half 置零，具体 R16/R32/R8 顺序完全跟随该函数。最终编译仍执行 `tu_nir_lower_ssbo_descriptor` 选择 half，不能简单把所有 load 指向第一个 64B。UBO 是 64B padded native record，其 range 用 vec4 单位；按 `write_ubo_descriptor_addr:1191` 编码，先校验地址/range 可表示，不凭位域宏静默截断。`a6xx_descriptors.xml:163-170` 给出 UBO BASE_HI 17bit、SIZE 15bit，因此该描述符地址最多49bit，ceil(range/16) 必须 <=32767（range <=524272 bytes），另需满足设备合法GPUVA/对齐/映射范围。SSBO 编码器把range转u32，前端先拒绝超过该范围的view，不允许隐式窄化。

### 2.2 Bank 3 的精确布局

此处 32 UBO / 32 SSBO 是**初版内部 ABI 容量**，不是声称 NVN 官方上限。manifest 若超出就拒绝此 ABI；后续 ABI_2 可扩，cache key 必须不同。

| ABI stage s | 值 | UBO binding | UBO native index | SSBO binding | SSBO native index |
|---|---:|---:|---:|---:|---:|
| VS | 0 | 1 | 1 + u | 2 | 33 + 2*b |
| FS | 1 | 3 | 97 + u | 4 | 129 + 2*b |
| CS | 2 | 5 | 193 + u | 6 | 225 + 2*b |

其中 u,b 为 0..31。Bank3 binding0 固定 index0，指向 256B root UBO；所有 UBO binding 的 size=64,array_size=32；SSBO binding size=128,array_size=32。最后使用 native index288，连续有效 289*64=18496 bytes，分配取 256B 对齐为 18688 bytes。所有 binding offset 是上述 native index * 64，`dynamic_offset_size=0`，`num_sets=4`，push_constant_size=0。图形和 compute 可以用不同 table version，但 layout 完全相同。

首个正确性实验允许复制这张18.25KiB表，但该模式尚不具备CPU收益方案资格，不能据此判定整个资源模型优化是否成功；CPU验证必须单独测完整拷贝成本，并与稳定版本复用/紧凑布局方案区分。提交前从上次 immutable template 复制，仅实际变化 stage/range 编码；后来用 per-program 紧凑 ABI 或 COW table-pages 优化必须明确 GPU 连续地址策略和新的 layout hash。不能把“页 COW”写成魔法：普通 KGSL BO 无原子页替换时仍须新连续 BO/子分配和更新 base，或者整表 snapshot。

Root BO 对齐256B，固定256B，little-endian：
- 0x00 u32 abi_version=1；0x04 u32 flags（未知位拒绝）；0x08 texture_count；0x0c sampler_count。
- 0x10 storage_image_count；0x14..0x3f 保留全零。
- 0x40 + 0x40*s：u32 ubo_count @+0；u32 ssbo_count @+4；u32 stage_flags @+8；u32 draw_id @+12；i32 base_vertex @+16；u32 base_instance @+20；u32 dispatch_base[3] @+24；余下28B保留全零。

Root 不含 MapEpoch/PublicationEpoch 等 CPU bookkeeping，也不假装能替代 ir3 的 driver params。base vertex/instance、drawID、workgroup 参数仍由对应 NIR system value 和 Turnip driver constant path 驱动；root 中同名值供 guest ABI 显式读取时使用，测试两条路径一致。没有使用字段不发 shader load。root 通过 bank3 index0 的 UBO load 访问，因此不会抢占手写 constfile 区间。ir3 仍可以合法提升/预取，依靠其 allocator 管理，禁止自己规定 c-register 编号。

### 2.3 大 pool 和索引边界

`MAX_SET_SIZE=1GiB` 是 Turnip 为 CP_LOAD_STATE6 28-bit DWORD offset 选的实现上限，源码只保守说 hardware 至少支持 2^16 descriptors，不是已证明 NVN 全部 20-bit 索引范围。ir3 动态 texture/sampler source 是 vec2 32-bit（`ir3_compiler_nir.c:1835-1977`），常量 >=256 转 S2EN，不应以短 immediate 位宽判断硬件上限。

初始能力表 `qualified_texture_slots` 独立于 `guest_pool_capacity`。先要求实际访问集合不超过已验证 native range；若 pool 容量更大但可证明静态仅访问小集合，可用显式 dense remap variant，不能仅凭上一帧没访问当证明。运行期任意索引需要完整范围编码/边界检查和最大索引 GPU 用例通过后才开放。full20bit 表需 (2^20+1)*64 bytes/bank，bank0+2 即约128MiB；不是每 draw 创建，不预先假设移动设备可承受。替代方案 shader 查 remap table 加依赖 load 是独立 ABI_2，不能无声改变 ABI_1。安全空 slot 不等于对所有 guest 越界定义了返回零：合法语义不能满足时在 native eligibility 阶段失败。

## 3. NIR、linkage、机器码之间的具体接线

首版刻意保留 Mesa **内部**固定 `tu_pipeline_layout` 元数据形状，避免第一周同时重写 NIR lowering。它不是 VkDescriptorSet/VkPipelineLayout handle，也不创建用户可见 Vulkan descriptor 对象。新增 schema 构造器只填 `num_sets/set[].layout/binding[]/size/push_constant_size/dynamic_offset` 等实际被 compiler 读取的字段；必须静态列出字段读取并补全 required state。未来去 Vulkan 类型是单独 refactor，不是速度目标。

新增接口与职责：
1. `nvnn_build_resource_schema(DeviceCaps, Abi1Schema*)`：上节四 banks，typed binding offsets，layout hash，校验 caps.max_sets>=4、descriptor size组合和 range。
2. `nvnn_guest_to_nir(GuestProgramSnapshot, GuestResourceManifest, Abi1Schema, nir_shader*[])`：输出常规 shader inputs/outputs、UBO/SSBO resource index/load descriptor 链、image/sampler deref。sampled texture set0 binding0、sampler set1 binding0、storage image set2 binding0；root set3 binding0；每stage buffer按表。guest index解码、bounds和+1在此。不得残留 guest CPU pointer 作为 GPU pointer。
3. 每stage 跑 `tu_lower_nir`（其中 `:3589` 把UBO/SSBO显式IO降为 `nir_address_format_vec2_index_32bit_offset`），再 `tu_link_shaders`，然后 `tu_shader_create`。真实顺序见 `tu_shader.cc:3950-3965`。NIR 验证放在 guest adapter 后、link 后、finalize 后；不允许在后端机器码生成后补资源 ABI。
4. `tu_shader_create:3655` 内部：`tu_lower_io:1462`（包括常量分配与descriptor conversion）→gather_info→`ir3_finalize_nir`→`tu_nir_lower_ssbo_descriptor:3705`。必须保留该最后 pass，因 vectorization 才决定 SSBO bit size。shader->active_desc_sets、conststate、driverparams、安全const variant都由这条路径产生。
5. `nvnn_link_native_program(CompiledStages, FixedFunctionKey, NativeProgram*)` 从 `tu_pipeline.cc` program state 构造抽出配置：`tu6_emit_program_config:1348/2398` 必须在依赖 config 的 constants 之前；VFD destinations、VS/FS varying linkage、FS inputs/outputs、SP state不能只凭 binary address猜。`tu_shader.cc:2524,2595,2796,2883,3050` 是具体 emitter anchors。

`tu_lower_io` 的 dynamic descriptor stride 要是二次幂；ABI 的1/2满足。动态索引不得盲加 ACCESS_CAN_SPECULATE；保留 `can_speculate_descriptor_load` 的条件、nonuniform 标记和 guest binding有效性。lane-varying texture/sampler 索引必须有测试，不能把动态误当 uniform。初版为每个 Program+ABI 输出 manifest JSON，列出所有资源 kind、stage、binding、bounds、读写/atomic、副作用、stageIO；manifest 不完整就不启用原生路径。

## 4. 状态缓存与 native block 所有权

所有 key 都包含 DeviceCaps fingerprint（精确gpu/chiprevision、compiler options/workarounds）、Mesa commit、ABI version。以下是字段契约，不是仅存 dirty mask：

- `ProgramKey`：guest shader/control bytes hash + semantic adapter version + specialization bytes + stageIO linkage + resource schema hash + subgroup policy + float/precision policy + required robustness + shader relevant attachment formats/sample/read-only feedback。没有把每帧内容 epoch放进 key。
- `ViewKey`：ResourceId generation + canonical backing span identity + MapEpoch + format/aspect/swizzle + dimension/mip/layer范围 + stride/layout/UBWC模式 + GPUVA + access kind。content改变不必重编码；重映射必重编码。
- `SamplerKey`：全部采样/比较/LOD/anisotropy/border bits + border table generation + devicecaps。border table slot生命期随sampler而非frame。
- `RootTableKey`：Program ABI + pool identity/generation + descriptor slot versions + 每 buffer view/address/range + root bytes hash；不能只比较 pool pointer或draw的句柄值。PublicationEpoch 影响依赖，不机械改变相同descriptor bytes的key。
- `ProgramStateKey`：linked variant IDs + VFD格式/stride/instancing + shader output mapping + sample shading和rasterization影响编译配置。
- `RenderStateKey`：attachment native views/physical generations + render area/layers/sample + blend/depth/stencil/rasterizer/viewport/scissor + sysmem/GMEM plan version + LRZ/CCU mode。不同组可拆 key，但总覆盖集合不能省。
- `HazardState`：按canonical backing byte/subresource范围，最近writer/readers的queue completion与访问domain、materialized representation；不与 descriptor cache混合。

NativeProgram/code、NativeDescriptor、RootTable、NativeStateIB、DrawIB各保存 referenced BO集合、relocation generations和所有 queue 的 RetireSet。已交给 GPU 的 bytes immutable；同一值新版本可以共享，同一地址新内容不能“顺手覆写”。command storage 的合法 guest patch 在语义消费点重新解析/capture，选择新的native block；禁止用 immutable native cache否定 guest command可写性。所有 IB 引用的descriptor/BO 纳入 submission residency/lifetime，不能只上报直接 command BO。

`NvnnHwState { context_generation; gfx_valid_mask; cs_valid_mask; cache_domains; }` 是 context内实际寄存器状态，和逻辑 NVN state 分离。每个 emitter 返回 `WriteSet { gfx_groups, cs_groups, cache_domains, query_state, lrz_state, render_mode }`。正常draw、compute、clear(2D/3D)、retile、resolve、querycopy、event、prologue/epilogue、恢复context都是 writers。首版任何 meta kernel/3D clear后将全部 GFX/CS group invalid，再按真实 key 重发；以后逐writer收窄。未知 packet writer=all invalid。缓存IB本身只避免再生成 bytes，不表示 GPU 当前已装载该状态。

## 5. 顺序、cache 与真实提交完成

`NativeAccess` 包含 backing span、读/写/atomic、pipeline stage、cache domain、representation。域至少区分 CPU、CP/sysmem、UCHE、CCU color/depth、CCHE、bindless descriptor cache、GMEM/blit cache。按别名span算 RAW/WAR/WAW，不以 texture对象ID替代。相同GPUVA不同view的 storage write→texture sample仍须dependency。

首版采用完整 conservative dependency emission，不从“同一submit”推断缓存一致。上游 `tu_cmd_buffer.cc:5790-5880` 明确：UCHE/CCU clean与目的invalidate关联；descriptor destination另触发 bindless invalidate；CCHE有多个不一致副本不可简单删invalidate。`tu_emit_cache_flush:393` 还有 SP_GFX/CS_BINDLESS_INVALIDATE，不能只更新 base忽略 descriptor cache。CPU改 descriptor时先按映射cache policy做host publication，再在首次GPU消费发descriptor invalidate；CPU-GPU coherent不自动使GPU所有cache互相一致。

新增 `nvnn_plan_dependency(accesses, HazardState, ExecutionPoint)` 输出 availability(clean)、execution wait、visibility(invalidate)、GMEM materialize，以及producer completion set。不能将这四项合并成一个 bool barrier。A7 BV/BR开启后目的vertex阶段wait需要双方参与（`tu_event.cc:152-181`）；首版禁并发避免漏半边，不把缺失当 hardware不能做。

保留 `tu_EndCommandBuffer:5448` 对pending+CCU尾flush的语义，最后signal/completion必须在这些写可见之后。kernel完成/UCHE行为不是CCU写回保证。以后只在native batch知道最后真实IB及外部消费者时合并中间clean，**不是**“删所有Vulkan尾flush”。NVN EndRecording不强制GPU提交；提交时点和nativeIB边界分离。

KGSL context timestamp映射单调内部u64 serial；32-bit wrap用固定窗口内模比较，禁止跨>=2^31未完成跨度。跨context/外部sync用真实dependency，退休取全部完成集合。empty/wait-only batch不伪造推进：走KGSL已实现的特殊sync处理或发合法最小GPU工作并验证有completion。用户返回的 NVN fence/query时间不是cpu wallclock，更不是内部 serial。

## 6. GMEM 与 once-only 的增量契约

首版每个 Query/Event/Sync publication、compute、copy、alias observer、非framebuffer barrier 结束 RenderEpoch。在有序一次性主CS执行，再开启后续epoch。这是合法的保守实现，可牺牲部分tiling，不改观察点。不能把所有event统一移到epilogue，例 `Signal(A); Wait(A); Draw` 会变成等尚未执行的 signal。

GMEM admission第二阶段要求：attachment集合/format/sample/extent兼容且可装入；没有任意texture反馈；跨tile读取无；没有未实现replay side effect；有效load/store/clear；外部observer和alias边界清楚；binning shader const布局与render variant相容；VSC容量溢出有上游fallback/overflow处理。过关前不启 concurrent binning。

每条 IR operation必须枚举执行域 `ONCE_PRE / BINNING_ONLY / TILE_REPLAY / ONCE_POST / EPOCH_BOUNDARY`，不可默认全部复制进draw_cs。VS/GS storage/atomic/XFB是不同于普通FS color store的副作用。初版 GMEM白名单只接受无非attachment shader write/atomic的VS+FS；其他shader保持sysmem，后续靠经过验证的binning variant/conditional exec处理，不能简单“有binning所以只执行一次”。

查询处理：
- Timestamp：按guest stage规范在原执行位置采真实GPUcounter，结果写与availability独立。`tu_query_pool.cc:2109` 在Vulkanrenderpass里每tile重写取最后值，是 Vulkan特定可接受策略，**不直接视为NVN时间点oracle**。初版epoch split后采一次；counter位宽、频率、wrap与guest tick换算用设备与guestABI证据，禁止硬编码一个GPU频率。
- Occlusion：begin/end计数差值覆盖原draw区间；跨epoch保持logical query active并累加native segment，不重复begin导致丢前段。多tile可累加per-tile，结果/availability直到全部片段完成。
- Primitive generated/XFB：上游 `tu_query_pool.cc:1425` 明确即使HW binning，tile也会再次计数，使用 SYSMEM|BINNING 条件执行。首版sysmem；GMEM时移植完整条件与begin/end accumulator，不能简单除tilecount（边界tile并不均匀）。
- query reset/reuse按query generation；GPU query destination与CPU读/间接draw别名时进入一般hazard。result可见后availability才为真，复制查询结果到guest memory必须正确宽度/stride/未就绪语义。
- Event/Sync：实现value/mask/比较/更新模式来自guest ABI，不直接假设 Vulkan boolean event。独立 recording contract确定细节；未确定的模式不得落到固定 `CP_WAIT_REG_MEM == 1`。每个signal记发布的scope，wait应用自己的consumer scope，跨queueforward wait不得把CPU提交线程阻死。

## 7. 开工测试与验收，不宣称已执行

Host/compile-only测试：schema offset/size及overflow；A6只4banks；UBO/SSBO边界31/32；64B vs128B encode golden；NIR index+1/root offset与layout一致；dynamic/nonuniform标记；link前后varying类型/locations；meta WriteSet全覆盖；retire多queue；cachekey每个字段单独变化应命中/失效。

GPU必测（每个chiprevision独立记录，不因A7通过就放行A6）：
1. Root三个stage在不同table版本读取不同常数，连续draw/dispatch交替；在途修改新版本不能污染旧draw。
2. Texture/sampler各取0,15,16,255,256,65535及qualified最大slot；lane-varying索引；第一个/最后一个合法slot与越界隔离；sampler/image分别跨bank。
3. SSBO 8/16/32-bit load/store及atomics（仅实际caps支持），129B range边界；不同half选择对比CPUreference。
4. A→meta clear/retile/CS→A，第二个A使用相同逻辑state但必须重装受污染hardwarestate；输出逐字节一致。
5. RT CCU write→同backing texture sample、SSBO write→UBO read、CPU descriptor修改→GPU sample，均跨/不跨submit，检测少flush导致旧值。
6. 一组draw分别sysmem、1tile、Ntile，比color/depth、atomic计数和query；primitive不能随tile数倍增。首版有atomic的GMEM admission必须拒绝。
7. Signal→Wait→Draw；Wait→Reset→Signal→Wait；不同stage scope；query结果写后available；query复用generation；跨epochactiveocclusion。
8. empty/wait-only batch、timestamp32 wrap模拟、最后IB写后fence CPU读；device loss不能回收未确认安全的BO并继续提交。

性能统计至少：每draw CPU资源解析次数、native descriptor编码次数/bytes、root/table复制bytes、stateIB重用率、descriptorinvalidate次数、cacheclean次数、epoch数量、tileload/store bytes、CPU encode时间、GPU耗时。Vulkan对象创建数为0不是通过指标，若18KiB表每draw复制成为热点必须如实报出。

## 8. Hardware限制与实现限制分类

A6五bank/A7八bank、descriptor索引编码和GPU缓存域是hardware机制。Tessellation sysmem、部分query需要binning、feedback禁GMEM是该Mesa硬件路径的实际实现约束，不证明NVN/API禁止。首版32个buffer/不支持GS/禁concurrentbinning是工程范围。Vulkan pipeline/layout重复构造可以删；合法依赖、tile materialization、guest布局转换、shader语义差异不能靠去掉API消失。UBO root与固定私有layout的方案用Mesa内部编译基础设施，不是重新套回逐draw Vulkan对象翻译。

### 补充：事件轮询不属于普通数据 WAR frontier

对 event word 使用 `SYNC_POLL` / `ATOMIC_SIGNAL` 独立访问类别。若普通 hazard planner 把 SignalEvent 写排在未完成 WaitEvent 读之后，会自造WAR死锁；但也不能把整页event backing排除普通hazard。协议只作用于精确word，signal携带发布数据scope，wait携带消费scope。对CP_WAIT阻塞的队列，同队列排在其后且未能执行的signal不是可推进producer；必须存在可运行的CPU/其他context producer，或由调度器在不改变guest顺序前提下形成合法依赖，否则拒绝该调度。测试包括wait先提交、外部signal后提交、同页非event RAW，以及同CP forward-signal死锁检测。
