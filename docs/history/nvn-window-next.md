> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 下一步受限 NVN Window / QueuePresent 接线合同

2026-10-23；只读研究，没有修改 tracked/source/core，没有构建、ADB 或 GPU 调用。本文是待实现合同，不是已实现功能。设备仅限当前 pipa / Android34 / Adreno650 chip 0x06050002。父任务报告同一 AHB 原始 shader 结果直接显示及两轮 HOME/重建通过、16 个显示 RGB 采样点与 raw 为零差；这证明当前 direct presenter/生命周期资格，不能替代 NVN Window/Acquire/Present 或真实 D2R 帧验收。

## 1. 已核对的 ABI，而非根据名字推测

`nvn-native/third_party/nvn/nvn.h:29–32` 明示这是从 DWARF 重建的 NVN 53.105 公共兼容头，不是完整官方 SDK 规范。它与 `dm/gfx/nvn/include/nvn.h` 中相关声明相同；原 HLE 的签名检查来自 `dm/gfx/nvn/nvn_protos.h:55–72,469–474`。

```c
void nvnWindowBuilderSetDevice(NVNwindowBuilder *, NVNdevice *);
void nvnWindowBuilderSetDefaults(NVNwindowBuilder *);
void nvnWindowBuilderSetNativeWindow(NVNwindowBuilder *, NVNnativeWindow);
void nvnWindowBuilderSetTextures(NVNwindowBuilder *, int, NVNtexture *const *);
void nvnWindowBuilderSetPresentInterval(NVNwindowBuilder *, int);
NVNnativeWindow nvnWindowBuilderGetNativeWindow(const NVNwindowBuilder *);
int nvnWindowBuilderGetPresentInterval(const NVNwindowBuilder *);
NVNboolean nvnWindowInitialize(NVNwindow *, const NVNwindowBuilder *);
void nvnWindowFinalize(NVNwindow *);
void nvnWindowSetDebugLabel(NVNwindow *, const char *);
NVNwindowAcquireTextureResult nvnWindowAcquireTexture(NVNwindow *, NVNsync *, int *);
NVNnativeWindow nvnWindowGetNativeWindow(const NVNwindow *);
int nvnWindowGetPresentInterval(const NVNwindow *);
void nvnWindowSetPresentInterval(NVNwindow *, int);
void nvnWindowSetCrop(NVNwindow *, int x, int y, int width, int height);
void nvnWindowGetCrop(const NVNwindow *, NVNrectangle *);
NVNqueueAcquireTextureResult nvnQueueAcquireTexture(NVNqueue *, NVNwindow *, int *);
void nvnQueuePresentTexture(NVNqueue *, NVNwindow *, int textureIndex);
NVNboolean nvnSyncInitialize(NVNsync *, NVNdevice *);
void nvnSyncFinalize(NVNsync *);
void nvnSyncSetDebugLabel(NVNsync *, const char *);
void nvnQueueFenceSync(NVNqueue *, NVNsync *, NVNsyncCondition, int flags);
NVNsyncWaitResult nvnSyncWait(const NVNsync *, uint64_t timeout);
NVNboolean nvnQueueWaitSync(NVNqueue *, const NVNsync *);
void nvnCommandBufferWaitSync(NVNcommandBuffer *, const NVNsync *);
void nvnCommandBufferFenceSync(NVNcommandBuffer *, NVNsync *, NVNsyncCondition, int flags);
```

上述真实 typedef 在独立头 :1174–1191,1700–1702,1741–1746。`NVNnativeWindow` 是 `void *`(:53)，不是 Android 指针 ABI 保证。opaque 大小：WindowBuilder64B(:219)、Window384B(:117)、Sync64B(:133)，状态继续使用现有外置对象表，不写宿主大结构到 caller opaque 内存。`NVNrectangle` 是四个 int(x,y,width,height)(:137)。

| 返回/枚举 | 精确数值 | 来源 |
|---|---|---|
| QueueAcquire / WindowAcquire | SUCCESS=0, NATIVE_ERROR=1；没有 timeout/out-of-date/suboptimal 枚举 | :590–602 |
| SyncWait | ALREADY_SIGNALED=0, CONDITION_SATISFIED=1, TIMEOUT_EXPIRED=2, FAILED=3 | :1041–1048 |
| SyncCondition | ALL_GPU_COMMANDS_COMPLETE=0, GRAPHICS_WORLD_SPACE_COMPLETE=1 | :676–680 |
| WindowOrigin | LOWER_LEFT=0, UPPER_LEFT=1 | :383–387 |
| Texture flags | DISPLAY=1, LINEAR=0x10, LINEAR_RENDER_TARGET=0x40 | :749–755 |

该头没有可依赖的 Sync flags 枚举；第一合同只接受 flags=0、ALL_GPU_COMMANDS_COMPLETE，其他组合拒绝并记录 ENOTSUP，不猜 bit。世界空间完成不能静默无操作。`SetNumActiveTextures` 等名字存在于原名字表但该头无签名（nvn_protos.h:496–503）；本阶段不猜原型/不注册。`QueuePresentTexture` 没有成功返回值，不能把 errno 当其 ABI 返回。`QueueGetError` 的 errorInfo 在重建头只有 dummy[1](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `:109`；未随公开仓库提供），暂不能声称实现完整原生错误报告结构。

## 2. 现有入口、token 和 Android 窗口生命周期

原 VI 文件实际在 `dm/gfx/hle_vi.c`，不是 dm/gfx/nvn/hle_vi.c。:7–8 静态 display/layer/native token；native token `{magic=0x4e49574e, void *layer=0}`。:17–18 GetNativeWindow 忽略 layer，返回 `&g_native`。这是稳定宿主 token，绝不能 cast 成 `ANativeWindow *`。建议增加 host 内部 token 注册/解析入口（精确 token 指针白名单，不任意读 guest 指针 magic）；保留对游戏返回值的稳定身份，不暴露平台 Window/AHB/SurfaceControl 地址。

host 注册表保存 `{token, current_generation, borrowed/pinned platform_window, resumed, closing, sticky_error}`。native token 生命周期长于任何 Java Surface；每次 SurfaceCreated 换 generation，SurfaceDestroyed 清当前 generation 后保留 token。WindowGetNativeWindow 应返回原 token，而不是会变的 ANativeWindow。一个受限 NVNwindow 同时独占这个 token 的呈现 child；第二个 Window 初始化同 token 返回 false/EBUSY，不悄悄叠加图层。

已读原 Android 胶合（仅 JNI/lifecycle，没有读取任何 Vulkan 实现）：

* `dm/android/java/com/d2rmobile/nativeport/MainActivity.java:635–665` SurfaceCreated 首次 nativeStart(surface)，后续 nativeSurface(surface)，仅 resumed 后恢复；:690–699 onResume；:730–750 onPause 保存/挂起，再 SurfaceDestroyed 调 nativeSurface(NULL)。:744 surfaceChanged 当前为空。
* `dm/host/android/jni.c:781–799` 首次 ANativeWindow_fromSurface、display config、启动游戏线程；:841–891 原生命周期先保存游戏，再冻结时钟/音频/图形；:893–900 nativeSurface 挂起后更新平台 window。现有末端调用属于旧后端，本研究只把这些调用点视为替换边界，未读取旧后端实现，也不复用它。
* `nvn-native/android/java/org/nvn/nativeprobe/PresentActivity.java:25–32` 已通过的 SurfaceView + inherited NativeActivity surface 桥；直接 root NativeActivity Surface 在此 Android 可能缺 parent handle。游戏 MainActivity 已用 SurfaceHolder 输入，因此可保留其 SurfaceView 路径，不需要另建大型 UI。
* `nvn-native/android/present_activity.c:128–195` 资格控制器 start/stop/join、pause、window acquire/release；它明确只有一个 bounded frame session，不能直接当游戏 frame loop。

JNI fromSurface 得到一个拥有引用；host registry 明确接管/获取自己的引用，在完成旧 generation drain/detach 后释放恰好一次。换窗口不能只覆盖静态指针。不要在游戏工作线程使用保存在 UI 线程的 JNIEnv；平台窗口通知通过线程安全消息进入呈现 executor。

## 3. 必须先闭合资源 backing，不能只补 Window 函数

独立后端 `nvn-native/src/nvn.c:169–184` MemoryPoolInitialize 只支持调用者 cached storage→nn_bo_import；:237–249 texture_profile 只允许严格 LINEAR/64x64或2x2/RGBA8/stride256；:307–322 TextureInitialize 绑定这个 pool BO；:343–344 TextureGetTextureAddress 返回 BO GPUVA+offset。它没有 AHB owner。DISPLAY 或 DISPLAY|LINEAR 目前也会被严格 flags 检查拒绝。`android/present.c:215–223` 则只接收实际64x64 RGBA8/usage819 AHB。这两条资源创建路径尚未合并。

首个 Window API 资格建议使用显式、host-only 的 presentable-texture 注册合同：将尚未注册/未录制使用的 NVNtexture opaque 身份绑定到 `{device, nn_ahb owner, borrowed AHB*, imported dmabuf nn_bo*, verified pixel_offset, whole_fd_bytes, pitch256, width64, height64, formatRGBA8}`。此扩展不按 shader/game hash分支；不替换已存在普通纹理的 backing；不是原 NVN 标准资源创建已经支持的声明。可命名 nn_nvn_register_presentable_texture，但这是建议的新符号，当前不存在。仅 host helper 创建，生命周期由明确 owner pin/注销控制。

每 slot AHB 仍使用已过的 usage819，不改分配 usage；先 CPU lock/唯一 dev+inode VMA 识别/安全 unlock，再 nn_bo_import_dmabuf；只识别 mapping，不用 CPU 填图或搬运出图。pixel_offset+16384<=wholefdsize，GPU target patch=importBO.GPUVA+pixel_offset；CPU map 不外泄为普通 NVN pool永久map，dmabuf cache 操作沿用 ENOTSUP。Imported BO 不能代替 AHB allocator owner；二者都需 pin。

要让游戏当前普通 pool-backed DISPLAY 纹理走通，仍须另选并验证一种合法资源路径：真正 presentable backing 的资源创建/别名语义，或 native KGSL GPU copy/blit 普通目标→AHB。后者允许无 CPU 图像搬运，但现在没有已实现/已验证的 generic copy 状态，不能把它写成已经可用。不可通过 WindowInitialize 偷换普通纹理/pool GPUVA，也不可 WindowPresent memcpy/CPU lock 出图。game block-linear、DISPLAY flag组合、实际大小/format均不在本64x64资格内。

## 4. 受限 2/3buffer 状态与所有权（待实现）

第一资格仅同 device、单 producer queue、单 host API owner、单 compositor worker、N=2或3、互不别名的64x64RGBA8 AHB slots，完整裁剪、已验证 opaque alpha。BuilderSetTextures 复制指针数组并严格验证数量、NULL、重复对象/device/backing；WindowInitialize pin texture/BO/AHB-owner/device，所有失败原子回滚。默认 interval=1（原HLE:1355），SetDefaults 保留已设 device，与现有 TextureBuilder 行为一致。

每 slot：`FREE -> ACQUIRED -> QUEUED(T, generation, ticket) -> CURRENT -> FREE`。show(B) 完成且上一个 A 的 previous release fence安全后，A 才从 CURRENT→FREE；B 的 show完成只令 B→CURRENT，不能令 B可写。首次 FREE不需等无关队列。只按 available slot轮转选择，不能 next++%N 直接复用。每slot最多一个 present job，没有隐式丢帧/mailbox替换。

1. WindowAcquireTexture(w,s,&i)：先检查 generation/live/error，选择 FREE→ACQUIRED；如没有 FREE 且有可进展呈现任务，在非UI线程条件等待释放/取消；若全是调用者 ACQUIRED 而没有任务可进展，返回 NATIVE_ERROR+EBUSY，避免永久自锁。失败将 output 设 -1 是本实现约定，不冒充 SDK强制规范。成功若给了已初始化同device sync，将其标记为这个 slot 的 availability 已满足。Acquire不是全queue fence。
2. QueueAcquireTexture(q,w,&i)：验证 queue/device并绑定首个producer queue；本阶段CPU已等安全再返回SUCCESS，强于立即排GPUwait。不同queue或并发producer拒绝。两种Acquire共享slot管理，不单独各轮转。
3. QueuePresentTexture(q,w,i)：只接受当前 generation 的 ACQUIRED index，复制不可变job、capture该queue已提交timestampT、pin全部资源，ACQUIRED→QUEUED，入界限队列并立即返回。未 acquire、重复present、无效index、跨device/queue、关闭或满队列在guest API线程设stickyerrno；不应用 compositor事务。不要在这里等OnComplete：原HLE:1391–1394明确游戏常持锁调用Present；:1695–1699使用任务票据入队。
4. worker：先等待捕获的 GPU T完成（正确CCU writeback原draw已经携带），再 show同slot AHB，acquirefd=-1；nn_present_show等OnComplete与optional present/previous-release fence。成功后在完成队列发布 `{ticket,B current,A released}`；host owner消费结果才能改 object refs/global表。compositor回调只操作其独立mutex+stats，不能进入NVN对象表/拿游戏锁。
5. Draw/Submit针对 window slot 的写入仅在该slot ACQUIRED时合法；重放recorded handle也要在提交期验证。CURRENT/QUEUED 不能绘制、CPU lock或进行alias写。第一资格拒绝其他pool别名、raw GPU地址写、跨queue写，不声称完整别名追踪。尚未Acquire的 FREE也不能按既有纹理地址随意写。window持有texture refs，TextureFinalize/PoolFinalize/DeviceFinalize提前调用返回stickyEBUSY并保留。
6. GPU QueueFinish 只证明此前GPU工作完成；它不释放显示CURRENT。WindowFinalize 才停止新Acquire/Present，排队停止，安全完成GPU工作，cancel尚未apply的jobs，detach当前child，再释放所有window pins。它是 void ABI，失败设stickyerrno且保持对象/owners不析构，禁止当前原HLE的 finalize nop行为。

当前 `nn_present_show` 已允许连续替换，成功后释放 explicit previous AHB ref（present.c:172–183）；这可共享。它只持有 AHB ref，不知道外部 BO/nn_ahb/texture refs；外层slot状态必须把它们保持到同一release事件。不可通过show(A)重复提交同一CURRENT A来冒充可复用，也不能仅以buffer refcount降到某值推断fence完成。

## 5. Sync、队列线程和错误边界

当前 `src/nvn.c:10` 没有WINDOW/SYNC种类；`src/nvn_entries.inc` 也未导出Window/Acquire/Present/Sync。现有契约 nvn_native/nvn.h:7–10要求调用者serialize，内部global objects/handle表及kgsl busy/pending不是线程安全。不能让worker直接调用当前 api_nvnQueueFinish / reap_jobs / remove_object 与guest线程并发。

建议新增每queue executor/互斥及完成消息，或最小捕获timestamp wait接口：`nn_queue_wait_timestamp(queue,T,timeout)` 与现有 nn_queue_finish(latest)分开。现有 kgsl.c:242–255 waittimestamp latest然后一次释放全部pending；不能拿这个函数当“只释放T之前”的wait而仍有T后提交。若新增exactwait，pending需记各提交timestamp并按KGSL上下文timestamp wraparound规则退休；不要普通无符号<=比较。一个简单、可审查的初版是把此queue的全部 submit/wait/pending管理交同一executor，guestAPI只enqueue顺序；若不做此改动，就不能声称异步Present worker合同完成。

sync最小状态：UNFENCED / SLOT_AVAILABLE / QUEUE_FENCE(queue,T) / FAILED，另有同device身份和引用计数。原HLE :588–628 的语义依据：Initialize未fenced；未fenced Wait超时；先非阻塞poll成功返回ALREADY_SIGNALED，实际等后成功CONDITION_SATISFIED；捕获queue fence与acquire即刻available分开。Wait失败(无效/poison/ioctl真正失败)返回FAILED；真正超时TIMEOUT_EXPIRED；timeout=0必须非阻塞；uint64 timeout按原HLE传递的纳秒语义，换ms必须向上取整并防溢出，infinite值需要显式限定/实现，不能窄化截断。

第一阶段 Acquire成功已CPU证明slot可用，因此生成 SLOT_AVAILABLE，CommandBufferWaitSync可以只验证并记录这个已满足依赖；不能把任意未fenced/未完成QueueFenceSync也当nop。QueueFenceSync snapshot本queue此前提交T，绝不能绑定未来latest；QueueWaitSync 或命令wait支持范围先限定同queue且已排在producer后/或CPU已满足；crossqueue在实现真正依赖前返回失败/ENOTSUP。不要照抄原单Metalqueue的nop结论到多个KGSL contexts。

nn_nvn_error当前是 _Thread_local(nvn.c:78–82)，worker调用它不会把错误传回guest。需window/queue共享首错latch，ownerAPI消费时镜像到TLS；Acquire通过NATIVE_ERROR可见，void Present/Finalize通过sticky诊断可见；unknown GPU/completion成为不可恢复poison，所有以后Acquire失败，不自动重试/重复提交。不要假写未知NVNqueueErrorInfo布局。

## 6. 销毁/重建、裁剪与显示策略

SurfaceDestroyed/onPause：先使当前generation closing并唤醒阻塞Acquire，停止接新present/新draw，取消尚未apply jobs（其GPU资源仍等GPU完成才退休），工作线程处理已apply事务和detach，安全后归还platformwindow引用。API等待不能持有UI/guest存盘锁。保留原“游戏存盘→挂起时钟/音频/图形”的顺序；不能先把所有游戏Acquire永久阻塞导致存盘自身不前进，原HLE nvn_mtl.m:6320注释所述历史问题正是这个方向，本研究未读该实现。

Surface重建：NVN native token身份不变，host window generation变，重新create presenter child；旧presenter不可show（present.h detach后不可复用）。安全旧slots可保留allocator/GPUVA避免失效游戏handle，清除旧current，恢复FREE；新的Acquire必须重新取index，旧generation acquired/presentjob不能映射到新窗口。若游戏自身Finalize/Initialize则按正常路径重新建，不强制改游戏流程。没有平台window时Initialize建议返回false/EAGAIN，不“成功但等UI出现”；已初始化window暂时离线Acquire返回NATIVE_ERROR+EAGAIN，可在新generation自动重新attach，游戏是否容忍该结果必须独立验收。

现有helper单次callback1500ms、单fence1000ms；逐项累加并不保证lifecycle总等待<5秒，队列多帧更不能无界drain。接线需一个全局shutdown deadline及worker停止ack，所有wait使用剩余预算，必要时unknown/retain快速返回；不在UI上无条件pthread_join一个不可证明已退出的worker。deadline到期保留仍被callback访问的context、window、SC、AHB/BO；禁止free后让late callback触碰内存。主线程可在worker声明不再发起新的window事务后归还它自己的window ref，worker保留独立ref；未取得此ack必须整组隔离至进程退出，不能宣称正常清理。

公开NDK `surface_control.h:282–303` 正面保证：替换/移除buffer或从tree移除surface，框架ref在OnComplete释放，per-SC optional fence需先signal；presentFence只证明呈现时序，不能独立释放当前buffer。现有helper枚举SC后读其stats，缺SC failclosed，detach用reparent(NULL)，符合本轮已采用contract。不要再次把缺内部逐字vendor证明作为开工障碍。API参考：https://developer.android.com/ndk/reference/group/native-activity#asurfacetransactionstats_getpreviousreleasefencefd 。

第一Window资格只接受interval1/fullrect；不声称interval0或2/3已正确节拍。OnComplete等待不是任意present interval的完整实现。SetCrop初版只接受(0,0,64,64)，默认crop按这个规范化值返回；未知(0,0,0,0)惯例需原调用证据/后续支持，不悄悄裁空。后续合法裁剪可传给setGeometry source，但越界/负数/变换需单独验收。WindowOrigin两枚举真实存在；现有Geometry transform0/AHB row0映射不能自动等同两种NVN origin。先固定已验证方向并拒绝未支持DeviceSetWindowOriginMode，必须非对称角标图验证后再接通游戏要求的原点。alpha必须已opaque或实现真实transparency语义；本轮只支持原case3全255，不能对任意含透明纹理强设OPAQUE。

## 7. 建议改动边界（本轮都未改）

| 后续文件/边界 | 要做的事情 |
|---|---|
| nvn-native/src/nvn.c + src/nvn_entries.inc | WINDOW_BUILDER/WINDOW/SYNC外置类型；签名检查/表导出；builder/getters；slot refs/验证；async job身份和shared errors；public wrappers |
| nvn-native/include/nvn_native/nvn.h | host-only token/presentable-texture registration接口，明确pin/borrowed/release与限制；不修改第三方原ABI |
| 新android/window.c/.h（建议） | token→generation/worker→presenter/slot状态；隔离Android依赖；headless backend可拒绝ENOTSUP |
| android/present.c/.h | 保留已验证transaction与fence协议；如需globaldeadline/cancel，最小新增接口，callback不释放外层owner |
| android/ahb.c/.h | 首阶段不改64RGBAusage819；继续唯一VMA mapping和uncertain-unlock owner规则 |
| src/kgsl.c + include/nvn_native/kgsl.h | 如采用异步queue：明确线程串行/exacttimestamp/提交refs退休规则；不得直接并发当前finish/submit |
| dm/gfx/hle_vi.c + dm/gfx/nvn/hle_nvn.c | VI稳定token的host注册/解析；原Bootstrap/thunk真正接独立后端；保持未覆盖入口failclosed，不能让旧nop Finalize遮住新实现 |
| dm/host/android/jni.c + MainActivity.java | 仅window attach/detach/lifecycle挂接新registry，保留原save/audio/input；不导入或复制Vulkan实现 |
| 独立 tests/nvn_window_probe.c（建议） | 只经NVN Window/Acquire/Present入口驱动2/3buffer；不直接present绕过待测接口 |
| CMake/main/tools runner | 明确mode与APKfreshness，source/fixture/artifact哈希、unique run nonce、清旧结果；单独证据路径，保留当前raw/directdisplay基线 |

原 `dm/gfx/nvn/nvn_slots.tsv:63–88,468–469,497–502` 是原main导入/调用点表：WindowAcquire1处、QueuePresent1处、QueueAcquire0处、CommandBufferWaitSync4处、QueueFenceSync1处、SyncWait2处。这是静态导入/引用表，不能写成运行频次/当前真实D2R流。最小API资格可同时覆盖两Acquire入口，但游戏桥优先 WindowAcquire→CommandBufferWaitSync 的实际链。

## 8. 独立验收与不应混用的结论

1. ABI/host：每个entry按真实PFN类型编译检查；非法数量/duplicate/device/token/index/未acquire present/重复present/finalize busy全部有可见失败、无副作用；N=2和N=3分别验证状态转移。Sync四返回值、timeout0、availability与unrelatedqueue fence区分、generation失效、worker错误传回owner均覆盖。
2. 普通UIDA650：经NVN创建/注册presentabletexture并WindowInitialize→WindowAcquire(sync)→record/submit真实原VS/FS→Present重复至少两轮槽复用；另一case调用QueueAcquire。禁止直接 nn_present_show 跳过这些API。GPU oracle/guards保留原严格checks，显示仍用独立screenshot对非对称角标/渐变/alpha检查，不降级raw判断。
3. 验证每slot重新Acquire之前确已previous-release安全，CURRENT不可写；不是只统计OnComplete次数。无CPU图像copy/ANativeWindow_lock出图，无窗口dequeue私有ABI、EGL/GL/Vulkan。
4. 独立压力：有QUEUED/ACQUIRED/CURRENT时HOME、SurfaceDestroyed、恢复新generation；丢失window时阻塞Acquire被唤醒；deadline/latecallback模拟仅在host可控边界，不向GPU注入未知指令。安全路径所有AHB/BO/window/SC/fd引用平衡，poison路径显式resources_retained且不误报clean。
5. Finalize后Handle/Texture/Pool/Queue/Device各依赖顺序可审查；调用者仍持recordedhandle时资源不提前释放。提交后更新另一slot的矩阵/texel不影响正显示slot。
6. runner独立mode/build/run nonce、安装APK哈希、源码/原shader输入/生成image SHA完整；清旧results后启动、结束停止probe。现有两轮HOME图片不能作为这些新API的证据。
7. 最后再做原ARM64游戏模块启动验收：明确实际执行的原module/build/rules/产物哈希、原VI→NVNWindow实际调用、实际frame及剩余unsupported。本文受限64x64+host注册不是完整genericWindow资源创建，更不是D2R原生游戏画面已经完成。

## 父任务本轮最新进展（只记录交接，不冒充本研究实测）

父任务随后报告：完整原 RomFS 同机到位后，真实 D2R 已越过资源启动，经原 SDK/ctors/nnMain 到 NVN；首个未覆盖入口为 DeviceBuilderSetFlags，进程 Exit78。与 direct AHB 显示两轮及窗口重建一起已形成父任务证据。本研究没有读取/复测该最新设备证据，不把它扩写为游戏完整帧已生成或 NVN Window 已工作。当前优先保存这组清晰边界：真实模块链已进入NVN；接下来补实际首缺口；本文件 Window 计划为后续准备，AHB注册与普通pool backing、异步回收/Acquire背压及VI token平台映射仍未实现。
