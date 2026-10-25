# v3 10-api-05-command-draw：Turnip调用合同
这是设计覆盖，不是已实现API。v3新增门禁和私有profile以本包normative contracts为准。

## nvnCommandBufferInitialize

- 阶段/证据：P0-D2R静态调用集合；static=4；source-defined
- 候选ABI：`NVNboolean (NVNcommandBuffer *c, NVNdevice *d)`；candidate-definition
- 使用：构造CommandBuffer对象
- 消费：host执行；保存传入device身份；初始化空对象和generation，不存在builder参数
- 实现：nvn_core + nvn_ir ordered stream；保存传入device身份；初始化空对象和generation，不存在builder参数
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L353)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L687)

## nvnCommandBufferFinalize

- 阶段/证据：P0-D2R静态调用集合；static=7；source-defined
- 候选ABI：`void (NVNcommandBuffer *c)`；candidate-definition
- 使用：推断：录制渲染状态/资源绑定；CommandBufferFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：nvn_core + nvn_ir ordered stream；CommandBufferFinalize对象操作
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：CommandHandle代数+显式引用/完成ticket退休；禁止固定8帧猜寿命
- 验收：T-CMD: 重录/复用/嵌套Call/Copy、回调扩容、并行录制、在途Finalize；CommandBufferFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L354)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L690)

## nvnCommandBufferSetMemoryCallback

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC cb)`；candidate-definition
- 使用：配置录制存储不足时guest扩容回调
- 消费：host保存函数/用户data/enable；耗尽时按ABI调用
- 实现：core callback bridge；不发GPU命令
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：仅耗尽触发，预留预算避免热路径频繁回调
- 寿命/同步：回调重入和thread/调用约定遵循guest ABI；不可在持有不可重入锁时调用
- 验收：递归扩容、disabled不足、回调失败、更新data
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L356)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L691)

## nvnCommandBufferSetMemoryCallbackData

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, void *d)`；candidate-definition
- 使用：配置录制存储不足时guest扩容回调
- 消费：host保存函数/用户data/enable；耗尽时按ABI调用
- 实现：core callback bridge；不发GPU命令
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：仅耗尽触发，预留预算避免热路径频繁回调
- 寿命/同步：回调重入和thread/调用约定遵循guest ABI；不可在持有不可重入锁时调用
- 验收：递归扩容、disabled不足、回调失败、更新data
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L357)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L692)

## nvnCommandBufferSetCommandMemoryCallbackEnabled

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：配置录制存储不足时guest扩容回调
- 消费：host保存函数/用户data/enable；耗尽时按ABI调用
- 实现：core callback bridge；不发GPU命令
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：仅耗尽触发，预留预算避免热路径频繁回调
- 寿命/同步：回调重入和thread/调用约定遵循guest ABI；不可在持有不可重入锁时调用
- 验收：递归扩容、disabled不足、回调失败、更新data
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L358)

## nvnCommandBufferAddCommandMemory

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNmemoryPool *p, ptrdiff_t off, size_t n)`；candidate-definition
- 使用：调用方提供可查询/可能被patch的录制存储段
- 消费：host保存pool+offset+size或host span及generation
- 实现：core guest storage arena；native command allocator另分离
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：顺序分配与段索引O(1)；不是把所有guest内存复制一份
- 寿命/同步：实际字节编码/回调需求保留；跨段patch检测；range在使用期间有效
- 验收：段不足回调、边界跨段、GetMemoryUsed定位patch、inflight reuse
- 门禁：完整guest命令编码是扩展资格门禁，不假定NVN API禁止外部修改
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L359)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L744)

## nvnCommandBufferAddControlMemory

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, void *m, size_t n)`；candidate-definition
- 使用：调用方提供可查询/可能被patch的录制存储段
- 消费：host保存pool+offset+size或host span及generation
- 实现：core guest storage arena；native command allocator另分离
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：顺序分配与段索引O(1)；不是把所有guest内存复制一份
- 寿命/同步：实际字节编码/回调需求保留；跨段patch检测；range在使用期间有效
- 验收：段不足回调、边界跨段、GetMemoryUsed定位patch、inflight reuse
- 门禁：完整guest命令编码是扩展资格门禁，不假定NVN API禁止外部修改
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L360)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L751)

## nvnCommandBufferGetCommandMemorySize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNcommandBuffer*)`；candidate-generated-header
- 使用：查询调用方命令/控制存储占用；非普通统计
- 消费：host立即查询Command arena的Size；typed-v1 used按Add起算，不按Begin起算
- 实现：core arena accounting；公开Size/Used/Free由目标ABI profile定义，不返回native IB/descriptor缓存用量
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：查询O(1)；native包大小另行统计不可代替guest占用
- 寿命/同步：nxnative LM3会以base+GetCommandMemoryUsed定位BindTexture+0x10并每帧patch：需受支持guest编码影子+写集检测+解码失效；纯immutable IR不兼容
- 验收：录制后/提交前patch绑定，确认只重编对应块，旧在途版本不变
- 门禁：v3 A3/A4：保留guest可观察command/control预算；私有typed-v1的32Bcontrol记录不代表正式NVN计费
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L361)

## nvnCommandBufferGetCommandMemoryUsed

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNcommandBuffer*)`；candidate-generated-header
- 使用：查询调用方命令/控制存储占用；非普通统计
- 消费：host立即查询Command arena的Used；typed-v1 used按Add起算，不按Begin起算
- 实现：core arena accounting；公开Size/Used/Free由目标ABI profile定义，不返回native IB/descriptor缓存用量
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：查询O(1)；native包大小另行统计不可代替guest占用
- 寿命/同步：nxnative LM3会以base+GetCommandMemoryUsed定位BindTexture+0x10并每帧patch：需受支持guest编码影子+写集检测+解码失效；纯immutable IR不兼容
- 验收：录制后/提交前patch绑定，确认只重编对应块，旧在途版本不变
- 门禁：v3 A3/A4：保留guest可观察command/control预算；私有typed-v1的32Bcontrol记录不代表正式NVN计费
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L362)

## nvnCommandBufferGetCommandMemoryFree

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNcommandBuffer*)`；candidate-generated-header
- 使用：查询调用方命令/控制存储占用；非普通统计
- 消费：host立即查询Command arena的Free；typed-v1 used按Add起算，不按Begin起算
- 实现：core arena accounting；公开Size/Used/Free由目标ABI profile定义，不返回native IB/descriptor缓存用量
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：查询O(1)；native包大小另行统计不可代替guest占用
- 寿命/同步：nxnative LM3会以base+GetCommandMemoryUsed定位BindTexture+0x10并每帧patch：需受支持guest编码影子+写集检测+解码失效；纯immutable IR不兼容
- 验收：录制后/提交前patch绑定，确认只重编对应块，旧在途版本不变
- 门禁：v3 A3/A4：保留guest可观察command/control预算；私有typed-v1的32Bcontrol记录不代表正式NVN计费
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L363)

## nvnCommandBufferGetControlMemorySize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNcommandBuffer*)`；candidate-generated-header
- 使用：查询调用方命令/控制存储占用；非普通统计
- 消费：host立即查询Control arena的Size；typed-v1 used按Add起算，不按Begin起算
- 实现：core arena accounting；公开Size/Used/Free由目标ABI profile定义，不返回native IB/descriptor缓存用量
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：查询O(1)；native包大小另行统计不可代替guest占用
- 寿命/同步：nxnative LM3会以base+GetCommandMemoryUsed定位BindTexture+0x10并每帧patch：需受支持guest编码影子+写集检测+解码失效；纯immutable IR不兼容
- 验收：录制后/提交前patch绑定，确认只重编对应块，旧在途版本不变
- 门禁：v3 A3/A4：保留guest可观察command/control预算；私有typed-v1的32Bcontrol记录不代表正式NVN计费
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L364)

## nvnCommandBufferGetControlMemoryUsed

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNcommandBuffer*)`；candidate-generated-header
- 使用：查询调用方命令/控制存储占用；非普通统计
- 消费：host立即查询Control arena的Used；typed-v1 used按Add起算，不按Begin起算
- 实现：core arena accounting；公开Size/Used/Free由目标ABI profile定义，不返回native IB/descriptor缓存用量
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：查询O(1)；native包大小另行统计不可代替guest占用
- 寿命/同步：nxnative LM3会以base+GetCommandMemoryUsed定位BindTexture+0x10并每帧patch：需受支持guest编码影子+写集检测+解码失效；纯immutable IR不兼容
- 验收：录制后/提交前patch绑定，确认只重编对应块，旧在途版本不变
- 门禁：v3 A3/A4：保留guest可观察command/control预算；私有typed-v1的32Bcontrol记录不代表正式NVN计费
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L365)

## nvnCommandBufferGetControlMemoryFree

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNcommandBuffer*)`；candidate-generated-header
- 使用：查询调用方命令/控制存储占用；非普通统计
- 消费：host立即查询Control arena的Free；typed-v1 used按Add起算，不按Begin起算
- 实现：core arena accounting；公开Size/Used/Free由目标ABI profile定义，不返回native IB/descriptor缓存用量
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：查询O(1)；native包大小另行统计不可代替guest占用
- 寿命/同步：nxnative LM3会以base+GetCommandMemoryUsed定位BindTexture+0x10并每帧patch：需受支持guest编码影子+写集检测+解码失效；纯immutable IR不兼容
- 验收：录制后/提交前patch绑定，确认只重编对应块，旧在途版本不变
- 门禁：v3 A3/A4：保留guest可观察command/control预算；私有typed-v1的32Bcontrol记录不代表正式NVN计费
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L366)

## nvnCommandBufferBeginRecording

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNcommandBuffer *c)`；candidate-definition
- 使用：开始新的命令录制实例
- 消费：host recorder进入RECORDING；typed-v1不额外写synthetic BEGIN，不以Begin重置Add起算的arena used计数
- 实现：Recorder状态机和事务存储；guest可见预算由ABI profile定义，与host IR/native包空间独立
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：沿用arena但不能覆盖仍在途guest-visible段
- 寿命/同步：重复Begin按错误契约处理；状态继承/默认规则按NVNoracle
- 验收：重录、空流、缺存储触发callback、并行不同recorder
- 门禁：v3 A2/A3：真实SDK对象inline字段、空记录和计费规则需profile；不能照候选sizeof或内部字节码预算写guest
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L367)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L764)

## nvnCommandBufferEndRecording

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`NVNcommandHandle (NVNcommandBuffer *c)`；candidate-definition
- 使用：结束录制返回可复用handle
- 消费：host冻结当前逻辑命令版本，资源字节仍执行时读取
- 实现：core句柄表+lazy backend compilation
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：首次编译依赖摘要；重复提交仅校验generation/readset
- 寿命/同步：固定8帧退休改完成ticket；返回handle不可直接作为native包地址
- 验收：空流、重录、重复提交、pending Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L368)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L770)

## nvnCommandBufferCallCommands

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, const NVNcommandHandle*)`；candidate-generated-header
- 使用：复用已录制命令块；可能跨帧调用
- 消费：录制复制handle数组；按已验证命令内存可修改语义在提交读取版本
- 实现：IR Call(handle,generation)；原生可重用包需可重定位资源根
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：缓存每个command generation的依赖摘要及backend编译块
- 寿命/同步：保留被调用块引用；循环/深度/已失效handle拒绝；patch使版本失效
- 验收：嵌套Call、多次提交、host patch后重放、在途销毁
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L369)

## nvnCommandBufferCopyCommands

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, int n, const NVNcommandHandle *h)`；candidate-definition
- 使用：将源命令当前记录复制到目标；typed-v1定义浅复制记录并保留嵌套Call边，正式NVN行为须独立资格
- 消费：typed-v1在调用时复制源record值；嵌套Call仍引用其target版本；不递归flatten资源和执行期字节
- 实现：core.copy_record_snapshot_preserve_call_edges；D2R现OP_CALL实现不能证明NVN Copy合同
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：可共享不可变内部块但目标语义不能随源后续改写
- 寿命/同步：源record快照与嵌套target引用分别保活；预算/失败事务见recording-abi-contract；未知raw格式不得准入
- 验收：Copy后改源再执行目标，与Call行为对照原机
- 门禁：v3 A5：Copy/Call差异、嵌套复制和可变命令存储必须通过目标profile鉴别；私有typed-v1不冒充NVN规范
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L370)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L777)

## nvnCommandBufferDrawTransformFeedback

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNdrawPrimitive, NVNbufferAddress)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferDrawTransformFeedback对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：BackendOps.render_epoch_emit；CommandBufferDrawTransformFeedback对象操作；专用能力：Adreno对应streamout/tess/metadata路径；Mali无stock GS/Tess/XFB完整路径，需独立lowering/资格门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：记录标量；顶点/索引/indirect内容在执行时读取；顺序/sideeffect/feedback决定tile边界
- 验收：T-DRAW: zero count/basevertex/baseinstance/restart/indirect GPU生成、深度混合一致性；CommandBufferDrawTransformFeedback参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L400)

## nvnCommandBufferDrawArrays

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNdrawPrimitive pr, int first, int n)`；candidate-definition
- 使用：直接非索引绘制；标量参数录制固定
- 消费：record标量/地址；exec读vertex/资源字节
- 实现：Adreno CP_DRAW_INDX_OFFSET；Mali RUN_IDVS / JM tiler job
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：draw state dirty patch；稳定根；zero count可省几何但不可吞语义事件
- 寿命/同步：first/vertexCount/baseInstance/instanceCount按该签名保留；非索引调用没有index buffer/basevertex/restart读取；indirect参数在exec读取，不改变draw顺序
- 验收：first、0vertexCount、instanceCount/baseInstance（仅具该参数版本）、GPU生成indirect及count（仅间接版本）；非索引builtin一致性
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L401)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L901)

## nvnCommandBufferDrawElements

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNdrawPrimitive, NVNindexType, int, NVNbufferAddress)`；candidate-generated-header
- 使用：直接索引绘制；标量参数录制固定
- 消费：record标量/地址；exec读index与vertex/资源字节
- 实现：Adreno CP_DRAW_INDX_OFFSET；Mali RUN_IDVS / JM tiler job
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：draw state dirty patch；稳定根；zero count可省几何但不可吞语义事件
- 寿命/同步：basevertex有符号加法；baseinstance与divisor；索引格式/restart及bounds；不改变顺序
- 验收：index8/16/32、负basevertex、instance、GPU写参数→barrier→draw、0count
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L402)

## nvnCommandBufferDrawElementsBaseVertex

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNdrawPrimitive, NVNindexType, int, NVNbufferAddress, int)`；candidate-generated-header
- 使用：直接索引绘制；标量参数录制固定
- 消费：record标量/地址；exec读index与vertex/资源字节
- 实现：Adreno CP_DRAW_INDX_OFFSET；Mali RUN_IDVS / JM tiler job
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：draw state dirty patch；稳定根；zero count可省几何但不可吞语义事件
- 寿命/同步：basevertex有符号加法；baseinstance与divisor；索引格式/restart及bounds；不改变顺序
- 验收：index8/16/32、负basevertex、instance、GPU写参数→barrier→draw、0count
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L403)

## nvnCommandBufferDrawArraysInstanced

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNdrawPrimitive, int, int, int, int)`；candidate-generated-header
- 使用：直接非索引绘制；标量参数录制固定
- 消费：record标量/地址；exec读vertex/资源字节
- 实现：Adreno CP_DRAW_INDX_OFFSET；Mali RUN_IDVS / JM tiler job
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：draw state dirty patch；稳定根；zero count可省几何但不可吞语义事件
- 寿命/同步：first/vertexCount/baseInstance/instanceCount按该签名保留；非索引调用没有index buffer/basevertex/restart读取；indirect参数在exec读取，不改变draw顺序
- 验收：first、0vertexCount、instanceCount/baseInstance（仅具该参数版本）、GPU生成indirect及count（仅间接版本）；非索引builtin一致性
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L404)

## nvnCommandBufferDrawElementsInstanced

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNdrawPrimitive pr, NVNindexType it, int n, NVNbufferAddress ib, int bv, int bi, int inst)`；candidate-definition
- 使用：直接索引绘制；标量参数录制固定
- 消费：record标量/地址；exec读index与vertex/资源字节
- 实现：Adreno CP_DRAW_INDX_OFFSET；Mali RUN_IDVS / JM tiler job
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：draw state dirty patch；稳定根；zero count可省几何但不可吞语义事件
- 寿命/同步：basevertex有符号加法；baseinstance与divisor；索引格式/restart及bounds；不改变顺序
- 验收：index8/16/32、负basevertex、instance、GPU写参数→barrier→draw、0count
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L405)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L902)

## nvnCommandBufferDrawArraysIndirect

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNdrawPrimitive, NVNbufferAddress)`；candidate-generated-header
- 使用：间接非索引绘制；GPU生成参数，不得CPU预读
- 消费：record标量/地址；exec读indirect/count/stride及vertex/资源字节
- 实现：Adreno CP_DRAW_INDIRECT_MULTI；Mali CSF indirect control flow / JM helper
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：draw state dirty patch；稳定根；zero count可省几何但不可吞语义事件
- 寿命/同步：first/vertexCount/baseInstance/instanceCount按该签名保留；非索引调用没有index buffer/basevertex/restart读取；indirect参数在exec读取，不改变draw顺序
- 验收：first、0vertexCount、instanceCount/baseInstance（仅具该参数版本）、GPU生成indirect及count（仅间接版本）；非索引builtin一致性
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L406)

## nvnCommandBufferDrawElementsIndirect

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNdrawPrimitive, NVNindexType, NVNbufferAddress, NVNbufferAddress)`；candidate-generated-header
- 使用：间接索引绘制；GPU生成参数，不得CPU预读
- 消费：record标量/地址；exec读indirect/count/stride及index与vertex/资源字节
- 实现：Adreno CP_DRAW_INDIRECT_MULTI；Mali CSF indirect control flow / JM helper
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：draw state dirty patch；稳定根；zero count可省几何但不可吞语义事件
- 寿命/同步：basevertex有符号加法；baseinstance与divisor；索引格式/restart及bounds；不改变顺序
- 验收：index8/16/32、负basevertex、instance、GPU写参数→barrier→draw、0count
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L407)

## nvnCommandBufferMultiDrawArraysIndirectCount

- 阶段/证据：P1-静态可达/历史运行报零；仍需显式能力门控；static=2；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNdrawPrimitive, NVNbufferAddress, NVNbufferAddress, int, ptrdiff_t)`；candidate-generated-header
- 使用：间接非索引绘制；GPU生成参数，不得CPU预读
- 消费：record标量/地址；exec读indirect/count/stride及vertex/资源字节
- 实现：Adreno CP_DRAW_INDIRECT_MULTI；Mali CSF indirect control flow / JM helper
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：draw state dirty patch；稳定根；GPU count clamp到maxdrawcount
- 寿命/同步：first/vertexCount/baseInstance/instanceCount按该签名保留；非索引调用没有index buffer/basevertex/restart读取；indirect参数在exec读取，不改变draw顺序
- 验收：first、0vertexCount、instanceCount/baseInstance（仅具该参数版本）、GPU生成indirect及count（仅间接版本）；非索引builtin一致性
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L408)

## nvnCommandBufferMultiDrawElementsIndirectCount

- 阶段/证据：P1-静态可达/历史运行报零；仍需显式能力门控；static=2；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNdrawPrimitive, NVNindexType, NVNbufferAddress, NVNbufferAddress, NVNbufferAddress, int, ptrdiff_t)`；candidate-generated-header
- 使用：间接索引绘制；GPU生成参数，不得CPU预读
- 消费：record标量/地址；exec读indirect/count/stride及index与vertex/资源字节
- 实现：Adreno CP_DRAW_INDIRECT_MULTI；Mali CSF indirect control flow / JM helper
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：draw state dirty patch；稳定根；GPU count clamp到maxdrawcount
- 寿命/同步：basevertex有符号加法；baseinstance与divisor；索引格式/restart及bounds；不改变顺序
- 验收：index8/16/32、负basevertex、instance、GPU写参数→barrier→draw、0count
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L409)

## nvnCommandBufferClearColor

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, int index, const float *rgba, int mask)`；candidate-definition
- 使用：清当前附件的指定颜色/深度模板组件
- 消费：record值、index、mask；执行时写绑定附件
- 实现：render_epoch clear op；Adreno GMEM/meta / Mali tile clear/meta
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：首次全覆盖无先前读取可折叠load-op；中途clear保留位置
- 寿命/同步：i/ui类型与write mask不能当float；depth/stencil独立保留
- 验收：部分通道、midpass、scissor、MSAA及整数附件
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L410)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L830)

## nvnCommandBufferClearColori

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, const int*, int)`；candidate-generated-header
- 使用：清当前附件的指定颜色/深度模板组件
- 消费：record值、index、mask；执行时写绑定附件
- 实现：render_epoch clear op；Adreno GMEM/meta / Mali tile clear/meta
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：首次全覆盖无先前读取可折叠load-op；中途clear保留位置
- 寿命/同步：i/ui类型与write mask不能当float；depth/stencil独立保留
- 验收：部分通道、midpass、scissor、MSAA及整数附件
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L411)

## nvnCommandBufferClearColorui

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, const uint32_t*, int)`；candidate-generated-header
- 使用：清当前附件的指定颜色/深度模板组件
- 消费：record值、index、mask；执行时写绑定附件
- 实现：render_epoch clear op；Adreno GMEM/meta / Mali tile clear/meta
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：首次全覆盖无先前读取可折叠load-op；中途clear保留位置
- 寿命/同步：i/ui类型与write mask不能当float；depth/stencil独立保留
- 验收：部分通道、midpass、scissor、MSAA及整数附件
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L412)

## nvnCommandBufferClearDepthStencil

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, float depth, uint32_t dmask, int stencil, int smask)`；candidate-definition
- 使用：清当前附件的指定颜色/深度模板组件
- 消费：record值、index、mask；执行时写绑定附件
- 实现：render_epoch clear op；Adreno GMEM/meta / Mali tile clear/meta
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：首次全覆盖无先前读取可折叠load-op；中途clear保留位置
- 寿命/同步：i/ui类型与write mask不能当float；depth/stencil独立保留
- 验收：部分通道、midpass、scissor、MSAA及整数附件
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L413)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L833)

## nvnCommandBufferDispatchCompute

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, int x, int y, int z)`；candidate-definition
- 使用：计算dispatch；间接版本旧resolver为nop不代表可忽略
- 消费：record参数；XYZ录制复制
- 实现：Adreno CP_EXEC_CS；Mali RUN_COMPUTE/JM compute job；间接由硬件/helper读取
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_EXEC_CS/indirect及ir3 compute状态，按域flush
- 优化：根表与compute程序版本cache；meta用独立scratchstate
- 寿命/同步：render→compute有访问冲突先store；compute→render精确cache dependency
- 验收：GPU写XYZ、SSBO/image依赖、zero workgroup、workgroup限制
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L414)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L906)

## nvnCommandBufferDispatchComputeIndirect

- 阶段/证据：P0-D2R静态调用集合；static=2；resolver-nop
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNbufferAddress)`；candidate-generated-header
- 使用：计算dispatch；间接版本旧resolver为nop不代表可忽略
- 消费：record参数；exec读取XYZ地址
- 实现：Adreno CP_EXEC_CS；Mali RUN_COMPUTE/JM compute job；间接由硬件/helper读取
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_EXEC_CS/indirect及ir3 compute状态，按域flush
- 优化：根表与compute程序版本cache；meta用独立scratchstate
- 寿命/同步：render→compute有访问冲突先store；compute→render精确cache dependency
- 验收：GPU写XYZ、SSBO/image依赖、zero workgroup、workgroup限制
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L415)

## nvnCommandBufferSetTiledCacheAction

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNtiledCacheAction)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；保留action语义；只能作为tile调度hint，不能把未知枚举当pass结束
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(TiledCacheAction)；nvn_core + nvn_ir ordered stream；保留action语义；只能作为tile调度hint，不能把未知枚举当pass结束
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：规范化TiledCacheAction后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：CommandHandle代数+显式引用/完成ticket退休；禁止固定8帧猜寿命
- 验收：T-CMD: 重录/复用/嵌套Call/Copy、回调扩容、并行录制、在途Finalize；CommandBufferSetTiledCacheAction参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L424)

## nvnCommandBufferSetTiledCacheTileSize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, int)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；guest tilehint不强迫采用Maxwell tile尺寸；按GMEM/tilebuffer预算重新选择
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(TiledCacheTileSize)；nvn_core + nvn_ir ordered stream；guest tilehint不强迫采用Maxwell tile尺寸；按GMEM/tilebuffer预算重新选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：规范化TiledCacheTileSize后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：CommandHandle代数+显式引用/完成ticket退休；禁止固定8帧猜寿命
- 验收：T-CMD: 重录/复用/嵌套Call/Copy、回调扩容、并行录制、在途Finalize；CommandBufferSetTiledCacheTileSize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L425)

## nvnCommandBufferCopyBufferToTexture

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNbufferAddress src, const NVNtexture *t, const NVNtextureView *v, const NVNcopyRegion *r, int flags)`；candidate-definition
- 使用：传输/清零/resolve操作 CopyBufferToTexture
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(CopyBufferToTexture)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L444)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L915)

## nvnCommandBufferCopyTextureToBuffer

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNtexture *t, const NVNtextureView *v, const NVNcopyRegion *r, NVNbufferAddress dst, int flags)`；candidate-definition
- 使用：传输/清零/resolve操作 CopyTextureToBuffer
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(CopyTextureToBuffer)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L445)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L919)

## nvnCommandBufferCopyTextureToTexture

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNtexture *s, const NVNtextureView *sv, const NVNcopyRegion *sr, const NVNtexture *d, const NVNtextureView *dv, const NVNcopyRegion *dr, int flags)`；candidate-definition
- 使用：传输/清零/resolve操作 CopyTextureToTexture
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(CopyTextureToTexture)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L446)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L931)

## nvnCommandBufferCopyBufferToBuffer

- 阶段/证据：P0-D2R静态调用集合；static=4；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNbufferAddress src, NVNbufferAddress dst, size_t n, int flags)`；candidate-definition
- 使用：传输/清零/resolve操作 CopyBufferToBuffer
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(CopyBufferToBuffer)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L447)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L793)

## nvnCommandBufferClearBuffer

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNbufferAddress, size_t, uint32_t)`；candidate-generated-header
- 使用：传输/清零/resolve操作 ClearBuffer
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(ClearBuffer)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L448)

## nvnCommandBufferClearTexture

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNtexture *t, const NVNtextureView *v, const NVNcopyRegion *r, const float *rgba, int mask)`；candidate-definition
- 使用：传输/清零/resolve操作 ClearTexture
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(ClearTexture)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L449)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L925)

## nvnCommandBufferClearTexturei

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, const NVNtexture*, const NVNtextureView*, const NVNcopyRegion*, const int*, int)`；candidate-generated-header
- 使用：传输/清零/resolve操作 ClearTexturei
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(ClearTexturei)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L450)

## nvnCommandBufferClearTextureui

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNtexture *t, const NVNtextureView *v, const NVNcopyRegion *r, const uint32_t *rgba, int mask)`；candidate-definition
- 使用：传输/清零/resolve操作 ClearTextureui
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(ClearTextureui)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L451)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L928)

## nvnCommandBufferUpdateUniformBuffer

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNbufferAddress buf, size_t bufsz, ptrdiff_t off, size_t n, const void *data)`；candidate-definition
- 使用：推断：录制小块动态常量更新；已有实现复制payload
- 消费：录制复制data[0:size]；目标buffer只在GPU执行写入
- 实现：IR UploadImmediate(dst+offset,bytes)→CP内存写或meta copy；Mali staging+copy/compute
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：小payload内联IR；大payload一次arena复制；不整UBO快照
- 寿命/同步：验证offset+size<=bufferSize；前后读取顺序由barrier保留
- 验收：录制后改源CPU数据不影响命令；执行前改目标不能覆盖稍后更新
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L452)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L800)

## nvnCommandBufferSetRenderTargets

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, int n, const NVNtexture *const *ct, const NVNtextureView *const *cv, const NVNtexture *dt, const NVNtextureView *dv)`；candidate-definition
- 使用：每个pass绑定颜色/深度及视图；已有实现快照元数据
- 消费：录制复制attachment身份、mip/layer/view，不复制像素
- 实现：RenderEpoch attachment interface→Adreno GMEM/SYSMEM；Mali FBD/tilebuffer
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：附件key相同仅更新逻辑绑定；不可据此省略依赖
- 寿命/同步：首次load/末次store按覆盖与消费者决定；任意采样feedback先物化
- 验收：同pool重叠view；绑定后Finalize；pass中CPU改像素
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L458)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L823)

## nvnCommandBufferDiscardColor

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferDiscardColor对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：BackendOps.render_epoch_emit；CommandBufferDiscardColor对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：记录标量；顶点/索引/indirect内容在执行时读取；顺序/sideeffect/feedback决定tile边界
- 验收：T-DRAW: zero count/basevertex/baseinstance/restart/indirect GPU生成、深度混合一致性；CommandBufferDiscardColor参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L459)

## nvnCommandBufferDiscardDepthStencil

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferDiscardDepthStencil对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：BackendOps.render_epoch_emit；CommandBufferDiscardDepthStencil对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：记录标量；顶点/索引/indirect内容在执行时读取；顺序/sideeffect/feedback决定tile边界
- 验收：T-DRAW: zero count/basevertex/baseinstance/restart/indirect GPU生成、深度混合一致性；CommandBufferDiscardDepthStencil参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L460)

## nvnCommandBufferDownsample

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, const NVNtexture*, const NVNtexture*)`；candidate-generated-header
- 使用：传输/清零/resolve操作 Downsample
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(Downsample)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L461)

## nvnCommandBufferTiledDownsample

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, const NVNtexture*, const NVNtexture*)`；candidate-generated-header
- 使用：传输/清零/resolve操作 TiledDownsample
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(TiledDownsample)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L462)

## nvnCommandBufferDownsampleTextureView

- 阶段/证据：P1-静态可达/历史运行报零；仍需显式能力门控；static=2；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, const NVNtexture*, const NVNtextureView*, const NVNtexture*, const NVNtextureView*)`；candidate-generated-header
- 使用：传输/清零/resolve操作 DownsampleTextureView
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(DownsampleTextureView)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L463)

## nvnCommandBufferTiledDownsampleTextureView

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, const NVNtexture*, const NVNtextureView*, const NVNtexture*, const NVNtextureView*)`；candidate-generated-header
- 使用：传输/清零/resolve操作 TiledDownsampleTextureView
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(TiledDownsampleTextureView)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L464)

## nvnCommandBufferSetShaderScratchMemory

- 阶段/证据：P0-D2R静态调用集合；static=1；resolver-nop
- 候选ABI：`void (*)(NVNcommandBuffer*, const NVNmemoryPool*, ptrdiff_t, size_t)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；记录guest scratch范围/容量；native编译器spill另分配并校验可观测性
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(ShaderScratchMemory)；nvn_core + nvn_ir ordered stream；记录guest scratch范围/容量；native编译器spill另分配并校验可观测性
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：规范化ShaderScratchMemory后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：CommandHandle代数+显式引用/完成ticket退休；禁止固定8帧猜寿命
- 验收：T-CMD: 重录/复用/嵌套Call/Copy、回调扩容、并行录制、在途Finalize；CommandBufferSetShaderScratchMemory参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L470)

## nvnCommandBufferSaveZCullData

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNbufferAddress, size_t)`；candidate-generated-header
- 使用：保存/恢复深度剔除加速opaque状态
- 消费：record地址/size；执行访问指定ZCull存储范围
- 实现：NvnOp.SaveZCullData→版本化兼容metadata blob；Adreno LRZ或Mali层次深度重新生成/失效，不转译为GS/tess
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：已知深度内容版本一致可重用native加速metadata；否则重建
- 寿命/同步：加速数据必须不改变正确像素；guest读/复制opaque字节语义需兼容；不得把Tegra ZCull直接当LRZ
- 验收：保存→深度修改→恢复、拷贝blob、size边界；与禁用剔除对照
- 门禁：opaque格式/大小及是否guest可观察需oracle；未验证前显式门禁
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L471)

## nvnCommandBufferRestoreZCullData

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNbufferAddress, size_t)`；candidate-generated-header
- 使用：保存/恢复深度剔除加速opaque状态
- 消费：record地址/size；执行访问指定ZCull存储范围
- 实现：NvnOp.RestoreZCullData→版本化兼容metadata blob；Adreno LRZ或Mali层次深度重新生成/失效，不转译为GS/tess
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：已知深度内容版本一致可重用native加速metadata；否则重建
- 寿命/同步：加速数据必须不改变正确像素；guest读/复制opaque字节语义需兼容；不得把Tegra ZCull直接当LRZ
- 验收：保存→深度修改→恢复、拷贝blob、size边界；与禁用剔除对照
- 门禁：opaque格式/大小及是否guest可观察需oracle；未验证前显式门禁
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L472)

## nvnCommandBufferSetCopyRowStride

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, ptrdiff_t st)`；candidate-definition
- 使用：设置后续操作的CopyRowStride
- 消费：record复制参数；host记录stride供后续copy录制时快照；stride0为紧排，ptrdiff负值按ABI校验；本调用不产生GPU资源访问
- 实现：NvnOp.SetCopyRowStride(host记录stride供后续copy录制时快照；stride0为紧排，ptrdiff负值按ABI校验；本调用不产生GPU资源访问)→state_patch；copy stride仅影响TransferOp布局
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：仅相应字段dirty；不会因状态改变重做不相干descriptor解析
- 寿命/同步：后续setter不能追溯改变已录制命令；上下文独立；不支持能力明确拒绝
- 验收：默认/边界/重复值、录制后改源、状态组合像素oracle
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L473)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L936)

## nvnCommandBufferSetCopyImageStride

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, ptrdiff_t)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；后续copy的slice stride；3D和array语义分离
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(CopyImageStride)；nvn_core + nvn_ir ordered stream；后续copy的slice stride；3D和array语义分离
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：规范化CopyImageStride后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：CommandHandle代数+显式引用/完成ticket退休；禁止固定8帧猜寿命
- 验收：T-CMD: 重录/复用/嵌套Call/Copy、回调扩容、并行录制、在途Finalize；CommandBufferSetCopyImageStride参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L474)

## nvnCommandBufferGetCopyRowStride

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`ptrdiff_t (*)(const NVNcommandBuffer*)`；candidate-generated-header
- 使用：查询当前录制对象的CopyRowStride，不插入命令
- 消费：host立即读取command recorder字段并返回
- 实现：core Recorder.CopyRowStride；无GPU指令
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：O(1)字段读取；不触发提交、解析或flush
- 寿命/同步：遵循同一commandbuffer外部同步约定；不读取GPU资源内容
- 验收：Begin/End前后、setter后立即get、两个recorder相互独立
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L475)

## nvnCommandBufferGetCopyImageStride

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`ptrdiff_t (*)(const NVNcommandBuffer*)`；candidate-generated-header
- 使用：查询当前录制对象的CopyImageStride，不插入命令
- 消费：host立即读取command recorder字段并返回
- 实现：core Recorder.CopyImageStride；无GPU指令
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：O(1)字段读取；不触发提交、解析或flush
- 寿命/同步：遵循同一commandbuffer外部同步约定；不读取GPU资源内容
- 验收：Begin/End前后、setter后立即get、两个recorder相互独立
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L476)

## nvnCommandBufferDrawTexture

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNtextureHandle, const NVNdrawTextureRegion*, const NVNdrawTextureRegion*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferDrawTexture对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：BackendOps.render_epoch_emit；CommandBufferDrawTexture对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：CP_DRAW_INDX_OFFSET/CP_DRAW_INDIRECT_MULTI；GMEM或SYSMEM调度
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：记录标量；顶点/索引/indirect内容在执行时读取；顺序/sideeffect/feedback决定tile边界
- 验收：T-DRAW: zero count/basevertex/baseinstance/restart/indirect GPU生成、深度混合一致性；CommandBufferDrawTexture参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L477)

## nvnCommandBufferSetProgramSubroutines

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNprogram*, NVNshaderStage, const int, const int, const int*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；shader ABI动态子程序分派；不能无条件NOP
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(ProgramSubroutines)；nvn_core + nvn_ir ordered stream；shader ABI动态子程序分派；不能无条件NOP；专用能力：Adreno对应streamout/tess/metadata路径；Mali无stock GS/Tess/XFB完整路径，需独立lowering/资格门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：后端编译成tu_cs包/可重用状态块；不是直接重放Maxwell包
- 优化：规范化ProgramSubroutines后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：CommandHandle代数+显式引用/完成ticket退休；禁止固定8帧猜寿命
- 验收：T-CMD: 重录/复用/嵌套Call/Copy、回调扩容、并行录制、在途Finalize；CommandBufferSetProgramSubroutines参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L479)

## nvnCommandBufferResolveDepthBuffer

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*)`；candidate-generated-header
- 使用：传输/清零/resolve操作 ResolveDepthBuffer
- 消费：record区域/view/clear值和copy stride；exec读源写目的
- 实现：transfer_emit(ResolveDepthBuffer)；Adreno2D/3D/CP/meta；Mali copy/meta/tile resolve；按格式选择
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_clear_blit映射CP/2D/3D或compute；源目的布局决定
- 优化：同布局直copy；load-op clear或tile store resolve仅在位置/覆盖等价时融合
- 寿命/同步：源目的alias、部分覆盖与MSAA规则保留；不可跨依赖前移
- 验收：格式/整数符号/压缩块/overlap；分层多sample；与独立执行像素对照
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L481)

## nvnCommandBufferGetMemoryCallback

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC (*)(const NVNcommandBuffer*)`；candidate-generated-header
- 使用：查询当前录制对象的MemoryCallback，不插入命令
- 消费：host立即读取command recorder字段并返回
- 实现：core Recorder.MemoryCallback；无GPU指令
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：O(1)字段读取；不触发提交、解析或flush
- 寿命/同步：遵循同一commandbuffer外部同步约定；不读取GPU资源内容
- 验收：Begin/End前后、setter后立即get、两个recorder相互独立
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L492)

## nvnCommandBufferGetMemoryCallbackData

- 阶段/证据：P2-扩展；ABI冲突门禁；static=0；resolver-redstub
- 候选ABI：`ABI冲突：重建头为void返回，但getter合同要求userdata指针；未核实前不得生成可调用shim`；ABI-GATE
- 使用：查询当前录制对象的MemoryCallbackData，不插入命令
- 消费：host立即读取command recorder字段并返回
- 实现：host recorder userdata查询；正式wrapper被ABI门禁阻断；typed-v1内部函数可明确返回void*
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：O(1)字段读取；不触发提交、解析或flush
- 寿命/同步：遵循同一commandbuffer外部同步约定；不读取GPU资源内容
- 验收：Begin/End前后、setter后立即get、两个recorder相互独立
- 门禁：v3 A1：nvn_protos.h:467的void返回与用途冲突；先恢复真实返回ABI，不凭名称推定void*即已验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L493)

## nvnCommandBufferIsRecording

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`NVNboolean (const NVNcommandBuffer *c)`；candidate-definition
- 使用：查询当前录制对象的ecording，不插入命令
- 消费：host立即读取command recorder字段并返回
- 实现：core Recorder.ecording；无GPU指令
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：O(1)字段读取；不触发提交、解析或flush
- 寿命/同步：遵循同一commandbuffer外部同步约定；不读取GPU资源内容
- 验收：Begin/End前后、setter后立即get、两个recorder相互独立
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L494)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L776)
