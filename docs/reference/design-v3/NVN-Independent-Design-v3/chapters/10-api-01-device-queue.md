# v3 10-api-01-device-queue：Turnip调用合同
这是设计覆盖，不是已实现API。v3新增门禁和私有profile以本包normative contracts为准。

## nvnBootstrapLoader

- 阶段/证据：P0-bootstrap；static=0；bootstrap
- 候选ABI：`候选：void * (const char *name)；按实际hook ABI校验`；bootstrap-candidate
- 使用：装载阶段按名称取得入口
- 消费：host立即读取名称并返回稳定函数指针
- 实现：core resolver；无GPU包
- Turnip：仅host resolver/callback桥；无GPU操作
- 优化：静态完备表+名称hash，未知名返回规定失败
- 寿命/同步：返回值寿命为设备库会话；不能落入崩溃redstub
- 验收：逐个查询523名称与未知名称；ABI调用烟测
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L183)

## nvnDeviceBuilderSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdeviceBuilder *b)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：memset(b, 0, sizeof(dev_builder_t));
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：初始化/销毁边界；能力不可在同会话偷偷改变
- 验收：T-DEV: 属性/能力与实际执行一致，未知枚举明确失败；DeviceBuilderSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L1)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L194)

## nvnDeviceBuilderSetFlags

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdeviceBuilder *b, int f)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.capabilities + core设备对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Flags后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：初始化/销毁边界；能力不可在同会话偷偷改变
- 验收：T-DEV: 属性/能力与实际执行一致，未知枚举明确失败；DeviceBuilderSetFlags参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L2)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L195)

## nvnDeviceBuilderGetFlags

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.capabilities + core设备对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：规范化Flags后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：初始化/销毁边界；能力不可在同会话偷偷改变
- 验收：T-DEV: 属性/能力与实际执行一致，未知枚举明确失败；DeviceBuilderGetFlags参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L3)

## nvnDeviceInitialize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`NVNboolean (NVNdevice *d, const NVNdeviceBuilder *b)`；candidate-definition
- 使用：构造Device对象
- 消费：host执行；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- 实现：BackendOps.capabilities + core设备对象；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L4)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L196)

## nvnDeviceFinalize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdevice *d)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；DeviceFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.capabilities + core设备对象；DeviceFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：初始化/销毁边界；能力不可在同会话偷偷改变
- 验收：T-DEV: 属性/能力与实际执行一致，未知枚举明确失败；DeviceFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L5)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L202)

## nvnDeviceGetProcAddress

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`PFNNVNGENERICFUNCPTRPROC (const NVNdevice *d, const char *name)`；candidate-definition
- 使用：按名称获取入口，游戏通过返回函数指针调用
- 消费：host读NUL终止name并查会话resolver
- 实现：core resolver(name,deviceCaps)；无GPU包
- Turnip：仅host resolver/callback桥；无GPU操作
- 优化：同Bootstrap共用524条表；不每调用动态symbollookup
- 寿命/同步：返回稳定ABI thunk；不支持项按约定返回而非redstub崩溃
- 验收：已知/未知/NULL名、大小写、设备销毁前后指针
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L7)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L203)

## nvnDeviceGetInteger

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (const NVNdevice *d, NVNdeviceInfo what, int *out)`；candidate-definition
- 使用：游戏能力查询及资源对齐/大小规划
- 消费：host立即写out
- 实现：实际后端capability→NVN枚举转换表；无GPU包
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：设备会话内缓存；每个enum有实现证据
- 寿命/同步：不能复制Tegra上限而后端不支持；描述符尺寸/对齐与guest ABI一致
- 验收：逐enum边界、宣称feature实际执行；未知enum拒绝
- 门禁：完整NVN枚举表及数值需重建校验
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L8)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L224)

## nvnDeviceGetCurrentTimestampInNanoseconds

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`uint64_t (*)(const NVNdevice*)`；candidate-generated-header
- 使用：CPU可见当前GPU/设备时间基准查询
- 消费：host读取校准clock而非伪用CPU时间替代
- 实现：Adreno timestamp counter / Mali timestamp source + period校准
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：缓存转换系数；仅必要时采样校准
- 寿命/同步：与ReportCounter时间基准/溢出约定一致
- 验收：时间单调性、频率变化、跨queue timestamp差值
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L9)

## nvnDeviceSetIntermediateShaderCache

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNdevice*, int)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；IntermediateShaderCache字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.capabilities + core设备对象；IntermediateShaderCache字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：规范化IntermediateShaderCache后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：初始化/销毁边界；能力不可在同会话偷偷改变
- 验收：T-DEV: 属性/能力与实际执行一致，未知枚举明确失败；DeviceSetIntermediateShaderCache参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L10)

## nvnDeviceSetWindowOriginMode

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdevice *d, NVNwindowOriginMode m)`；candidate-definition
- 使用：设置全设备坐标约定WindowOriginMode
- 消费：host改变device convention version；后续命令消费时带入相应版本
- 实现：core DeviceConvention(WindowOriginMode)→viewport/frontface/clip/depth transform及shader builtin lowering，非单纯capability
- Turnip：host convention version；后续Adreno viewport/frontface/depth及ir3 builtin变体消费
- 优化：相同约定不失效；改变仅使依赖该约定的ProgramVariant/state块失效
- 寿命/同步：录制/提交跨约定改变的消费边界需oracle；不能悄悄改变已冻结状态
- 验收：上下原点、0..1/-1..1depth、cullface、shadow compare
- 门禁：坐标约定动态变更的时点需资格测试
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L16)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L241)

## nvnDeviceSetDepthMode

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdevice *d, NVNdepthMode m)`；candidate-definition
- 使用：设置全设备坐标约定DepthMode
- 消费：host改变device convention version；后续命令消费时带入相应版本
- 实现：core DeviceConvention(DepthMode)→viewport/frontface/clip/depth transform及shader builtin lowering，非单纯capability
- Turnip：host convention version；后续Adreno viewport/frontface/depth及ir3 builtin变体消费
- 优化：相同约定不失效；改变仅使依赖该约定的ProgramVariant/state块失效
- 寿命/同步：录制/提交跨约定改变的消费边界需oracle；不能悄悄改变已冻结状态
- 验收：上下原点、0..1/-1..1depth、cullface、shadow compare
- 门禁：坐标约定动态变更的时点需资格测试
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L17)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L242)

## nvnDeviceRegisterFastClearColor

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(NVNdevice*, const float*, NVNformat)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；DeviceRegisterFastClearColor对象操作
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.capabilities + core设备对象；DeviceRegisterFastClearColor对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：初始化/销毁边界；能力不可在同会话偷偷改变
- 验收：T-DEV: 属性/能力与实际执行一致，未知枚举明确失败；DeviceRegisterFastClearColor参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L18)

## nvnDeviceRegisterFastClearColori

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(NVNdevice*, const int*, NVNformat)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；DeviceRegisterFastClearColori对象操作
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.capabilities + core设备对象；DeviceRegisterFastClearColori对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：初始化/销毁边界；能力不可在同会话偷偷改变
- 验收：T-DEV: 属性/能力与实际执行一致，未知枚举明确失败；DeviceRegisterFastClearColori参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L19)

## nvnDeviceRegisterFastClearColorui

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(NVNdevice*, const uint32_t*, NVNformat)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；DeviceRegisterFastClearColorui对象操作
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.capabilities + core设备对象；DeviceRegisterFastClearColorui对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：初始化/销毁边界；能力不可在同会话偷偷改变
- 验收：T-DEV: 属性/能力与实际执行一致，未知枚举明确失败；DeviceRegisterFastClearColorui参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L20)

## nvnDeviceRegisterFastClearDepth

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(NVNdevice*, float)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；DeviceRegisterFastClearDepth对象操作
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.capabilities + core设备对象；DeviceRegisterFastClearDepth对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：初始化/销毁边界；能力不可在同会话偷偷改变
- 验收：T-DEV: 属性/能力与实际执行一致，未知枚举明确失败；DeviceRegisterFastClearDepth参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L21)

## nvnDeviceGetWindowOriginMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNwindowOriginMode (*)(const NVNdevice*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；WindowOriginMode字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.capabilities + core设备对象；WindowOriginMode字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：规范化WindowOriginMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：初始化/销毁边界；能力不可在同会话偷偷改变
- 验收：T-DEV: 属性/能力与实际执行一致，未知枚举明确失败；DeviceGetWindowOriginMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L22)

## nvnDeviceGetDepthMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNdepthMode (*)(const NVNdevice*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；DepthMode字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.capabilities + core设备对象；DepthMode字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：规范化DepthMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：初始化/销毁边界；能力不可在同会话偷偷改变
- 验收：T-DEV: 属性/能力与实际执行一致，未知枚举明确失败；DeviceGetDepthMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L23)

## nvnDeviceGetTimestampInNanoseconds

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`uint64_t (*)(const NVNdevice*, const NVNcounterData*)`；candidate-generated-header
- 使用：把指定NVN timestamp/report转换成纳秒
- 消费：host读取输入timestamp数据和已校准转换参数
- 实现：core convert_timestamp(raw,period,epoch)；不发新的GPU query
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：整数有理比例换算避免浮点漂移
- 寿命/同步：输入结构、计数宽度、wrap按NVN ABI；不同于CurrentTimestamp
- 验收：已知counter与ReportCounter对照、wrap、溢出
- 门禁：参数结构和period精度需oracle验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L24)

## nvnDeviceApplyDeferredFinalizes

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNdevice*, int)`；candidate-generated-header
- 使用：回收已经延迟的对象资源，而非Finalize设备
- 消费：host遍历满足完成ticket的退休记录
- 实现：core retire_queue.collect(device,completedTickets)
- Turnip：按completion ticket释放BO/descriptor/program和command块，不发draw
- 优化：批量释放BO/descriptor/program；未完成条目保留
- 寿命/同步：不能使仍有效device或guest对象失效；不猜8帧寿命
- 验收：完成/未完成混合、重复调用、引用链
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L25)

## nvnDeviceFinalizeCommandHandle

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdevice *d, NVNcommandHandle h)`；candidate-definition
- 使用：释放录制handle；不同于Finalize对象
- 消费：host释放guest引用；在途/Call引用保持
- 实现：core handle table generation+deferred retire
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：无GPU指令；查询实际fd_dev_info/tu物理能力并经NVN语义白名单转换
- 优化：按提交ticket批量释放native编译块
- 寿命/同步：Call/Copy区别保持；地址重用不复活旧handle
- 验收：先Finalize后GPU完成、嵌套Call引用、重复销毁
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L26)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L243)

## nvnQueueGetError

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`NVNqueueGetErrorResult (NVNqueue *q, NVNqueueErrorInfo *info)`；candidate-definition
- 使用：查询queue错误并可写错误详情
- 消费：host读取sticky error state并返回NVNqueueGetErrorResult
- 实现：core QueueErrorState；不发GPU包
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：健康queue O(1)读取
- 寿命/同步：错误输出结构必须真实大小；现header NVNqueueErrorInfo是TODO dummy[1]，不能按该sizeof写作完整实现
- 验收：GPU fault/device lost、NULL输出许可、结构大小哨兵
- 门禁：NVNqueueErrorInfo布局未知：ABI恢复前仅受限兼容模式，不能称通用支持
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L31)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L676)

## nvnQueueGetTotalCommandMemoryUsed

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(NVNqueue*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；TotalCommandMemoryUsed字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；TotalCommandMemoryUsed字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化TotalCommandMemoryUsed后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueGetTotalCommandMemoryUsed参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L32)

## nvnQueueGetTotalControlMemoryUsed

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(NVNqueue*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；TotalControlMemoryUsed字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；TotalControlMemoryUsed字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化TotalControlMemoryUsed后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueGetTotalControlMemoryUsed参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L33)

## nvnQueueGetTotalComputeMemoryUsed

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(NVNqueue*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；TotalComputeMemoryUsed字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；TotalComputeMemoryUsed字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化TotalComputeMemoryUsed后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueGetTotalComputeMemoryUsed参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L34)

## nvnQueueResetMemoryUsageCounts

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNqueue*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；QueueResetMemoryUsageCounts对象操作
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；QueueResetMemoryUsageCounts对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueResetMemoryUsageCounts参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L35)

## nvnQueueBuilderSetDevice

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueueBuilder *b, NVNdevice *d)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；保存设备身份并验证同设备引用
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderSetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L36)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L658)

## nvnQueueBuilderSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueueBuilder *b)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：NVNdevice *dev = QB(b)->dev; memset(b, 0, sizeof(queue_builder_t)); QB(b)->dev = dev; QB(b)->cmd_sz = 65536; QB(b)->comp_sz = 262144; QB(b)->ctl_sz = 16384; QB(b)->flush = 4096; // 设备默认值 | Device defaults
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L37)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L654)

## nvnQueueBuilderSetFlags

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueueBuilder *b, int f)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Flags后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderSetFlags参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L38)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L659)

## nvnQueueBuilderSetCommandMemorySize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueueBuilder *b, size_t n)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；命令内存预算/阈值，不等同hardware包实际字节
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；命令内存预算/阈值，不等同hardware包实际字节
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化CommandMemorySize后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderSetCommandMemorySize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L39)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L660)

## nvnQueueBuilderSetComputeMemorySize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueueBuilder *b, size_t n)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；compute队列scratch预算；后端硬件分配另记
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；compute队列scratch预算；后端硬件分配另记
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化ComputeMemorySize后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderSetComputeMemorySize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L40)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L661)

## nvnQueueBuilderSetControlMemorySize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueueBuilder *b, size_t n)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；控制记录预算，独立于GPU可读命令内存
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；控制记录预算，独立于GPU可读命令内存
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化ControlMemorySize后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderSetControlMemorySize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L41)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L662)

## nvnQueueBuilderGetQueueMemorySize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`size_t (const NVNqueueBuilder *b)`；candidate-definition
- 使用：获取调用方需要分配的queue存储大小
- 消费：host计算cmd/compute/control预算，size_t返回
- 实现：core queue_budget_size；当前兼容候选round_up(cmd+comp+ctl,4096)，所有加法校验溢出
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：纯字段计算缓存；native queue scratch另记不外露
- 寿命/同步：最小值/设备版本/flags对结果影响需oracle，不直接认定现公式为规范
- 验收：边界、零预算、overflow、对齐
- 门禁：当前公式是仓库兼容实现，通用NVN需oracle
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L42)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L666)

## nvnQueueBuilderSetQueueMemory

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueueBuilder *b, void *m, size_t n)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存调用者提供的队列存储及大小；不得越界写
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；保存调用者提供的队列存储及大小；不得越界写
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化QueueMemory后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderSetQueueMemory参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L43)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L670)

## nvnQueueBuilderSetCommandFlushThreshold

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueueBuilder *b, size_t n)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；触发提交的阈值是hint，不能越过未闭合RenderEpoch语义
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；触发提交的阈值是hint，不能越过未闭合RenderEpoch语义
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化CommandFlushThreshold后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderSetCommandFlushThreshold参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L44)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L663)

## nvnQueueBuilderSetQueuePriority

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；映射受内核允许的优先级；不请求实时权限或悄悄提高系统调度
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；映射受内核允许的优先级；不请求实时权限或悄悄提高系统调度
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化QueuePriority后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderSetQueuePriority参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L45)

## nvnQueueBuilderGetQueuePriority

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；映射受内核允许的优先级；不请求实时权限或悄悄提高系统调度
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；映射受内核允许的优先级；不请求实时权限或悄悄提高系统调度
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化QueuePriority后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderGetQueuePriority参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L46)

## nvnQueueBuilderGetDevice

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；保存设备身份并验证同设备引用
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderGetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L47)

## nvnQueueBuilderGetFlags

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化Flags后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderGetFlags参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L48)

## nvnQueueBuilderGetCommandMemorySize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；命令内存预算/阈值，不等同hardware包实际字节
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；命令内存预算/阈值，不等同hardware包实际字节
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化CommandMemorySize后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderGetCommandMemorySize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L49)

## nvnQueueBuilderGetComputeMemorySize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；compute队列scratch预算；后端硬件分配另记
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；compute队列scratch预算；后端硬件分配另记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化ComputeMemorySize后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderGetComputeMemorySize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L50)

## nvnQueueBuilderGetControlMemorySize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；控制记录预算，独立于GPU可读命令内存
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；控制记录预算，独立于GPU可读命令内存
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化ControlMemorySize后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderGetControlMemorySize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L51)

## nvnQueueBuilderGetCommandFlushThreshold

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；触发提交的阈值是hint，不能越过未闭合RenderEpoch语义
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；触发提交的阈值是hint，不能越过未闭合RenderEpoch语义
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化CommandFlushThreshold后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderGetCommandFlushThreshold参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L52)

## nvnQueueBuilderGetMemorySize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；MemorySize字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；MemorySize字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化MemorySize后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderGetMemorySize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L53)

## nvnQueueBuilderGetMemory

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；Memory字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；Memory字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化Memory后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueBuilderGetMemory参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L54)

## nvnQueueInitialize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`NVNboolean (NVNqueue *q, const NVNqueueBuilder *b)`；candidate-definition
- 使用：构造Queue对象
- 消费：host执行；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- 实现：BackendOps.queue_submit / present / fence_wait；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L55)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L671)

## nvnQueueFinalize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueue *q)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；QueueFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.queue_submit / present / fence_wait；QueueFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；QueueFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L56)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L675)

## nvnQueueSubmitCommands

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueue *q, int n, const NVNcommandHandle *h)`；candidate-definition
- 使用：每帧/批次提交handle数组；已有async实现复制数组
- 消费：host复制数组；提交消费命令版本；GPU执行读资源内容
- 实现：lower dependency graph→Adreno tu_knl submit / Mali JM or CSF submit
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：重用块/descriptor roots；每提交只检查dirty publication和依赖摘要
- 寿命/同步：命令内存patch在实际消费边界校验；跨队列显式timeline；资源退休按ticket
- 验收：CPU改handle数组、submit后改buffer、跨线程提交、重复块
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L58)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L1282)

## nvnQueueFlush

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueue *q)`；candidate-definition
- 使用：将已排队的GPU工作推进到提交，不要求完成
- 消费：host触发pending batches提交；返回不代表GPU完成
- 实现：queue_submit(flush_pending)；Adreno/Mali kernel submit
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：仅非空批次发ioctl；不强加finish
- 寿命/同步：保留早先queue命令顺序；不可像旧实现无条件nop
- 验收：Flush后另一queue等待其fence；CPU持续工作不阻塞
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L59)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L679)

## nvnQueueFinish

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueue *q)`；candidate-definition
- 使用：CPU等待本queue先前全部工作完成，无timeout参数
- 消费：host先flush再无超时等待该queue尾ticket
- 实现：queue_submit+fence_wait(queue_tail)，只需本queue及其依赖
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：空queue立即返回；不全设备idle
- 寿命/同步：返回void；device lost错误走规定机制；不把CPU翻译完成当GPU完成
- 验收：pending work、跨queue依赖、空队列、device lost
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L60)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L680)

## nvnQueuePresentTexture

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueue *q, NVNwindow *w, int idx)`；candidate-definition
- 使用：将指定guest window texture呈现
- 消费：host记录index；GPU完成所需store/转换后present
- 实现：Adreno GMEM store/Mali tile store→WSI image+release fence
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：能直写可scanout图像则省最终blit；不为呈现强制全队列idle
- 寿命/同步：window format/颜色空间/旋转/crop保持；present release≠GPU全局完成
- 验收：颜色空间、crop、多个window、异步present重用
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L61)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L1683)

## nvnQueueAcquireTexture

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNqueueAcquireTextureResult (*)(NVNqueue*, NVNwindow*, int*)`；candidate-generated-header
- 使用：在queue中获取window下一可渲染纹理
- 消费：host返回index及状态；acquire fence加入本queue后续依赖
- 实现：WSI acquire→queue_wait(native acquire fence)；guest index映射
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：无需CPU等GPU；同present链复用native sync
- 寿命/同步：后续写附件必须等release；queueacquire不等于WindowAcquire的sync返回接口
- 验收：多queue、重建、swapchain枯竭、索引复用
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L62)

## nvnWindowBuilderSetDevice

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNwindowBuilder *b, NVNdevice *d)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；保存设备身份并验证同设备引用
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderSetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L63)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L1356)

## nvnWindowBuilderSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNwindowBuilder *b)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：NVNdevice *dev = WB(b)->dev; memset(b, 0, sizeof(win_builder_t)); WB(b)->dev = dev; WB(b)->interval = 1;
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L64)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L1355)

## nvnWindowBuilderSetNativeWindow

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNwindowBuilder *b, NVNnativeWindow w)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；host窗口桥身份；不可直接当Android native指针解引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；host窗口桥身份；不可直接当Android native指针解引用
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化NativeWindow后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderSetNativeWindow参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L65)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L1357)

## nvnWindowBuilderSetTextures

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNwindowBuilder *b, int n, NVNtexture *const *t)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；复制纹理对象身份数组；保持guest swapchain index→host image映射
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；复制纹理对象身份数组；保持guest swapchain index→host image映射
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Textures后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderSetTextures参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L66)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L1358)

## nvnWindowBuilderSetPresentInterval

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNwindowBuilder *b, int v)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；guest间隔转换为present pacing目标，不假装增加GPU性能
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；guest间隔转换为present pacing目标，不假装增加GPU性能
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化PresentInterval后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderSetPresentInterval参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L67)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L1362)

## nvnWindowBuilderSetNumActiveTextures

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；活跃buffer数，不能在in-flight撤回正在显示的纹理
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；活跃buffer数，不能在in-flight撤回正在显示的纹理
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化NumActiveTextures后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderSetNumActiveTextures参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L68)

## nvnWindowBuilderGetDevice

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；保存设备身份并验证同设备引用
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderGetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L69)

## nvnWindowBuilderGetNumTextures

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；NumTextures字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；NumTextures字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化NumTextures后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderGetNumTextures参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L70)

## nvnWindowBuilderGetTexture

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；Texture字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；Texture字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化Texture后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderGetTexture参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L71)

## nvnWindowBuilderGetNativeWindow

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNnativeWindow (*)(const NVNwindowBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；host窗口桥身份；不可直接当Android native指针解引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；host窗口桥身份；不可直接当Android native指针解引用
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化NativeWindow后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderGetNativeWindow参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L72)

## nvnWindowBuilderGetPresentInterval

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNwindowBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；guest间隔转换为present pacing目标，不假装增加GPU性能
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；guest间隔转换为present pacing目标，不假装增加GPU性能
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化PresentInterval后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderGetPresentInterval参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L73)

## nvnWindowBuilderGetNumActiveTextures

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；活跃buffer数，不能在in-flight撤回正在显示的纹理
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；活跃buffer数，不能在in-flight撤回正在显示的纹理
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化NumActiveTextures后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowBuilderGetNumActiveTextures参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L74)

## nvnWindowInitialize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`NVNboolean (NVNwindow *w, const NVNwindowBuilder *b)`；candidate-definition
- 使用：构造Window对象
- 消费：host执行；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- 实现：BackendOps.queue_submit / present / fence_wait；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L75)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L1363)

## nvnWindowFinalize

- 阶段/证据：P0-D2R静态调用集合；static=1；resolver-nop
- 候选ABI：`void (*)(NVNwindow*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；WindowFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.queue_submit / present / fence_wait；WindowFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L76)

## nvnWindowAcquireTexture

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`NVNwindowAcquireTextureResult (NVNwindow *w, NVNsync *s, int *idx)`；candidate-definition
- 使用：获得可渲染swapchain索引及可用sync
- 消费：host获得host image，写guest idx，建立acquire sync
- 实现：Android WSI acquire/native fence→queue dependency；guest index映射
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：直接可用已匹配格式native目标时零copy；否则明确present blit
- 寿命/同步：不得复用仍显示/在途图像；返回状态准确
- 验收：双/三buffer、重建、超时、crop、旋转
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L78)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L1388)

## nvnWindowGetNativeWindow

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNnativeWindow (*)(const NVNwindow*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；host窗口桥身份；不可直接当Android native指针解引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；host窗口桥身份；不可直接当Android native指针解引用
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化NativeWindow后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowGetNativeWindow参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L79)

## nvnWindowGetPresentInterval

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNwindow*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；guest间隔转换为present pacing目标，不假装增加GPU性能
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；guest间隔转换为present pacing目标，不假装增加GPU性能
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化PresentInterval后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowGetPresentInterval参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L80)

## nvnWindowSetPresentInterval

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNwindow *w, int v)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；guest间隔转换为present pacing目标，不假装增加GPU性能
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；guest间隔转换为present pacing目标，不假装增加GPU性能
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化PresentInterval后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowSetPresentInterval参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L81)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L1372)

## nvnWindowSetCrop

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNwindow *w, int x, int y, int ww, int hh)`；candidate-definition
- 使用：设置窗口呈现裁剪矩形
- 消费：host复制x/y/w/h；present入队时快照该版本
- 实现：core Window.crop→WSI present transform/blit裁剪；setter不submit
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：crop不变不更新present状态
- 寿命/同步：pending present持有自己的crop；窗口大小变化重新验证
- 验收：边缘/负/越界按ABI、present排队后改crop、旋转
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L82)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L1373)

## nvnWindowGetCrop

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNwindow*, NVNrectangle*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；Crop字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；Crop字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化Crop后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowGetCrop参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L83)

## nvnWindowSetNumActiveTextures

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；活跃buffer数，不能在in-flight撤回正在显示的纹理
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；活跃buffer数，不能在in-flight撤回正在显示的纹理
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化NumActiveTextures后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowSetNumActiveTextures参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L84)

## nvnWindowGetNumActiveTextures

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；活跃buffer数，不能在in-flight撤回正在显示的纹理
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；活跃buffer数，不能在in-flight撤回正在显示的纹理
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化NumActiveTextures后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowGetNumActiveTextures参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L85)

## nvnWindowGetNumTextures

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；NumTextures字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.queue_submit / present / fence_wait；NumTextures字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl.h的BO/submit接口；KGSL或MSM；WSI native fence
- 优化：规范化NumTextures后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：每队列有序timeline；跨队列等待必须携带显式依赖；present独立于GPU工作完成
- 验收：T-QUEUE: 双队列producer/consumer、acquire/present重用、超时和设备丢失；WindowGetNumTextures参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L86)
