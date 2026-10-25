# 第三轮静态审查：内存、消费快照及失败原子性

审查对象为 revision2 及章节 01/03/06 和 interface，2026-10-23。下面是设计缺口，不声称已观察到设备 bug。P0 表示实现前必须固定合同，否则不同模块会各自作出不兼容选择；P1 表示可由首版明确禁用该能力封住。本文的“可执行 oracle”由同目录 memory-contract.md 和 memory_model_test.py 具体化。源版本见 data/source-pins.json；未重新测量游戏或 GPU。

## M01 / P0：声明 canonical backing，却没有身份来源与拒绝规则

位置：03:27–36、78；revision2/turnip-strategy.md:82–92、117；interface/nvn_native_contract.h:18–26。

反例：Pool A 用 CPU VA X 导入 memfd 的页 0；Pool B 用另一 mmap VA Y 导入同一页。KGSL ID、CPU VA、PoolId 都不同。只以 VA 重叠聚合就把两个访问当成无别名。反过来，地址 X 释放再 mmap 到另一个对象后，若 backing generation 随 Pool 而非实际 owner 管理，旧 fault/描述符可命中新映射。

修正：身份只能来自受控 allocation owner、被持有的已验证外部对象 identity+object offset，或由宿主提供并验证的 alias provenance。不能把 KGSL id、文件描述符整数、CPU VA、甚至仅 fstat 数字作为永久 backing identity。未知 userptr 的跨 VA 同页关系不可通过普通区间树证明；首版不准进入多表示/可写零拷贝快路。旧对象的 tombstone/observer token 在所有回调与 GPU 引用消失前不重用。

验收 M01：同一 backing 两个 Pool、两个地址窗口的 canonical 区间必须相交；同一 VA 重建的新 backing 不相交；旧 generation 请求被拒绝。未知 provenance 返回 UNQUALIFIED_ALIAS，不伪造唯一身份。

## M02 / P0：跨窗口 copy 回退不是“加同步就等价”

位置：revision2/turnip-strategy.md:117；06:68–83。

反例：一个 draw 的 SSBO S0=[0,128)、S1=[64,192) 各复制到一个连续 BO；shader 在 S0[64] 做 atomicAdd，再从 S1[0] 读。两个 native 地址指向不同副本，任何 draw 前后 copy/barrier 都不能恢复 draw 内别名/原子语义。

修正：同次消费的可写别名连通分量必须用同一物理表示，且全部 root 指针由同一分量基址派生。能建立有界连续 union BO 且所有访问可重定位时允许统一 copy；原子、指针逃逸、跨不连续映射且不支持 software lowering 时拒绝，不拆成多个独立 staging。读只读快照也须共享内容版本和合法消费边界。

验收 M02：上述两指针的 native 地址差必须为 64；同一地址 atomic 递增结果一致。若不能构造则 prepare 返回 unsupported，submit 次数为零。

## M03 / P0：只有 latest producer，没有 read frontier / pending reservation

位置：03:132–154；01:74–80；revision2/turnip-strategy.md:103–109、138。

反例：Q0 的 native-image 转 raw 转换正在读取版本 V；Q1 有合法等待/发布顺序的覆盖写 V+1。仅保存最新 writer 会丢掉转换 reader，Q1 覆写旧 image 后转换才读到 V+1；或者转换结束回调把 raw 错标为 V+1 的 BOTH_CURRENT。两线程同时 prepare 同一旧状态也会同时安排转换，最后一个提交覆盖另一个依赖集合。

修正：每个 canonical interval 保存 writer frontier、reader frontier、pending reservations；转换是有明确 source-version 的读和 destination-version 的写。访问准备在 owner 中预留，提交成功后转为 committed。只在已有 guest happens-before 关系上编译 RAW/WAR/WAW 依赖；不能把无序重叠竞争偷偷序列化成确定结果。转换完成只安装其捕获版本的副本，若当前 authority 已前进，不得覆盖当前版本标记。

验收 M03：Q0 转换 V 完成迟于 Q1 写 V+1，raw 的 V 副本可保留但 current=false。合法写的 wait set 覆盖老 writer 和所有未退役 readers，两个无序写被诊断为竞争而不是 max(epoch)。

## M04 / P0：分片 MapEpoch 与单 scalar ResolvedSpan 尚未接通

位置：03:78、134；revision2/turnip-strategy.md:84、90–92；interface/nvn_native_contract.h:22–26、58–60；06:87–96。

反例：[0,8192) 横跨两个 4 KiB mapping extent，缓存只记第一个 extent 的 map_epoch=7。仅第二页 remap 后首字段仍为 7，整个 descriptor 错误复用。另一故障：先校验 epoch，再 remap，再 retain owner，导致 pin 了新 owner 却仍提交旧地址。

修正：scalar 必须是不可变 MappingSnapshot registry ID，而非首片 epoch/最大 epoch；快路比较 change journal 或精确 extent generation vector。解析、记录 snapshot、pin owner 与建立访问 reservation 在一个 owner transaction 内完成。remap 发布新 snapshot，不能改旧 snapshot。内容写不改变 mapping token。

验收 M04：第二页 remap 使跨页 root 失效而第一页面积的 root 保持可复用；prepare 与 remap 的两种线性化顺序分别得到完整旧图或完整新图，永不混合。

## M05 / P0：fault 托管尚不能保证无死锁，首版不应靠 handler 驱动 GPU

位置：03:121–128、144–150；revision2/turnip-strategy.md:139、159。

反例一：被保护页同时放 guest 数据和翻译器正在持有的锁/回调数据，线程持锁后访问 stale byte 触发 fault；memory owner 为 materialize 要同一把锁，永久阻塞。反例二：CPU 等 fault→owner 等 producer GPU point→GPU 前面的 wait event 要由该 CPU 写，检测“只等范围 producer”仍不能消除这个环。反例三：递归 fault 写入未预触页的请求队列；或者 first-write 放开整个页，另一资源/别名地址继续可读 stale raw。

修正：首版的 generic observer 不是可直接开工的默认功能。只准宿主显式读写 lease，或已独立验证的受控 observation domain；保护范围须覆盖全部 alias VA，排除服务线程、锁、stack、signal altstack、请求槽。signal handler 不做解析/分配/GPU wait；platform transport 必须独立证明可重入和唤醒可靠。缺该能力时，池创建/CPU-accessible GPU-write 模式就拒绝，不等首次 fault 再失败。未知直接 CPU 访问必须继续使用已有合格 observer 或在访问前物化的受控路径，不能静默禁用。

验收 M05：fault 在任意 backend lock 持有点、owner thread、自身队列地址、递归 handler 中均不会进入 blocking owner roundtrip；未资格 domain 根本不安装 lazy-protection 模式。图模型识别 CPU→GPU→CPU wait 环并拒绝 lazy 模式。

## M06 / P0：preservation unit 不是一个向上 round-up 就完成

位置：03:111、145、158；revision2/turnip-strategy.md:140；06:99。

反例：CPU 对非 coherent cache line 的前半写，GPU 改后半。CPU flush 使旧后半回写，或 invalidate 丢掉 CPU 前半。页保护只按声明 pool 生效时，页尾另一个对象的直接 CPU store 同样绕过 tracking。压缩/tiling 转换的保存单位还可能通过另一 representation 覆盖再扩张。

修正：区分 semantic effect range、CPU ownership/cache maintenance range、转换 preservation closure。非 coherent 共享缓存行不能两侧同时取得 write lease，即使 semantic 字节不相交；按 cacheline 并集串行所有权。closure 对每个别名/布局反复扩张到不动点，不可跨未知页 ownership。首版只支持后端分配、完整页及已知 cache granule 的 coherent qualification 或整粒度受控缓存维护。

验收 M06：line 内 disjoint writes 不可无序并发；互不相交的 coherent 字节路径可不误当真别名，但仍保持页 observer 的范围分离。closure 扩张必须终止、有界且包含原写集，不能借扩张声明整页 discard。

## M07 / P0：submit 一个 status 不能表达前缀已执行

位置：interface/nvn_native_contract.h:11–15、97；01:76、90；revision2/turnip-strategy.md:105–110、144。

源证据：固定 Mesa 的 tu_knl_kgsl.cc:1694–1705 普通提交可遇 EPROTO（注释明确设备此前 faulted）；1707–1744 bind 路径逐命令 ioctl。后者不属于无 VBO 首版，却证明不能为未来 adapter 假定一个高层 submit 原子完成。首版也可能拆多 IB submission，或已完成 GPU_COMMAND 后创建 sync fd 失败。

反例：native batch 分成三次 ioctl，第一成功、第二失败；调用方看一个错误便释放整个 batch，GPU 正在读第一部分 BO。若“失败就重试”，第一部分 atomic/event 被执行两次。即使单次 ioctl，完成凭证导出失败也不代表 GPU 工作未提交。

修正：MVP 强制一个 backend submission transaction 至多一个 GPU_COMMAND，并预分配全部元数据；接受、未接受、结果未知分开记录。成功接受立即建立不可撤销 completion/retirement。后续 fd export 失败保持已接受记录，不重提工作。设备失去可验证完成能力时 quarantine，直到 context teardown 确认不再访问。多 chunk 用 accepted prefix receipt，后缀单独决定是否可提交。

验收 M07：在 prepare、ioctl 前后、receipt commit、fence export 的每个故障点注入；已接受 chunk 的 refs 不释放、不重试、不伪报成功信号；未接受 suffix 不改 authority。结果未知进入 device-lost/quarantine。

## M08 / P0：retirement union 已有，但 completion point 生命周期还不完整

位置：interface/nvn_native_contract.h:31–34、65、68–70；revision2/turnip-strategy.md:109–110。

反例：context C 被销毁重建，timestamp 再从小值开始；旧 completion 的 queue_id 被复用。或者 uint32 timestamp 从 ffffffff 回卷到 0，用普通 >= 判断已完成导致过早回收。另一个问题是 descriptor GPU 消费已结束但 raw command decoder/fault 请求仍持同一 owner，只查 retirement_set 就释放 host 元数据。

修正：QueueKey 含 context generation；host submission serial 与 uint32 KGSL timestamp 分离，保留已提交 point registry，遵从固定驱动的 wrap-aware 规则和资格限制，不自行 uint32 比大小。可释放条件是 guest-visible logical refs=0、prepared refs=0、CPU/observer refs=0、全部 accepted GPU consumers 已终结且 cache/teardown 要求满足。lost 不等同 completed。

验收 M08：wrap、context recreate、observer 延迟回调、未提交 prepared snapshot 都不能造成 premature free；销毁逻辑 Pool 只禁止新 resolve，旧 prepared snapshot 自己持 owner。

## M09 / P1（首版明确禁用未资格 raw patch）：command_version 不是一致性快照协议

位置：03:63–67、90、97；01:151–152；06:89–96。

反例：编译器先读 command_version=5，CPU patch handle 的低/高 32 位之间被抢占；编译器读到混合 handle，再读 version（dirty 只在下一 flush 更新）仍为5。没有明确封口边界时 double-check 也无法发现写入。旧 raw 段被释放重用后，相同 handle+version 还能误中 IR。

修正：typed 模式的 record-value 参数不可变持有；raw 模式必须有合法发布边界与 storage generation，先结束 writer lease 再复制命令字节到不可变 decode snapshot，随后仅据该 snapshot 解码。slot 录后允许更新与执行期读取分别资格化，不能靠任意 memcpy 全部资源掩盖未定义消费时点。无法形成稳定 snapshot 的 ABI profile 不启用 raw-compatible。

验收 M09：patch→seal→decode 必须得到完整新 handle，seal 前并发 patch 返回 BUSY/非法竞争而不是编译混合值；同 VA storage recreate 必须 miss。仅 command bytes 复制，indirect/uniform 执行期内容仍保留执行期语义。

## 可开工边界

上述 P0 不要求先造通用信号处理器、VBO 或完整图像 roundtrip。第一个验证阶段 应实现 canonical registry、mapping snapshot、range reservation 和 submit receipt 的纯 host 合同及故障注入测试。GPU 集成只接受已拥有/可证明别名的普通 KGSL BO，以及资格化宿主访问；保留旧 provider 承担尚未覆盖的 D2R 调用闭环。具体 structs/算法与扩展 gate 在 memory-contract.md。

## M10 / P0：把所有访问都归 READ/WRITE 会使 WaitEvent 与 SignalEvent 死锁

位置：03:49–54、124、132–154；revision2/turnip-strategy.md:152。此项同时修订 M03 的普通数据 frontier 适用范围。

反例：Q0 WaitEvent 轮询 word E，Q1 SignalEvent 写 E。若 Q0 poll 被记成普通未退休 reader，Q1 写就得到 WAR 等待 Q0；Q0 又等 Q1 signal，后端凭空造环。合法并发 atomicAdd 同一位置也不能一律按“没有 guest happens-before 就 unsupported”。

修正：访问类显式区分 DATA、SYNC_POLL、SYNC_SIGNAL、ATOMIC_RMW。同步 word 使用事件 generation、比较谓词、signal value、scope、cache协议，signal 不等待该 word 的 poll reader。poll/signal 的队列先后与真实 event wait 仍保留；不能排除整个页的普通数据 hazard。合法 concurrent atomic 须保证唯一 backing、设备支持的粒度/内存序，不能拆副本；非原子重叠继续按普通访问规则。event 与普通 buffer alias 同址时需要已核验同步语义，不能无限泛化 signal 例外。

验收 M10：wait→signal 图只有真实 wait 依赖，不出现 signal→poll-retire；同页别字的数据读写仍建立正常 hazard；supported concurrent atomic 不报普通数据竞争，拆分表示则拒绝。
