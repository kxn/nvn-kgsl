> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 同进程原宿主生命周期：只读下一步设计

2026-10-23；只读源码/既有执行证据，不构建、不ADB、不执行游戏。本报告不是恢复资格。

## 已成立与当前缺口

真实 SDK v4、constructors 已在 untrusted_app/ABI2 同进程运行返回；完整46文件RomFS后的原 nnMain 已运行到 ProgramSetShaders errno95 after 的具名终止，尚无真实游戏 Draw/Window/存档。标准 Window N2/N3 两代共40次呈现证明的是测试程序重新创建 Window，不是同一 D2R 世界、同一 NVN Window 的恢复。

1. controller.c:190–220：detached owner 调原 host_run 后一直在原 main/nnMain 内，直到入口返回才 close、release ANW、写 result。UI置 stop 不会中断/暂停原 ARM64 线程。controller.c:222–252：pause/destroy 都永久置 stop，resumed 只置 resumed=1、不清 stop；started/pending 时新 Surface 返回 ENOTSUP。controller 引用和 ANW 引用防 UAF，不等于原线程已结束。
2. activity_host.c:19–42、44–70：原 host 的 started 一次性；启动后 attach ENOTSUP，第二次 run EALREADY。close 只关 registry，不做存档、游戏线程握手或 GPU Finish，host/dlopen 常驻。不能清 started 再跑 SDK/构造器/nnMain：全局模块、线程/TLS、HLE状态均已有实例且没有卸载合同。
3. window_session.c:160–170、181–194：close 移除 registry 的 window、标 closing，session 尚在时 attach EBUSY。250–282：closing 唤醒 Acquire 并返回 ECANCELED；票据有 generation+serial。src/nvn.c:1743–1769、1785–1798：这些错误进入 sticky，typed bridge 随后具名终止。不能把 close 当临时停呈现再直接 resume，也不能把取消改成伪成功槽。
4. src/nvn.c:1800–1816：只有 serialized NVN owner 路径先 Finish producer Queue，再 session_retire+destroy 成功，才释放 texture/pool/external pins 和 producer refs。registry close 的 SC acknowledgement 不是 GPU 退休；任何 timeout 必须保留 owner/pins，不能 UI free。
5. src/nvn.c:1671–1697：Window 仍仅2/3个64×64、pitch256、RGBA8、外部AHB、interval1。真实游戏 DISPLAY 普通storage、实际大小/格式未获资格。Surface恢复与真实DISPLAY桥接应分别验收，不能用64AHB测试替代。

## 原有生命周期/存档来源

- 原 Android MainActivity.java:730–749 在 onPause/surfaceDestroyed 调 nativeSuspend(true)，reset input；688–698 有 Surface 才 resume。原 JNI jni.c:841–890 是原生命周期调用顺序证据，未读取旧图形实现：focus3 → dm_ui_lifecycle(1) → present-discard → 局内 dm_save_wait(2.5) → guest_clock_freeze_for(1,1) → audio suspend → graphics suspend。resume 反序恢复再 focus1。不要直接链接整个旧JNI：它还依赖旧renderer/缓存/输入等大量平台路径。
- hle_oe.c:65–75 的 dm_focus_set 原子更新实际状态并排入原通知15；127返回状态；332–355由原游戏 TryPopNotificationMessage 消费，359的既有 Resume开关以focus通知表达。此模块已在 shared target 中，最小焦点接线可以复用，不需重建Horizon服务。但仅发focus3不能证明存档或线程停止。
- diag.c:1231–1242 的既有 dm_request_save 并非通用HLE：它在局内把既有 server G+0x5800 上次存盘时间置旧，交原游戏下一拍走原自动存盘。不能把它说成平台纯API，也不能新创存档格式/玩法。diag.c:1721–1732 的 dm_save_wait 请求并等待 dm_fs_commit_count 增长、最后一次提交静默0.2秒，最多max_s；asked仅表示请求到了，commits=0仍未确认存档。
- 暂停菜单时原游戏无tick；diag.c:1681–1710 复用上游既有观测门10–13冻结世界，临时释放两个原暂停状态位，提交后恢复。属于原游戏专用行为，不能塞入通用NVN或无证据另写地址补丁。
- 当前 canonical native/elfdesc bundle main.gates.tsv 只有 obs1–5，没有10–13；dm.h:60 定义这些观测号。DM_PRIVATE_GATES=1 这个参数本身不证明现产物具备暂停存盘门。不能盲切 touch-run-v4（其规则还有其它玩法/画质改动），也不能给已加工binary补字节。若确需这段，必须审既有上游门在统一规则中的最小启用范围，从原NSO重建并核manifest；缺门应如原函数返回不支持，不能声称暂停存盘通过。
- hle_fs.c:423–424 是提交成功计数；425–473 原事务Commit拒绝poison写，clone+atomic swap成功才增计数，失败走front_save_failed。验收应使用实际提交计数和已提交目录，不以asked/固定等待/入口返回代替。不要从runner在外部强行Commit工作目录。
- hle_os.c:102–117 的 guest_clock_freeze_for(1,...) 已可复用，按原因冻结客体时钟；它不挂起所有线程、不证明CPU/GPU静止。shared CMakeLists.txt:15–28 未选 diag.c、dm_cursor.c、hle_audio.c、hle_lifecycle.c，且47是DM_HEADLESS，不含DM_NATIVE_FEATURES。不能只添加函数声明就假称旧存档/音频/图形全在。
- hle_lifecycle.c:15–33 的旧shutdown依赖Apple dispatch、旧graphics suspend，1.5秒后 _exit；本体不请求存档。不宜搬入Android当“安全退出”。

## 最小实施分解及次序

A. 继续实际NVN/shader/DISPLAY闭包，保持一次性 Activity bring-up。SDK/ctors/nnMain到明确阻塞仅记入口里程碑。现在每次已确认退出的实例不为补存档重新启动；还未进局不能杜撰一次“存档成功”。

B. 第一份可独立评审的生命周期接线：shared host 增一个版本明确的 native-only request/state接口，由UI仅投递实际focus/background事件和递增请求号，原 hle_oe dm_focus_set 发通知；保留close与suspend不同状态。队列必须保序且重复pause可合并、resume不能越过尚未完成的pause。事件已投递、原游戏已消费通知、save requested、commit observed、GPU quiescent、SC detached各自记状态，不共用一个passed。需要同步更新现5API的header/exports/controller typed加载/工具审计与HOST mocks。该小步只可称focus接线，仍不宣称可恢复。

C. 存档闭包：从既有原实现提炼/复用必要的请求与等待，明确游戏版本、原观测门资格；不要全开DM_NATIVE_FEATURES引入相机/玩法优化，也不要重写游戏save。在不会阻塞UI的单一生命周期控制路径运行，若该线程调用原game stat函数必须 guest_thread_init，保持原TLS/ABI，序列化原save_unpause状态。先保证原game tick在等待期间仍能执行并且render/SC关闭不会导致bridge立即fatal；当前closing→Acquire fatal正是尚缺的前提。不能冻结时钟或拦住所有guest入口后再等下一拍存档，形成自锁。原JNI最多2.5秒等待不是“UI可无限join”的依据。零commit/缺门/超时/写失败都报告incomplete，并保留已提交版本。

D. 原生停绘与安全退休：需要明确实际game的NVN调用线程/producer Queue，再由相同serialized owner在安全边界确认停止接受新帧、完成在途GPU、处理已Acquire未Present的写入。owner当前陷在原nnMain，不能简单往controller owner投递任务并假定它会执行；需原HLE/调用门的实际合作边界或独立请求由原owner消费。UI始终不调用NVN/KGSL、不join。Surface将销毁时平台close最多1500ms只证明SC停止ack，GPU退休另证；现有close0不能被提升为保存完毕/线程静止。SC脱离和保存期间的present-discard或Acquire等待必须有真实资源/时序合同，不能凭空返回成功或把暂停变成不可恢复sticky。

E. 同一游戏Window重建：只有D成功，才能在同一token下回收旧session，保存原textures/pool/producer引用，再为新ANW新generation创建会话；清空旧Acquire票据、availability及pending呈现，旧generation所有调用拒绝，等新会话ready后才允许实际Acquire/Submit/Present。若改变尺寸/格式必须先实现原游戏相应重建路径，不能继续写旧DISPLAY backing。是否由原游戏Finalize/Initialize还是host-onlyrebind接口驱动，当前真实日志尚不能决定；必须观察实际游戏行为后选，不应把测试程序新建Window路线当答案。任何旧session/SC release/GPU退休失败保留上下文，重新attach仍failclosed。

F. 恢复验收必须是同一原game进程/模块/世界：原focus/时钟/音频状态正确恢复，有实际存档提交证据；HOME后没有后台空跑；新Surface到来后同一gameWindow或原游戏真实重建路径出新帧，旧票据/旧ANW不再被访问；至少两轮HOME/返回含在途帧、菜单与局内/暂停菜单三个状态，并核稳定存档。仅“看到新generation”“程序又启动”“64RGBA probe再次显示”不算D2R恢复。冷重启可作为明确的独立产品模式，但不能冒充同进程恢复，且重启前仍须上述保存闭包。

## 当前不能确认

真实 ProgramSetShaders 阻塞之后的render线程分工、DISPLAY布局/尺寸、原游戏收到focus3后是否销毁Window、进入局内及原存档触发/成功提交、原ANW销毁时有无未完成NVN帧均未实际观察。不能据已有SDK/ctors通过给这些填成功值。
