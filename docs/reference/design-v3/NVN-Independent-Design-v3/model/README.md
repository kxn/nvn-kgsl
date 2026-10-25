# NVN native backend：纯主机参考契约测试

## 边界与执行

这些是设计的可执行反例与参考状态模型，不是 NVN 实现、驱动模拟器或硬件验证。没有加载或运行 Mesa、游戏、着色器、内核 ioctl 或第三方代码。只运行本目录自编 Python 标准库代码。

```sh
cd model
python3 test_contracts.py
```

成功退出码为 0；每次执行重写 `check-report.json`。不依赖当前工作目录以外的文件。Python 3.8+ 即可。固定种子是 0x4E564E、0x4E564F 和 0x4E5650。此报告必须单独列作“参考契约测试通过”，不能写成“后端测试通过”。

## 覆盖与具体失败模式

| 测试 | 正确契约 | 被主动击穿的简化 |
|---|---|---|
| cross_pool_alias_and_overlapping_ssbo | pool window 解析为 backing identity + generation + offset；重叠视图真正共享字节 | 用 PoolId 判别 alias；每个 writable SSBO 做独立副本，导致同一 draw 内写 A 后读 B 看不到更新 |
| range_publication_and_war | RAW/WAW 跟踪写者；WAR 跟踪全部未完成读者；不相交范围独立 | ensure_readable 只看 producer，遗漏 Q0 read → Q1 write |
| multiqueue_retirement_epoch | 所有使用者完成，且 queue incarnation 一致 | 全局最大 timestamp；queue 重建后同数值误认旧任务完成 |
| mapping_and_descriptor_versions | MapEpoch / descriptor encoding 变更使 root 失效；内容写不改变编码；A/B/A root 不可变复用；record 保存 slot selector，在已定义消费边界解析版本 | 只按逻辑地址缓存；原地重写 root；record 时冻结 descriptor pool 内容 |
| callback_patch_copy_and_handle_ABA | 容量 callback 同步完成；无进展确定失败且不留半条命令；提交快照独立；patch 使后续 decode 失效 | 按不变外部 handle 地址复用 decode；把 Copy 当动态 Call |
| render_split_once_only | 邻域读取、sample/attachment 不兼容、容量不足、shader 副作用不进入该 tile-local 合并模型；query/event 一次执行 | tile replay 重复 query/event；邻域读取假作 framebuffer-local |
| signal_wait_order_and_availability | 保持 Signal → Wait 时序；query result 可见后 availability 才可见 | 把 Signal 统一移到 render epilogue 造成同队列等待自锁；availability 提前 |
| submit_partial_failure | accepted prefix 保留生命周期；明确 rejected 可回滚但不发布；unknown 隔离；仅 accepted 可 complete | 一个 ioctl 失败就整批回滚；unknown 当作未提交；失败伪造 publication |
| raw_partial_store_sequential_oracle | CPU store 前 materialize，保留所有未写 GPU 字节 | snapshot diff 丢失写回旧值；整页上传旧 raw 污染 GPU 新值 |

另有 200 组生成反例同时喂给正确模型与 broken_models.py 的错误算法，每组分别击穿 pool-key alias、producer-only WAR、全局 scalar retirement、old-value snapshot diff，共 800 次确定性差异。

随机测试不是仅调用两次同一实现：range 测试用独立区间交集公式作为 oracle；500 组、每组 20 次 CPU/GPU 字节写使用一个顺序 bytearray 作为 oracle，参考模型则使用独立 CPU/GPU 副本及 authority 状态。负向控制会实际计算错误缓存/副本/diff/reorder 的结果，再验证与契约要求不一致。其余状态测试为最小确定性反例，不宣称随机穷举。

## 同步字不是普通数据访问

新增 test_event_poll_is_not_data_reader：Q0 正在 GPUWaitEvent 轮询、Q1 或 CPU 执行 signal，token 轮询不进入普通 DATA_READ 的 WAR frontier。实际错误模型 PollAsDataReader 会给 signaler 加上等待 waiter 完成的反向依赖，和 waiter 等 signal 构成二节点死锁。豁免只针对明确识别的同步 token 字节范围，不能豁免整页；同页相邻数据的 WAR 仍被测试保留。SyncToken 携带 release/acquire resource scope 元数据，scope 缺失不能声称 payload 已经可见。测试不模拟 polling 指令、cache flush 或完整调度器。

## 明确建模假设

1. 所有输入 byte range 已经过上层 64 位溢出、跨映射分段、权限、资源生存期校验。Python 整数不会自然复现 C 溢出；本模型不是边界校验替代品。
2. 生成的跨队列 DATA hazard trace 假设 guest 已有合法 happens-before；测试验证的是实现对该顺序所需依赖的保留，不定义未同步的 NVN queue race 应有何结果。并发 atomic RMW 是另一种 access class，需要原子范围、作用域和硬件支持契约，本模型未覆盖，不能当作普通 READ/WRITE 自动串行化的证明。

   Hazards 使用逐字节键，作为 interval 实现的慢速 oracle。access 返回的依赖必须被真实调度器执行，模型不会自动发出 GPU wait。假设调用方按计划顺序登记访问；没有把跨队列整数值作全局顺序。
3. Ledger 使用保守 retirement：device epoch 变化后旧点不会因新 timestamp 而自动退休。设备丢失后的资源回收须另有已确认设备停止访问的证明，不能永久保留所有 BO，也不能拿新 epoch 完成代替该证明。
4. mapping 测试输入 canonical backing identity 已正确建立。导入同一 dma-buf 的多个 handle 如何归一、别名检测和 import 生存期依然要平台实现验证。
5. Roots key 是已解析的硬件编码身份，不包含内容版本。读取内容仍然需要 hazard dependency，descriptor 命中不免除同步。本目录 descriptor consumption 与扁平 Copy 都是拟议 private compatibility profile 的模型，不是 NVN normative oracle。selector 的确切 NVN 消费时间须由 API 证据/trace 定义；测试只证明“不应无条件 record-time 冻结”。
6. Recorder 的每命令单位预算只模拟容量契约，不声称是 NVN 二进制字节长度。callback 可 AddCommandMemory，但拒绝额外 record 重入是本模型的策略，不是已验证 NVN 全部重入语义。没有模拟 distinct control arena，也没有模拟真实锁；同步 callback 测试不能证明真实实现无死锁。
7. Copy 测试只针对扁平、无 nested Call 的命令序列。真实 CopyCommands 包含嵌套 Call 的快照/引用边界仍是 gate，不能据此递归展开全部调用。
8. render 模型保守地将 shader side effect 排除 GMEM，覆盖 VS SSBO atomic，而非只看 fragment。相同 attachment id 不是完整兼容性证明，真实模型还须含 format/view/subresource/layout/resolve/feedback/cache 域。bytes_required 由完整 tile working-set 估算得出，不能只算单张 color attachment。
9. replay_schedule 只说明 query/event 的结构性 once-only；不模拟真实 NVN 时间点、计数器累积、binning pass、cache flush 或内存可见性。Signal/Wait 测试仅模拟同队列顺序约束。
10. Submission 将每一提交 chunk 的 outcome 分别记录。GPU write 发布不仅需要执行完成，还需要相应 cache 可见性及 layout materialization；模型的 complete 是已满足这些前置条件的抽象事件，不能直接等同 ioctl success。
11. RawMirror 是串行、精确 byte store、无压缩、无 GOB 的内容保真 oracle。真实 mprotect handler 要解决故障指令重试、并发 CPU writers、read fault、信号安全、页/缓存线粒度、protect→seal 竞态。Python 参考模型没有证明这些。

## 开工时接入真实实现

- 保留这些反例，在 C/C++ interval tracker、root cache、submit ledger、command decoder 单测中复刻；对同一随机 trace 比较参考模型和实际结果。
- 将输入操作/依赖集合做 trace 序列化，检查真实 tracker 没遗漏依赖。多余依赖是性能回归，遗漏依赖是正确性失败，应分开计数。
- 在实际 KGSL probe 上单独验证 accepted/unknown/partial outcome、queue reset、cache clean/invalidate、GPUVA 生命周期，不以本模型通过解锁。
- RenderEpoch、query timestamp、descriptor consumption、nested Call/Copy 等语义未获证据前采用保守路径或明确 gate；不得为让测试通过修改 oracle 来追随现有 hack。
- 添加真正并发 writer stress、真实 CPU fault 注入、OOM callback、command/control arena 双预算、handle 地址复用、fragment/VS atomic、跨 tile query 读写用例。这些属于后续实现验收，当前报告未执行。

## 证据来源

测试反例对应 revision3 的 memory、recording ABI、GPU execution 审查契约。源码锚点和证据等级由这些独立审查文档提供，本目录不复制不稳定行号。特别地，Turnip 的 primitive counter tile replay 条件执行与 timestamp 取最后 tile 的 Vulkan 实现不等于 NVN 时间点语义；本模型按保守 NVN 时序要求切分。
