# 第三轮修正台账与开工门

下面“设计已修订”表示新合同明确处理方式，不表示对应C++/驱动代码已完成。四份专项审计保留反例和原文位置；与二版冲突时以本版合同为准。

| 缺口 | 本版决定 | 可执行检查/首个实施位置 | 状态 |
|---|---|---|---|
| WAR未记录、prepare提前发布 | canonical interval的writer/reader frontier；tentative reservation独立，接受后commit | host M03；V04/07 | 设计已修订，host模型通过 |
| WaitEvent poll被普通WAR阻塞 | 精确token word的SYNC_POLL/SIGNAL；普通邻接字节不豁免 | poll-as-reader mutant；V04/同步后续 | 设计已修订，反例检出 |
| 多pool同物理页身份未知 | canonical backing来源、generation和provenance；未知alias不准入快路 | M01；V04/06 | 设计已修订 |
| 每view shadow破坏同draw alias | 同一可写alias分量必须共同表示，不能成立则拒绝 | overlapping SSBO模型；V04/资源后续 | 设计已修订 |
| 一个map_epoch漏局部remap | 不可变MappingSnapshot/extents vector，resolve+pin+reserve原子事务 | M04；V04/06 | 设计已修订 |
| CPU读/部分写及fault路径自锁 | 独立observer/lease协议、写前物化、禁止signal路径中阻塞自等；初期显式lease | M05/M06及字节模型；后续资格 | 初始范围明确；通用observer待实现 |
| 部分submit成功后整体回滚 | 每chunk receipt；MVP每transaction至多一个GPU_COMMAND；unknown quarantine | 两组提交模型；V07 | 设计已修订 |
| 外部fence导出失败被当未提交 | 接受性与export status独立；预分配journal，先commit再export | M07；V07/WSI | 设计已修订 |
| timestamp/context复用提前回收 | device/queue generation、host serial与native stamp分开、全部引用+消费者集合退休 | M08；V04/07 | 设计已修订 |
| SDK对象sizeof和inline写混用 | ABI profile允许写范围，sidecar保存host状态，未知inline字段gate | A2 canary；V02 | 可实现private profile；正式SDK资格未完成 |
| command预算误按Begin/内部IR计费 | 双arena，Add起算，私有计费明确；真实profile另核 | A3/A4；V03 | 设计已修订 |
| Copy误当Call/递归flatten | typed-v1浅复制records并保留嵌套Call边；正式行为另做鉴别 | A5；V03 | 私有合同可开工，真实profile门禁 |
| Sync只有对象generation | 引入FenceEpoch和wait锁存点 | A7；同步后续 | 设计已修订；NVN时点须证据 |
| GetMemoryCallbackData错误候选void | 改为ABI-GATE，不生成生产shim | API overlay；V02 | 门禁已显式，不能说已恢复ABI |
| KGSL几个文件被当独立driver | 同Mesa DSO bridge，完整冷初始化/析构、独立export map | B0、V01/05 | 开工路径已固定 |
| import补丁被误认为缺失 | 找到两条完整公开分支；combined patch不叠加 | 固定source pins、patch hash | 错误阻塞已撤销；f830整产物重建仍门控 |
| BO/VMA/fd清理与IB保活缺失 | ownership矩阵、dup失败unwind、borrowed VMA不unmap、IB/root/shader/data refs保持 | bootstrap失败矩阵；V06/07 | 设计已修订 |
| WSI零wait release误作完成 | 同device/queue实际acquire和release signal桥接，offscreen先行 | WSI生命周期fixture | 初期不启用；后续门明确 |
| descriptor bank与大小未定义 | 4-bank ABI、256B root、SSBO128B保留槽、索引/UBO字段边界 | E01/E02/E04；V09 | 精确内部ABI已定，待GPU检查 |
| shader binary被当完整管线 | NIR顺序、descriptor half修正、varying/VFD/SP/const配置 | E03；V10/11 | 接线已明确，未编译驱动 |
| sysmem仍可能并发binning | MVP显式关闭该模式及复杂阶段 | E05；V11 | 能力边界明确 |
| Query/Event tile重复与位置漂移 | 执行域枚举；初期epoch边界一次性执行；不统一移epilogue | once-only/Signal→Wait反例 | 设计已修订 |
| meta写状态破坏缓存 | emitter返回WriteSet；初期meta后全失效再重发 | STATE夹meta fixture；V11 | 设计已修订 |
| “页COW”没有连续GPUVA实现 | 普通BO新连续版本；紧凑表/rebanking新ABI；真正页COW单独gate | 条件字节预算+dirty统计 | 设计已修订 |

## 不再混淆四种“完成”

- 文档/host模型完成：本次交付。
- 可加载native前端/bridge：V01/02/05的代码与链接验收，尚未执行。
- 离屏native像素/资源闭环：V06–11真GPU验证，尚未执行。
- D2R实际小核心/场景闭环：真实输入、资源、shader、同步、WSI和语义资格，不能由静态216名称或triangle替代。

## 将来实验线的建议起点

如果后续明确授权实施，起点是V01的独立Mesa实验构建和V02–V04的host合同验证。当前仅交付设计。不要直接把现有D2R切换到一个尚无完整初始化、提交和ABI资格的半成品provider。普通BO路线可前进，不需要等待PanVK、VBO或f830完整二进制来源。
