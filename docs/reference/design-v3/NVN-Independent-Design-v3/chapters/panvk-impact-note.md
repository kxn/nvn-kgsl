# PanVK 对第三轮修订的继承范围

本次“开工”收敛到已选的Adreno/KGSL。PanVK独立二版仍是候选设计，没有执行新Mali硬件资格或承诺同时实现。

未来PanVK必须继承的语义修正：
- canonical backing与alias provenance，而非PoolId/VA/BO编号即物理身份。
- reader/writer frontier、tentative reservation及内容版本，普通WAR不套在event polling word上。
- 同一shader中的可写别名不能被独立shadow拆散。
- submission接受性、完成性、导出失败和device lost隔离分开；CSF多queue/group应按自己的实际接受原子性记录receipt图，不机械复制KGSL前缀模型。
- SDK对象profile、可观察command预算、Copy/Call与FenceEpoch消费边界独立核实。
- native窗口buffer/acquire/release与descriptor/IB寿命分别管理。

不能继承的Adreno细节：4-bank ABI、64B索引单位及SSBO双槽规则、PM4/CP_SET_DRAW_STATE、KGSL uint32 context timestamp、tu_device冷启动和CCU/UCHE协议。Mali必须保留Panthor CSF、kbase CSF和JM各自的实现路径。

因此本版并未把PanVK文档宣称为“也已可直接开工”。启动Mali时，要在已可用的实际kernel/WSI平台上重新完成对应构建闭包、native资源ABI和提交失败语义审查。
