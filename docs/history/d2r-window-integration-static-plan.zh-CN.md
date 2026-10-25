> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 真实 D2R Window 接入的最小增量与依赖

本轮仅只读原 host/非 Vulkan NVN 接口及原始 main 的三个限定函数，写 ignored 报告和静态元数据；未改 tracked、未用 ADB/git、未执行模块/GPU，也未读取旧 Vulkan 实现。当前 Window2/3 真机资格由 root 进行；本文不预判结果。原真实 D2R 最新 descriptor type7 阻塞来自 root/其他 agent 的消息，不是本轮复测，更不能说实际游戏已调用 Window。

## 1. 可立即实施的接线

最小第一步是 **同进程 exact VI token + 真实 Activity SurfaceView/ANW 注册**，然后保留现有严格资源拒绝；不是把 probe 的 64×64 AHB 替换进游戏纹理数组。

- `dm/gfx/hle_vi.c:7–8`：静态 display/layer/native 三对象；`g_native` 是 `{magic=0x4e49574e, layer=NULL}`，不是 Android native window。
- `:11–18`：四个原 HLE_FORCE 中 OpenDefaultDisplay 输出 `&g_display`，CreateLayer 输出 `&g_layer`，GetNativeWindow 输出 **`&g_native`**。这个 exact 地址就是原 NVN WindowBuilder 的 token 来源。当前两个输入 disp/layer 未校验，这是原 HLE 的已有边界，不能由此推导任意 guest pointer 可以变 ANW。
- `nvn-native/src/nvn.c:1570–1574`：SetNativeWindow 只保存 pointer identity，不解引用；`:1612–1669` Initialize 经 session_create 精确查 registry。`android/window_session.c:151–170` register/attach 需要 exact token，attach 保留 ANW 引用并产生 generation，已 attach/session 时 EBUSY；`:228–232` create 未注册/未 attach/closing 返回 EAGAIN。

建议只给原 VI 源新增一个 **host-only getter** `const void *dm_vi_native_window_token(void)` 返回 `&g_native`；四个 HLE_FORCE 的符号/签名/返回 token 保持原样。此 getter 不是新 Horizon 服务，也不是新的 guest NVN API。Android adapter 在 SurfaceView 可用、启动真实 nnMain **之前**调用 getter→registry_register→registry_attach；host 记录 token identity、ANW dimensions、generation、UID/实际进程。无需 guest 先调用 GetNativeWindow，也不能用 dlsym 访问 static g_native。getter 只有一处定义，避免为 Android 再造另一份 g_native。

另一可行但略复杂的做法是在原 GetNativeWindow 内用具名可选 host observer 通知 `&g_native`，adapter 先保存已 acquire 的 Surface，再在 observer 内完成 register/attach 后返回。不能不等 attach 就返回成功、随后赌 WindowInitialize 时 Surface 恰好存在；也不能靠忽略 registry 错误继续。推荐前述 getter，失败使用 host 具名终止诊断，不猜测 nn::vi Result 错误码。

**CLI 不伪造 token/Surface：** 继续保留当前 ABI1 run-as 边界工具。没有 Activity ANW 时不要注册虚假 ANW，更不把 64×64 probe token 注入游戏；Window session 的 EAGAIN/adapter Exit78 是正确诊断。

## 2. 进程与 ABI：现 CLI 不是可直接显示的 Activity host

`nvn-native/adapters/d2r/host/CMakeLists.txt:15–28` 引用原 main/loader/HLE/entry/thread/VI 源，排除原 JNI/旧图形；`:42–54` 构建的是 **PIE executable**，DM_HEADLESS，原 initial-exec TLS/HOST_ABI1（`:47–48`）。`tools/build_d2r_host.py:90–109` 固定 rewrite `elf/native/private-gates1`，`run_d2r_host.py:339–360` 明记普通 UID 的 **run-as shell/runas_app**，不是 Activity/untrusted_app。

即使 binary 文件名为 `libnvn_d2r_host.so`，它仍是 executable 别名；不能 System.loadLibrary/dlopen 当普通共享库。Activity 与 run-as 子进程即使 UID 相同也有不同地址空间，不能共享静态 registry/g_native，也不能把 ANativeWindow* 的数值经 argv/env 传到 CLI。跨进程可走 Android 正式 Surface/Binder/BufferQueue 能力传递，但当前没有这种通路，不是本轮最薄接法。

推荐新增一个独立 **Activity shared host target**，继续逐个引用原 loader/HLE/entry/thread/platform/main 源和现 typed bridge OBJECT、nvn_kgsl；只加很薄 SurfaceView/JNI/owner-thread frontend，不整体链接旧 jni.c 或旧 renderer。初期可留 CLI 对照；两种 target 明确区分。共享 host 必须使用原已存在的 TLSDESC 路线：`dm/host/android/platform_elf.c:10–20` 区分 DM_TLS_DESC 与 initial-exec slots；`dm/host/loader.c:213–217` 分别强制 HOST_ABI2/3 与1。优先从未改写三原 NSO 通过统一 `scripts/dev/rewrite-native.py:55–73 --abi elfdesc-gate --rules native` 生成 **新的隔离产物/manifest**，目标所有 CPU/HLE/adapter/driver 参与代码使用匹配 DM_TLS_DESC/native host TLS。不得复用/补丁 ABI1 module binary，也不能只改 manifest 声称 ABI3；保持 original input/source/options/all artifacts 哈希和 cache verify。

原 `dm/host/android/main.c:58–107` load→guest TLS→原 SDK init→constructors→nnMain 顺序继续复用，不能另写游戏入口/CPU执行器。原 APK 已用 `main=dm_android_main`（`dm/host/android/CMakeLists.txt:155`），但原 main 末尾 `_exit(0)`（`:115`）不是“可返回普通库函数”的约定；新诊断 Activity 要如实记录 nnMain 返回/进程终止，不能把返回0当初始化或首帧成功。全生命周期正常 teardown 在以后完整 host closure 中处理。

原 `dm/host/android/jni.c:781–798` 的可复用**顺序事实**是 ANativeWindow_fromSurface→display_configure→启动 game thread；`:794` 是旧后端绑定，不能照搬或用 fake 同名函数满足链接。此处仅看 host 调用边界，没有读取其图形实现。该文件还夹有旧后端控制项、checkpoint 等引用，不能整体复用为 native frontend。

## 3. 生命周期：接线正确仍不等于可恢复真实游戏

原 Java `dm/android/java/com/d2rmobile/nativeport/MainActivity.java:635–659` 首次 SurfaceCreated 选 operation mode 并 nativeStart，后续 SurfaceCreated 调 nativeSurface；`:688–698` onResume 恢复；`:730–748` onPause 先 nativeSuspend(true)，SurfaceDestroyed 再 suspend/SurfaceNULL。

原 host `dm/host/android/jni.c:850–877` 的顺序是 focus/UI lifecycle→原存档事务 dm_save_wait→clock/audio/gfx suspend，恢复在`:883–890`。新 adapter 必须复用这些原服务/已有生命周期规则；不能仅 registry_close 就声称完成游戏存档，也不能为接显示新写玩法/存档状态。当前最小 CLI 没有原完整 features/UI/audio 闭包，需逐项列出真正加入的原模块及其平台依赖，不用无条件成功 stub 补齐。

native 通用 Window 当前只支持单 serialized owner、单 producer queue。UI只能 registry_close（不进入 core/BO/KGSL），owner Finish→session retire/destroy 后才释放 texture/pool pins（`src/nvn.c:1734–1765`）。registry close 仅 compositor stop ack，不是 GPU retirement；`window_session.c:181–194` timeout0只请求，非0<=1500为单总 deadline。

**当前不能无缝重绑 live游戏 Window：** close 后已有 Window state/session 仍绑旧 generation且 Acquire拒绝；attach在旧 session未destroy时EBUSY。Window core没有“pause后原Window恢复到新ANW”的API，probe每代重新Initialize/Finalize与真实游戏生命周期不同。原 Java `surfaceCreated` 不重启游戏，原 game是否会因 focus/surface事件重建 Window 未确认。第一接入可严格在 Surface丢失后停止本轮并保留/退休，不能宣称 resume已支持。要实现恢复，须取得真实调用/原上游语义证据，定义 owner quiesce→GPU Finish→compositor退役→安全generation reattach→原Window slots恢复或原caller重建的状态机；不能把旧票据/oldCURRENT直接变FREE。实际HOME若SC stats缺失应保留，不把bounded registry_close成功外推为GPU结束。

## 4. 原始 NSO 静态证据：真实 DISPLAY 资源没有被当前 external path覆盖

原始 main SHA256 `55fd909bb7230b002357e5b0d8499c11f06888f345c49e69eb4734df44a27d0e`，三段 SHA及声明SHA均匹配，限定函数范围SHA见 `static-proof.json`。只在内存按原 nso.py 规则解压；不落原游戏 binary/code，不运行。`dm/gfx/nvn/nvn_slots.tsv:65–77,147–176` 名称/slot映射已核验。下面是**静态数据来源**，不是实际执行参数：

| main 位置 | 可确定事实 | 尚不能确定 |
|---|---|---|
| 0x17414ec..1558 | WindowBuilder Device→Defaults→NativeWindow→Textures→Interval→Initialize；设备先于Defaults | 是否本轮真实调用已到达、分支/次数/线程 |
| 0x174150c / 1520 /1538 | native来自输入desc+0；count来自+0x28；interval来自+0x2c | count是否2/3，interval是否1，token是否实际原VI返回地址 |
| 0x17413d0..13e8 | 宽高来自desc+0x18/+0x1c；两者均0时fallback1280×720 | 真实活动分辨率/Surface尺寸/resize |
| 0x174139c /13ec..13f0 | 临时默认format index0x1f随后被输入desc+0x24 byte覆盖 | 不可把0x1f当真实请求format |
| table0x1df0c24+4*0x1f | index0x1f映射public0x25=RGBA8；2D内部index2映射NVNtarget1，sampleindex0→0 | 输入实际format index，sRGB/BGRA/alpha是否用到 |
| 0x1741460 /173c..1758 | Window资源构造传内部type10，callee比较10后将TextureBuilderGetFlags结果 **OR1 DISPLAY** 再SetFlags | 基础flags（COMPRESSIBLE/LINEAR等）随输入，非本轮实测 |
| 0x1741760..17b4 | 查询StorageSize/Alignment→原allocator0x174b018→原pool+offset | 实际size/alignment/pool flags以及suballocation复用 |
| 0x17417e8..1808 | 同原pool/offset进TextureBuilderSetStorage，再TextureInitialize | 没有AHB owner/ANW；不能改成创建probe AHB冒充同storage |

`DISPLAY=1` 是 **Texture flags**（公共头`:749`），不是 MemoryPool 的新增bit。当前 pool bit中没有display capability。不得把 COMPRESSIBLE pool 或CPU_NOACCESS当已有display_buffer证据；DISPLAY eligibility也不自动意味着必须复刻Maxwell私有字节布局。native后端可以选择自有合法layout/无压缩策略，但须兑现GetStorageSize/alignment、上传/alias/copy/readback与渲染可观察语义，不能仅接受flag后保留不兼容实现。

当前固定范围是逐层真实限制，不是一个地方删64检查即可：

- `src/nvn.c:468–476,499–519` Texture只完整 LINEAR=0x10、2D、RGBA8、64²或2²、pitch256；DISPLAY1/默认非LINEAR/其他format明确拒绝。
- `src/nvn.c:1628–1646` Window只 N2/3 external64RGBA full payload；普通caller pool必拒绝。当前没运行“游戏标准DISPLAY对象→AHB”转换。
- `src/external_image.c:24–29` 只A650、64²、RGBA8、pitch256、payload16384、fdsize<=1MiB；真正1280×720/1920×1080直接超范围。
- `android/ahb.c:165–182` 分配固定64RGBA；`ahb_image_pool.c:16–30` facade固定layout；`window_session.c:196–213` slots固定64/pitch256/usage819；`window_android.c:8–12`描述符严格固定；`present.c:120` source crop64²。不能单独放开上层尺寸而底层仍裁64/导错FD布局。
- 现 graphics images/profile、Viewport/Scissor以及RT资源检查也固定64（`src/nvn.c:863,1284–1306`）。更大AHB分配或Surface显示成功不证明真实游戏render pipeline/原shader可用。

CLI默认 `display.c:8–9` 是handheld1280×720/docked1920×1080；它没有调用display_configure，也没有自动读DM_GAME_RES。`dm_opmode`默认1（`hle_oe.c:15–18`），runner environment未设DM_OPMODE。这里是配置源事实，不能断言实际game已创建1920×1080纹理。原Activity在启动前使用真实ANW尺寸/可选原game_res调用display_configure（jni:784–790）；函数范围16..4096、窄屏letterbox、handheld高720等在display.c:11–37，已有原native_display测试。不要为当前64profile把游戏强制改成64来宣称D2R已出图。

## 5. 实施拆分：接线、资源桥和真实图形是三项验收

**可立即并行、无新GPU资格依赖：**

1. 原VI host-only token getter和同进程 Activity register/attach，测试确切 pointer identity、错误token拒绝、generation/ANW引用。host内伪ANW仅测试边界，不叫真机。
2. 独立shared-host target/原TLSDESC重写记录/SDKinit和constructors阶段；原HLE_FORCE桥保持typedpre/post sticky错误截断，known wrapper遇unsupported仍Exit78。新target不混旧renderer/privateAPI，既有CLI继续有界原模块证据。
3. 精确参数观察：WindowBuilder Native token、texture数组copy（安全只通过已建对象metadata，不任意解引用gueststorage）、count/interval/init result；Texture flags/target/format/width/height/depth/levels/samples/stride/storage pool+offset/sizealignmentgetter；资源诊断保留guest_pc=unavailable。先观察再扩，不为缺参数填success。
4. 生命周期接线只做到明确停止/退役与Surface销毁引用安全；完整存档/输入/音频/重建恢复各自列原模块依赖，不因Windowprobe两代成功自动通过。

**必须独立GPU/显示资格：**

- 先等待root现 N2/N3标准PFN probe的CURRENT截图、所有原oracle、安全读回、HOME/recreate和ownerrelease结果。它只证明小尺寸、有限shader/单queue/availability Sync。
- 真实分辨率/format/native FDL stride+tile布局必须扩通用资源层，并有同layout渲染/采样/上传/读回资格；不直接采用Maxwell packaged bytes。
- 对现普通game backing，最薄可靠呈现路线通常是 **保留caller pool与GPUVA→native GPU copy/blit到受控AHB display slots**。需要真正格式/layout匹配、cache/barrrier/fence与快照寿命；不能CPU memcpy出图。现Window要求textures就是external slots，故此路线需显式扩Window“普通source texture + 私有present destination”的合同，不是假装ordinary pool已有AHB。copy只发生成功Acquire对应slot，GPU源/目的均计refs，Finish后才enqueue；退出期间两组owner均保留。
- 另一方案是native后端从资源创建时使用真正presentable AHB backing，但原caller早已为NVNpool提供原allocator storage；不能在TextureInitialize/WindowInitialize偷偷换BO、另配shadow或MAP_FIXED覆写caller页。要先证明有合法allocator/host接口，或选择上述GPUcopy路线。

目前尚未实现copy/blit，不能把这段设计当GPU能力。先支持明确的LINEAR RGBA8 nativecopy也不能覆盖未知DISPLAY默认layout。扩展选择以真实caller参数为准，不按名字猜poolflag/格式；当前其他agent的type7修复不改变Window依赖。

## 6. 下一次真实运行必须决定的值

必须记录而非猜测：实际第一次Window/Texture调用顺序、type7之后是否还先遇Queue预算或property；native token实际地址链；N/interval/crop；完整format/flags/尺寸/stride/storageclass；原pool身份/flags/size/offset/alias/使用期间更新；真实shader身份/全部资源与state；线程/queue数；HOME后真实game是否重建Window、availability Sync之外是否用QueueFenceSync；源目标GPU格式及最终Surface dataspace/alpha/origin。当前common objects table/handle表不宣称一般多线程安全。

第一层成功应报告“原ARM64 nnMain经原HLE产生VI token并进入native WindowBuilder”，第二层“实际DISPLAY backing的native处理与GPU oracle通过”，第三层才是“真实游戏提交→SC CURRENT→屏幕可验证首帧”。三者不互相代替；N2/N3 probe不是第三层。
