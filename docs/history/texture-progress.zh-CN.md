> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始纹理 shader 与 NVN 描述符资格

本轮已在替换的 A650 上，通过原始纹理 VS/FS 的直接 KGSL 绘制及 NVN 图形接口调用。开发仍在 `codex/nvn-turnip-study-20261023` 的独立 worktree；没有改原共享 checkout，也没有改游戏 binary 或执行真实 ARM64 游戏模块。

## 着色器与硬件结果

使用已核验原始 NSO 中的 VS `96757bb36a024b91`、FS `a7672f1d3bfeb7d0`，没有替换 shader、借 PC shader 输出生成期望值或使用 CPU 指令解释器。闭式 oracle 来自原 Maxwell 的手工推导。位置、UV、颜色分别为 RG32F/off0、RG32F/off8、RGBA32F/off16，stride32；VS/FS 各有 64 字节 Uniform0。纹理是 2×2 RGBA8、linear、pitch256、size4096、nearest/clamp、单 mip。

四组测试依次改变纹理内容、UV/颜色/片元常量、矩阵/gamma/contrast；重复使用同一录制命令。GPU 完成后逐像素检查，shaded 只允许一个 UNORM 编码单位；discard 和几何范围外必须精确保留初始背景。四组共 65536 个通道，11520 shaded、3840 discard、1024 outside。直接 KGSL 还检查 142464 个 guard 字节和 21376 个只读字节，全部通过，清理无错误。证据见 [原生纹理执行](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-texture-native.json`；未随公开仓库提供）。

## NVN 语义接线

原 FS 从 CB2 word110 取 texture、word346 取 sampler，OR 后采样。其偏移分别为 `0x168+8*10`、`0x568+8*0`，所以是 separate texture10/sampler0。combined API 是 `0x20+8*i` 的独立域。映射依据包括原始 shader 解码、严格前端反射、项目原 Metal 的 driver CB2 构造，以及 [Switchbrew 的 MAXWELL_B 逆向记录](https://switchbrew.org/wiki/GPU_Classes)。这些是有来源的实现依据，不能视为完整官方 NVN 规范。

新增 typed 注册 metadata 保留 stage、原 CB/word、API domain/slot、native index。每次提交再按录制的 pool 身份和 handle 的低20/高12位选择池条目。测试刻意使用 texture ID257、sampler ID259，硬件 indices1/1，与 API slots10/0、源 words110/346 均不同。

API descriptor pool 保持 32B 槽距和 256 个保留条目；这是已声明的兼容 profile。Register 写入带校验的软件描述符并保存布局、backing pool/offset 或 sampler 值，不能直接把 Adreno64B 描述符塞进原32B空间。纹理和采样器源对象可在登记后 Finalize；条目继续保活 backing。当前不支持绕过 Register 任意手工编写 Maxwell TIC/TSC，也不声称这些软件字节等同原厂私有格式。

Draw 固化 selector、池身份和非描述符状态；ENQUEUE 只解析可达条目，生成每次提交独立的 parent/child BO 与硬件 descriptor。原 command arena 不被提交修补；旧提交持有旧 BO，新登记影响下一次提交。这对应 ZIP `recording-abi-contract` §7 的 typed-v1 消费模型。主机版本/寿命测试验证了两次未完成提交使用不同 backing 时，前一批 parent/child 字节不变且旧 backing 仍 busy；这是主机所有权证据，不冒充已做 GPU 并发压力测试。每次物化会新增 BO 分配和命令复制成本，当前先保证语义；尚未以 profile 证明或宣称整帧性能收益。

NVN 真机调用者完成以下鉴别：

- 仅 combined10、错 separate texture11、错 separate sampler1 均不能满足所需绑定；Draw 拒绝且不增加 command bytes。
- bulk handle 数组在调用返回后被改写，录制结果仍正确；无关槽放入 poison ID 不改变输出。
- 先录制 Draw，再登记纹理/采样器；首次提交读取新登记条目。
- 同一 handle 下，将相同 texture ID 改登记到另一 backing，并把旧 backing 改成错误颜色；下一次提交读取新 backing。
- 两个 pool 使用相同 ID，B 在 A 之后登记；主四组仍选择 A，第五组明确选择 B 并得到全屏 magenta。
- 已登记的源 texture/sampler 可 Finalize，backing/descriptor pool 与 Program 的有引用或在途销毁明确拒绝。

主四组 65536 个颜色通道全部通过；第五组另计 16384 个通道。总计5次 GPU 提交、2个录制 handle、173360 个 guard 字节和757760个只读字节检查通过，cleanup0。完整计数、构建指纹和五张实际读回图见 [NVN 纹理资格](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-texture-nvn.json`；未随公开仓库提供）。新增采样器与池接口后 resolver 共154入口；入口数不等同于完整 API 语义覆盖。

已有资源/ClearBuffer、四个 CS 的12组及4次句柄复用、无纹理图形4组均在当前新核心上重建并真机回归通过。见 [资源回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `texture-resource-regression.json`；未随公开仓库提供）、[计算回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `texture-compute-regression.json`；未随公开仓库提供）、[无纹理回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `texture-graphics-regression.json`；未随公开仓库提供）。

## Android 出图准备

`--ahb-inspect` 已在普通应用 APP_UID 下实际调用 AHardwareBuffer 分配、describe、借用 fd 查询与 CPU lock/unlock：64×64 RGBA8、usage819、stride64像素/256字节，两 DMA-BUF 大小分别16384/32768；四角16个跨锁 CPU 哨兵正确。没有解释22个 vendor integers，也没有仅凭 fd 顺序指定像素/元数据角色。证据见 [AHB 真机查询](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-ahb-properties.json`；未随公开仓库提供）。

`AHardwareBuffer_getNativeHandle` 不在公共 NDK header 中，但其平台声明及导出符号有公开 Android 源码依据；已在本机链接并在新手机实际调用。该结果只资格此设备的平台扩展，不承诺跨厂商可移植。查询程序零 GPU 提交、没有窗口呈现，不能算已经零拷贝出图。

后续增量已在真机上按 CPU lock 指针所在共享 VMA 的 device/inode，与自有 fd 的 fstat 身份作唯一匹配。write/read 两次均得到 fd index0、offset0、payload16384；整段由同一 `rw-s` VMA 覆盖，并满足文件大小边界。这次是通过实际映射关系识别当前分配的 CPU 像素 backing，依据不是 fd 顺序或大小猜测。见 [AHB 映射鉴别](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-ahb-mapping.json`；未随公开仓库提供）。它仍不能直接证明 GPU tile/UBWC 布局。

已继续通过 `GPUOBJ_IMPORT / DMABUF` 导入由映射关系鉴别的像素 fd。这个接口的 `priv_len` 是 fd 结构体大小，不能照搬 ADDR 导入的 BO 字节数；flags 不强设缓存模式，实际 `GET_INFO` 核对映射大小、GPUVA 和可写/非 secure。借用 fd 不由调用者关闭，AHB owner 保留到 GPU 完成；导入 BO 不提供 CPU map，并拒绝通用 `nn_bo_cache`，CPU 同步全部经 AHB lock/unlock。内核依据为 [已固定的 pipa KGSL 公开源码](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/blob/77dc8e2290f3f5f15861821564f68d30ea193449/drivers/gpu/msm/kgsl.c)，不是对运行内核逐字相同的断言。

新机普通 APP_UID 的实际资格测试先由 CP 向四角写16字节，再核对完整16384字节：除四角外必须保持非均匀初始 pattern。随后同一原始无纹理 VS/FS、同一 IB 在 AHB 上完成全屏红、平移右半绿、完全裁剪、RGB插值四组绘制，65536个颜色通道通过；79040个非目标 guard 字节、12944个只读字节、2次外部分配器缓存接口拒绝、5次在途目标销毁拒绝也通过。共5次提交，10次CPU lock/unlock，资源清理错误0。CPU仅初始化背景，未把渲染结果或期望图复制到目标；GPU直接写入AHB。证据及四张实际读回图见 [AHB GPU 资格](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-ahb-direct.json`；未随公开仓库提供）。四张结果还与此前 NVN 离屏 GPU 图逐字节一致，见 [结果比较](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `ahb-offscreen-comparison.json`；未随公开仓库提供）。

这只验证当前芯片、64×64 RGBA8、usage819、pitch256、offset0的分配与linear目标配置能相互读写；不能据此泛化任意gralloc/UBWC格式。AHB payload之外没有CPU映射检查，不宣称有目标尾部越界证明。尚未调用compositor，因此不是屏幕显示、direct scanout或完整窗口接口通过。AHB helper从原属性探针抽出单一映射解析器，严格ARM64/API29链接通过；原CPU映射/16哨兵测试重建后在同一手机回归通过，见 [helper主机检查](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `ahb-owner-host-check.json`；未随公开仓库提供） 和 [真机回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-ahb-helper-regression.json`；未随公开仓库提供）。所有测试均由runner停止并核实退出，未再使用原A735手机。

同一AHB现已通过 `ASurfaceControl` 实际上屏；两轮窗口呈现、HOME退后台销毁、前台重建及最终退休在新机普通APP_UID下通过。每轮仍先通过5次KGSL提交、4组原始shader/65536通道资格，再把最后RGB插值图的AHB owner交给呈现层；绘制BO/queue已完成并释放，不把CPU期望图或读回图拷入显示目标。呈现只提交同一 `AHardwareBuffer`，没有改变usage、布局或RGBA。GPU Finish之后的acquire fence为-1；这是当前阻塞式正确性链，不宣称异步吞吐优化或硬件overlay直扫。

原NativeActivity窗口在此机上 `createFromWindow` 返回不支持；[官方NDK问题1983](https://github.com/android/ndk/issues/1983)记录了相同API33/34限制。最小 `PresentActivity extends NativeActivity` 取消root `takeSurface`，使用 `SurfaceView` 并注册基类已有 `SurfaceHolder.Callback2`，继续复用原生窗口与生命周期桥，不新增游戏逻辑或渲染API。这个资格窗口尚不承诺游戏IME/contentRect完整接入，后者仍须沿原游戏Java/JNI宿主处理。

工作线程完成绘制与呈现后等待生命周期信号，不持续提交GPU工作。onPause/onNativeWindowDestroyed先通知工作线程退休并join，再释放自身window引用。退休通过reparent(NULL)移除图层，等待OnComplete及任何返回的previous release fence；不能仅凭fence值-1判断已释放。依据是[NDK remove-tree回收合同](https://developer.android.com/ndk/reference/group/native-activity#asurfacetransactionstats_getpreviousreleasefencefd)；Android14 [SurfaceFlinger移除层处理](https://android.googlesource.com/platform/frameworks/native/+/android-14.0.0_r1/services/surfaceflinger/SurfaceFlinger.cpp#3998)也有latchAndReleaseBuffer路径。这里没有逐字验证运行中的vendor实现。缺少surface stats、超时或未知完成均拒绝回收并保留到进程退出，不重试提交。

本次两轮共2次show、2次detach、4次完成回调，实际等待2个present fence；没有额外release fd返回。2次window acquire/release、2个AHB owner退休均平衡，cleanup无错误、无未决资源。ADB实际截图按SurfaceView报告的屏幕位置(0,60)、2880×1700目标采16个内部RGB点；两张接受截图的48个颜色通道与原GPU读回值完全相同。显示检查允许8个编码单位用于缩放/色彩管线边界，原始4图GPU oracle的严格标准不变。恢复时一次系统窗口动画截图不匹配，作为失败尝试保留；随后截图正确。完整构建、实际PNG、4张读回图与逐点记录见 [显示和生命周期真机证据](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-present.json`；未随公开仓库提供）。测试后已停止并确认probe退出。

显示/生命周期通过的是独立原生资格调用者，尚非真实游戏帧。后续已在同机实际完成原始D2R模块的SDK/构造器初始化，并由真实nnMain进入typed NVN边界；完整原RomFS核验后首个缺口为DeviceBuilderSetFlags/Exit78，见 [真实接入增量](d2r-next-integration.zh-CN.md)。深度、一般纹理/状态、手机端新shader miss编译及真实游戏帧仍未完成。D2R 启动还存在 DeviceBuilderSetFlags、API版本查询、GPU-only pool 等实际依赖；不能用154个名字已解析来宣称游戏已经可跑。当前154入口的 bridge OBJECT 与 archive 已重新构建并核验 AArch64/24B swforce及缓存产物；新增的真实PFN签名包装还会在调用前后检查sticky错误，并以API/errno/phase明确退出，避免unsupported参数留下未写输出后游戏继续运行。合法false/zero/float/struct返回值保持原样，476个typed入口、1个专用resolver和46个未知ABI fatal边界已编译验证；host行为检查与ARM64编译不算真实game执行，仍没有选入真实host。接入顺序和原始调用点证据见 [D2R 接入审计](d2r-next-integration.zh-CN.md)。
