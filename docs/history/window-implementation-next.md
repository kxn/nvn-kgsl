> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Window 接线：可立即实现的最小闭包（提案，未改 tracked）

2026-10-23。只读当前 source/core，未构建、未 ADB/git。本文件补充 nvn-window-next.md，收束为可分工接口与实现草案；当前没有导出这些新接口，也没有 NVN Window 真机通过结论。所有 NVN 类型继续使用独立第三方重建头，Android 仅 AHB/SurfaceControl，不读取/复制 Vulkan 或旧 renderer。

## 决策：第一闭包不需要先重写整个异步 KGSL 队列

当前 src/nvn.c 和 kgsl.c 的对象/handle/pending/busy 状态只支持单 owner。最小安全第一实现保持这个前提：**QueuePresentTexture 在 owner 线程先 nn_queue_finish(q,1000)，GPU 完成后才向 compositor worker 入队。worker 只调用现成 nn_present_show/detach/destroy，绝不访问 core 对象表、BO busy、KGSL pending 或 TLS error。**

这样 Present 会有 GPU fence 的同步成本，但没有在 guest Present 等 OnComplete/显示节拍；不需要先做 exact-timestamp pending retirement，也不会并发执行现有 finish/submit。它是当前有限 profile 的正确性方案，不声称异步 GPU pipeline 性能已优化。现有源没有要求 Present 必须永不阻塞的 ABI 证据；原 HLE 指出游戏常持锁调用 Present，因此尤其不能在它里面等 compositor 或依赖游戏锁的 worker。当前已支持 shader 没有等待 CPU signal 的 GPU event，不外推到尚未支持的事件流。

后续优化才是 snapshot timestamp→异步 fence/export/safe retirement；不能让现有 latest Finish 在另一个线程边清 pending 边与新 submit 竞争。

## 1. PFN 合同（本轮重新核实）

源：nvn-native/third_party/nvn/nvn.h:1174–1191；它明确为 DWARF 重建兼容 ABI(:29)，不是完整官方SDK语义。

```c
void nvnWindowBuilderSetDevice(NVNwindowBuilder*, NVNdevice*);
void nvnWindowBuilderSetDefaults(NVNwindowBuilder*);
void nvnWindowBuilderSetNativeWindow(NVNwindowBuilder*, NVNnativeWindow);
void nvnWindowBuilderSetTextures(NVNwindowBuilder*, int, NVNtexture* const*);
void nvnWindowBuilderSetPresentInterval(NVNwindowBuilder*, int);
NVNnativeWindow nvnWindowBuilderGetNativeWindow(const NVNwindowBuilder*);
int nvnWindowBuilderGetPresentInterval(const NVNwindowBuilder*);
NVNboolean nvnWindowInitialize(NVNwindow*, const NVNwindowBuilder*);
void nvnWindowFinalize(NVNwindow*);
void nvnWindowSetDebugLabel(NVNwindow*, const char*);
NVNwindowAcquireTextureResult nvnWindowAcquireTexture(NVNwindow*, NVNsync*, int*);
NVNnativeWindow nvnWindowGetNativeWindow(const NVNwindow*);
int nvnWindowGetPresentInterval(const NVNwindow*);
void nvnWindowSetPresentInterval(NVNwindow*, int);
void nvnWindowSetCrop(NVNwindow*, int x, int y, int width, int height);
void nvnWindowGetCrop(const NVNwindow*, NVNrectangle*);
NVNqueueAcquireTextureResult nvnQueueAcquireTexture(NVNqueue*, NVNwindow*, int*);
void nvnQueuePresentTexture(NVNqueue*, NVNwindow*, int index);
```

两 Acquire：SUCCESS0/NATIVE_ERROR1(:590–602)，没有 timeout/out-of-date 等扩展结果。Present/Finalize 均 void，内部 positive errno 必须以 shared first-error→owner TLS 诊断体现；不能改变返回 ABI。NVNwindowBuilder64B、NVNwindow384B、NVNsync64B，继续外置状态。SetDefaults 保留 device，清纹理/native、interval1，与原HLE:1355一致。SetTextures 复制 caller 指针数组，不保存数组地址；builder没有Finalize，建议不无限 pin资源，只在Initialize验证 caller仍活着的每个texture并正式pin。

Sync PFN位于 :1741–1746，CommandBufferWaitSync :1700。Wait结果 ALREADY_SIGNALED0/CONDITION_SATISFIED1/TIMEOUT2/FAILED3。第一闭包 WindowAcquire 可CPU等完才返回，因此 availability sync立即满足；它不是整queue fence。QueueFenceSync/任意未完成sync不能当nop，先只支持同queue已完成/明确顺序的依赖；未知条件/flags拒绝。详细 timeout 与 fence差别见前份报告。

## 2. 资源入口收敛：host-only AHB pool，然后继续标准 TextureBuilder

直接注册 pool=NULL 外部texture不可取：src/nvn.c:1055–1059,1082 的 draw资源数组包含 target->pool；:323 TextureFinalize、:555 getters都解引用pool。**复用现有完整texture→pool→BO链，新增严格 host-only 外部 image pool 导入入口。**

建议新接口签名（只是提案，不是已存在 symbol）：

```c
/* 位于独立扩展头；核心不依赖 Android 类型。 */
struct nn_nvn_external_image_pool {
    int fd;                       /* import调用期间borrowed */
    size_t fd_bytes, pixel_offset, payload_bytes;
    uint32_t width, height, pitch, format;
    void *owner_cookie;            /* allocator bundle，成功转移其销毁责任 */
    int (*release_owner)(void *);  /* positive errno；仅BO安全销毁后调用 */
};
int nn_nvn_import_image_pool(NVNdevice *device, NVNmemoryPool *pool,
                            const struct nn_nvn_external_image_pool *image);
```

这个函数在 core 里解析 NVNdevice→nn_device，同一device执行 nn_bo_import_dmabuf，再建立 POOL 对象。只接受全新pool key；验证固定64x64/RGBA8/pitch256/payload16384、offset+payload<=fd_bytes、GPUVA+offset对齐64、防溢出。成功转移 owner_cookie；失败不转移，调用者负责安全回收。core外部pool保存bo及payload范围/owner metadata，storage=NULL，flags明确CPU_NO_ACCESS|GPU_CACHED。fd无需额外dup只为import；KGSL已保留dma-buf，AHB allocator owner仍须活着。

Android facade 先 nn_ahb_create→CPU_READ lock只做mapping鉴别→nn_ahb_unlock→nn_ahb_get_pixel，构造此descriptor；cookie存 nn_ahb* 与borrowed AHB*、pixel metadata，release_owner最终调用 nn_ahb_destroy。lock内不写图、不submit；一次mapping lock不是CPU搬运出图。成功后调用者不再独立destroy这个owner。import callback只析构allocator，不做图形操作。

随后 **原PFN**：TextureBuilderSetDefaults/Device/Flags(LINEAR)/Target2D/Size64/Depth1/Levels1/Samples0/FormatRGBA8/Stride256/SetStorage(pool,pixel_offset)→TextureInitialize。target->bo原样是已导入AHB的BO，offset原样用于已验证nn_gfx_encode的NN_GFX_TARGET patch；Core graphics无需复制或改shader。外部pool只允许这个唯一完整payload texture，拒绝第二个别名texture；fd整体size仍供内核import，texture size仍16384。

必须新增防护：external pool不能作为Buffer/SSBO/UBO/command arena/shader code/descriptor table；这些路径有nn_bo_map()+offset，NULL+offset也不安全。Map返回NULL且ENOTSUP，flush/invalidate ENOTSUP；不能伪装普通cached pool。只允许target TextureInitialize那条限定用法。普通WB pool路径完全保持原状。

正常PoolFinalize：refs0→nn_bo_destroy成功→标BO_RETIRED→release_owner成功→remove_object/device refs--。若release_owner异常，保留BO_RETIRED owner bundle并poison，不能用已销毁BO再绘制或重做free；若bo_destroy EBUSY，owner一步都不能动。Window/handle持有的pool refs能阻止提前Finalize。

**边界：这不是标准 NVN MemoryPoolInitialize 的新语义，也不是普通游戏DISPLAY pool自动可显示。** 实际游戏 pool backing要另做合法presentable资源创建或经过资格的native GPU copy/blit到AHB；禁止重定向既有BO或CPU memcpy出图。当前flags只支持LINEAR，DISPLAY1/LINEAR_RENDER_TARGET0x40未覆盖；新入口不能掩盖这点。

## 3. 模块与私有内部接口

| 模块 | 最小责任 | 禁止跨界 |
|---|---|---|
| core nvn.c + entries | WINDOW_BUILDER/WINDOW/SYNC；PFN检查；pool导入；资源pin；owner线程GPUFinish与enqueue；submit期slot写资格 | worker不能find/remove对象或修改refs；UI不调用这些API |
| 新 android/window.c/.h | token registry/generation；opaque window session；2/3slot mutex+cond；compositor worker及完成消息 | 不直接改KGSL pending，不把frame测试当完整game loop |
| 现 ahb.c/.h | 单owner分配/鉴别/锁协议/最终release | 不改usage819，不读vendorints，不CPUcopy图像 |
| 现 present.c/.h | 一个SC的show/replace/detach；OnComplete/fences/ref协议 | 只受worker调用；不释放core texture/pool引用 |
| 原 VI/JNI | 稳定token注册与真实ANativeWindow attach/pause/detach | 不cast VI token为ANativeWindow，不调用旧renderer |
| 新 tests/nvn_window_probe.c | 只经Window/Acquire/Present的最小资格；N=2/3 | 不直接show绕过待测API |

建议 core与platform的内部session接口：`create(token, immutable_images[], N, &session)`、`acquire(session,&index)`、`enqueue_ready(session,index,generation)`、`snapshot(session,stats)`、`stop_and_retire(session)`。core先GPUFinish再enqueue_ready，platform不需要拿NVNqueue或nn_queue。images是不可变 `{borrowed AHB*, owner_cookie, pixel_metadata}`；整个Window寿命core pin全部texture/pool，所以worker无需每job进入core管理引用。

token registry独立于NVN窗口：原dm/gfx/hle_vi.c:8,17–18 的&g_native是稳定host token（magic0x4e49574e），其layer指针目前空且不可视为ANativeWindow。注册 exact pointer identity→generation/window，不任意解引用guest地址。首次window attach前WindowInitialize返回false/EAGAIN；同token只一个liveNVNwindow，第二个拒绝EBUSY。

## 4. 完整 slots/worker 草案（设计代码，不冒充已编译实现）

```c
enum slot_state { FREE, ACQUIRED, GPU_FINISHING, QUEUED, CURRENT, RETAINED };
struct window_slot {
    /* 初始化后不可变，外层Window refs保证整个session期间不析构。 */
    AHardwareBuffer *buffer;
    void *pool_owner;
    uint64_t pixel_offset, payload_bytes;
    enum slot_state state;        /* 以下受session.mutex保护 */
    uint64_t acquire_generation, last_ticket;
};
struct window_job { unsigned slot; uint64_t generation, ticket; };
struct window_session {
    pthread_mutex_t mutex;
    pthread_cond_t changed;       /* CLOCK_MONOTONIC */
    pthread_t worker;
    struct window_slot slots[3];
    struct window_job jobs[3];    /* bounded FIFO，绝不隐式drop */
    unsigned nslots, head, count, cursor;
    int current;                 /* -1或CURRENT index */
    uint64_t generation, next_ticket;
    ANativeWindow *window;        /* 独立owned ref，不是UI borrowed ref */
    nn_present *presenter;        /* 仅worker使用 */
    int closing, worker_running, no_more_transactions;
    int first_error, poisoned, resources_retained;
};
```

状态操作都在mutex内；不可在持mutex时调用GPU ioctl、nn_present或等待callback，也不拿guest/game锁。lifecycle只请求stop/唤醒，不进入core refs。工作线程完成只发布状态；资源析构只由core owner在安全retire后执行。

Acquire：lock；检查live generation/closing/first_error；RR选择FREE→ACQUIRED，记录generation；成功CPU已证明free，可发availability sync；没有FREE则等待worker归还，closing广播后返回NATIVE_ERROR。若全slot被callerACQUIRED且没有可进展任务，EBUSY而不是永久等待。outIndex错误设-1属本实现约定。

Present owner：验证同device/单producer queue/index/ACQUIRED/generation；预先分配job并把slot置GPU_FINISHING，unlock；现有nn_queue_finish(q,1000)及owner安全reap；lock重新检查closing/generation：失败/unknown→RETAINED+shared_error、拒新任务；若GPU成功但窗口已closing，取消该未applyjob且slot可在retire时归还；否则GPU_FINISHING→QUEUED、FIFO入队、signal，返回。无compositor等待。QueueFinish后没有任何路径允许caller再写已QUEUEDslot。

worker：在独立mutex等待FIFO/closing；没有closing则取job，校验generation，unlock→nn_present_show(buffer)。它已有preconditionGPUFinish，所以acquirefd=-1正确。成功后lock，把先前current由CURRENT→FREE，把新slot由QUEUED→CURRENT，更新current/ticket/counters并broadcast；只有这时oldslot可Acquire。调用期间如果closing，新slot仍先完成必要所有权同步，再按停止路径detach，不能把show成功当新slot已可写。异常→涉及的current/incoming/queued全RETAINED、first_error，停止新apply，不重试。

停止：closing先置位并broadcast，取消尚未apply的FIFO（它们都已GPUFinish，可在安全retire归还）；正在nn_present调用由worker自己完成/poison；worker不再发新show，正常执行nn_present_detach→destroy，再current→FREE并置no_more_transactions/worker_running0。detach任何失败所有owner保持；不能把missingSC当release。外层等待worker已退出后才join并释放window refs、texture/pool pins。

录制及重放：SetRenderTargets/Draw可记录某slot，但提交时必须检查该target当前ACQUIRED或GPU_FINISHING前的合法producer阶段；QUEUED/CURRENT/RETAINED提交拒绝。已有 untextured handles的子IB在record时已编码，h->resources仍保留target对象，所以可在QueueSubmitCommands检查其中窗口target，不必改shader或重编码机器码。外部pool禁止其他写别名，资格才能证明独占。禁止旧generation acquire的句柄冒充新acquire；handle可复用，但每次submit看当前有效acquire，不以录制时generation永远放行。

正常Finalize：core先标关闭/阻止新的target提交；没有GPU_FINISHING API并发（单owner）；owner必须先nn_queue_finish producer，覆盖已提交但尚未Present的ACQUIRED slot写入，成功后才允许workerretire并归还这些slot。producer要在首次window-target提交时绑定/引用，而非仅在首次Present才记录；QueueAcquire可提前绑定。Finish失败整组保留，绝不跨重建把仍可能GPU写的slot标FREE。workerretire成功后释放Window pins，资源仍被recording/handles pin时继续EBUSY，调用者先FinalizeCommandHandle/CommandBuffer再Texture/Pool。QueueFinalize/DeviceFinalize应同时受window的引用约束。WindowFinalize void若失败只报shared错误/保留，不删除opaque状态后让latecallback悬空。

## 5. 生命周期与有限异常预算

继承已过的SurfaceView→ANativeWindow handle路径，不能回退NativeActivity rootSurface。原MainActivity的surfaceCreated/nativeStart或nativeSurface、onPause先原存盘/挂起再SurfaceDestroyed的顺序保留；JNI只调用新registry。真实NVN token不变，平台generation随Surface重建变化。

窗口无效后不让Acquire永久阻塞：closing立即唤醒，NATIVE_ERROR+EAGAIN；generation重新attach后创建新presenter并在所有旧owner安全retire后恢复FREE。旧presenterdetach后不可复用。按原game是否会重新Initialize决定恢复现Window还是新Window，不能伪造仍在旧window呈现的成功结果。

worker持有完全独立的context/window/slot强引用，不借用即将free的activity状态。UI可请求stop并等待no_more_transactions；正常workerretire/join后归还UI引用。超时必须显式retain整组state（含被callback引用的presenter），不得UI free后latecallback写悬空内存。现helpercallback1500ms+单fence1000ms，逐项相加和先show后detach会超出全局UI预算；完整queued状态生命周期资格前需给present helper增加共享remaining-deadline（或worker完成ack后异步retire机制），不能把多帧串行join写成已解决ANR。第一最小设备入口可以先在show_completed且无queuedjob时测试正常HOME，再独立补inflight stop压力与统一deadline；前者不能替代后者的验收。

公开API已保证replace/remove/tree removal经OnComplete及optional per-SC releasefence可退休旧ref；当前present.c已枚举SC并failclosed。链接：https://developer.android.com/ndk/reference/group/native-activity#asurfacetransactionstats_getpreviousreleasefencefd 。无需重开vendor逐字内部证明阻塞，但也不能靠presentFence/QueueFinish释放CURRENT。

## 6. 最小真机资格入口：不重跑与Window无关的全套CP测试

建议唯一新mode `nvn_window`、新函数`nn_nvn_window_probe(directory,json,capacity,platform_token)`，复用已过NVN graphics fixture与原code/control/reflection、同机chip650，不改shader/游戏逻辑。controller只提供SurfaceView-backed registrytoken；probe不得直接nn_present_show。

1. 标准Device/Queue初始化，注册已过原untextured graphics image；普通pool只存代码/VBO/matrix/command arena。
2. N=2或3分别独立run；每slot分配AHB/鉴别/import_image_pool，再通过真实TextureBuilder/TextureInitialize得到可呈现target。所有constructor指纹/metadata记入结果，AHBusage仍819。
3. WindowBuilderSetDevice→Defaults→NativeWindow(token)→Textures→Interval1→WindowInitialize；验证getters。只支持fullcrop和已验证opaque方向；SetCrop非full/interval非1/未知origin先显式ENOTSUP，后续才扩。
4. 经WindowAcquire(sync)得到i→CommandBufferWaitSync验证availability→标准NVN draw该target（identity+RGB gradient）→QueueSubmitCommands→QueuePresentTexture。禁止直接rawdraw或present bypass。等待独立session stats仅作runner阶段，不伪造Sync GPU成功。
5. 最少2N+1次循环验证每slot至少两次复用；另一独立case用QueueAcquire入口。可以使用已支持Uniform矩阵/顶点数据形成可区分图，但只更新已Acquire slot资源，raw严格oracle保持，不写显示中的AHB。
6. 仅在slot真正FREE且GPU完成时AHB CPU_READ锁验证raw；CURRENT需先替换/安全detach再读。screenshot独立验证最终渐变与非对称方向。AHB map识别不是新profiling工具；不自写GPU计时/采样。
7. 正常showready→HOME/重建→恢复及finalize引用平衡；随后单独实现/验证queued/blockedAcquire销毁压力和统一deadline。迟到callback与unknown路径可host fake compositor边界测试，不提交故意损坏GPU包。
8. JSON独立schema/run_nonce/APK/source/shader/generatedimage SHA；记录N、slot acquire/queue/show/release次数、generation、slotstate快照、sharedfirsterror、BO/AHB/window/SC引用、cpu_image_copy=false、game_modules_executed=false。这仍是Window资格，不是D2R实际frame。

negative最少包括：VI假token、duplicatebacking、错误pixelspan、从externalpool建Buffer/commandarena、未acquire present、重复present、CURRENT重放handle、pool过早finalize、closing唤醒Acquire。不得用stale结果补通过。

## 7. 后续普通pool路线的真实源依据（未实现，不作为当前闭包前提）

已只读 pinned Mesa Gallium fd6_blitter.cc 的format/2D路径：:30fd6_ifmt，RGBA8映射R2D_UNORM8；:113ok_format；:1292handle_rgba_blit；:1340/1345选择buffer/texture blit；:1277 CP_BLIT(BLIT_OP_SCALE)，:1282明确CP_BLIT写CCU，需要对应flush。它证明有可研究的native2D硬件路线，不证明当前backend已经能copy。后续应独立提取最小generated-packer模板/FDL layout/源dstpatch与cache协议，再GPU oracle验证；不得复制整个Gallium runtime或假设memcpy等价。这个阶段才可把既有ordinarypool的LINEAR target接到AHB，不需要改原targetGPUVA。

## 本轮交付边界

只有本MD/JSON两份ignored文件；上述代码是接口/状态机草案，不是已编译或真机实现。本轮root推进真实D2R DeviceBuilderSetFlags；不修改其sourcefreeze。实现顺序：externalpool+negative guards→Window/sync PFN与slots→owner-GPUFinish+compositorworker→N=2/N=3设备资格→inflight lifecycle deadline→普通pool/实际游戏DISPLAY资源路径。原真实SDK/ctors/nnMain进入NVN及Exit78证据沿用父任务，不与Window资格混写。
