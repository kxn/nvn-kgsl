# v3 10-api-02-memory-descriptors：Turnip调用合同
这是设计覆盖，不是已实现API。v3新增门禁和私有profile以本包normative contracts为准。

## nvnDeviceGetTextureHandle

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNtextureHandle (*)(const NVNdevice*, int, int)`；candidate-generated-header
- 使用：生成Texture可供shader/Bind调用的guest句柄
- 消费：host立即组合/验证guest索引；不是提前读取纹理内容
- 实现：shader ABI handle decode→pool根+slot；Adreno bindless/Mali CSF资源表
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：稳定索引；不为每个handle创建descriptor set
- 寿命/同步：句柄数值语义不直接暴露native pointer；pool切换仍正确
- 验收：边界ID、same ID different pool、GPU存储句柄再读取
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L11)

## nvnDeviceGetTexelFetchHandle

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNtextureHandle (*)(const NVNdevice*, int)`；candidate-generated-header
- 使用：生成TexelFetch可供shader/Bind调用的guest句柄
- 消费：host立即组合/验证guest索引；不是提前读取纹理内容
- 实现：shader ABI handle decode→pool根+slot；Adreno bindless/Mali CSF资源表
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：稳定索引；不为每个handle创建descriptor set
- 寿命/同步：句柄数值语义不直接暴露native pointer；pool切换仍正确
- 验收：边界ID、same ID different pool、GPU存储句柄再读取
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L12)

## nvnDeviceGetImageHandle

- 阶段/证据：P0-D2R静态调用集合；static=4；source-defined
- 候选ABI：`NVNimageHandle (const NVNdevice *d, int id)`；candidate-definition
- 使用：生成Image可供shader/Bind调用的guest句柄
- 消费：host立即组合/验证guest索引；不是提前读取纹理内容
- 实现：shader ABI handle decode→pool根+slot；Adreno bindless/Mali CSF资源表
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：稳定索引；不为每个handle创建descriptor set
- 寿命/同步：句柄数值语义不直接暴露native pointer；pool切换仍正确
- 验收：边界ID、same ID different pool、GPU存储句柄再读取
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L13)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L233)

## nvnDeviceGetSeparateTextureHandle

- 阶段/证据：P0-D2R静态调用集合；static=4；source-defined
- 候选ABI：`NVNseparateTextureHandle (const NVNdevice *d, int id)`；candidate-definition
- 使用：生成SeparateTexture可供shader/Bind调用的guest句柄
- 消费：host立即组合/验证guest索引；不是提前读取纹理内容
- 实现：shader ABI handle decode→pool根+slot；Adreno bindless/Mali CSF资源表
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：稳定索引；不为每个handle创建descriptor set
- 寿命/同步：句柄数值语义不直接暴露native pointer；pool切换仍正确
- 验收：边界ID、same ID different pool、GPU存储句柄再读取
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L28)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L236)

## nvnDeviceGetSeparateSamplerHandle

- 阶段/证据：P0-D2R静态调用集合；static=7；source-defined
- 候选ABI：`NVNseparateSamplerHandle (const NVNdevice *d, int id)`；candidate-definition
- 使用：生成SeparateSampler可供shader/Bind调用的guest句柄
- 消费：host立即组合/验证guest索引；不是提前读取纹理内容
- 实现：shader ABI handle decode→pool根+slot；Adreno bindless/Mali CSF资源表
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：稳定索引；不为每个handle创建descriptor set
- 寿命/同步：句柄数值语义不直接暴露native pointer；pool切换仍正确
- 验收：边界ID、same ID different pool、GPU存储句柄再读取
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L29)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L237)

## nvnMemoryPoolBuilderSetDevice

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNmemoryPoolBuilder *b, NVNdevice *d)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；保存设备身份并验证同设备引用
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolBuilderSetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L93)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L248)

## nvnMemoryPoolBuilderSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNmemoryPoolBuilder *b)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：memset(b, 0, sizeof(pool_builder_t));
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolBuilderSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L94)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L247)

## nvnMemoryPoolBuilderSetStorage

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNmemoryPoolBuilder *b, void *m, size_t n)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存pool/offset/size或host指针；检查溢出、对齐和pool容量；不复制资源字节
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；保存pool/offset/size或host指针；检查溢出、对齐和pool容量；不复制资源字节
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Storage后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolBuilderSetStorage参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L95)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L249)

## nvnMemoryPoolBuilderSetFlags

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNmemoryPoolBuilder *b, int f)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Flags后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolBuilderSetFlags参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L96)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L250)

## nvnMemoryPoolBuilderGetDevice

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；保存设备身份并验证同设备引用
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolBuilderGetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L97)

## nvnMemoryPoolBuilderGetMemory

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNmemoryPoolBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；Memory字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；Memory字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化Memory后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolBuilderGetMemory参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L98)

## nvnMemoryPoolBuilderGetSize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNmemoryPoolBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；精确逻辑大小；内部分配向上取整不可外露
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；精确逻辑大小；内部分配向上取整不可外露
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化Size后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolBuilderGetSize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L99)

## nvnMemoryPoolBuilderGetFlags

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNmemoryPoolFlags (*)(const NVNmemoryPoolBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化Flags后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolBuilderGetFlags参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L100)

## nvnMemoryPoolInitialize

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`NVNboolean (NVNmemoryPool *p, const NVNmemoryPoolBuilder *b)`；candidate-definition
- 使用：构造MemoryPool对象
- 消费：host执行；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- 实现：BackendOps.memory_map_bind / cache_domain_ops；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L101)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L251)

## nvnMemoryPoolFinalize

- 阶段/证据：P0-D2R静态调用集合；static=8；source-defined
- 候选ABI：`void (NVNmemoryPool *p)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；MemoryPoolFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.memory_map_bind / cache_domain_ops；MemoryPoolFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L103)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L263)

## nvnMemoryPoolMap

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void* (*)(const NVNmemoryPool*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；MemoryPoolMap对象操作
- 消费：host返回有效guest字节映射；不是GPU完成保证
- 实现：memory_map_bind映射线性BO或guest shadow；不把native tiled像素当guest bytes
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：保持稳定mapping；发布epoch触发最小同步
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolMap参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L104)

## nvnMemoryPoolFlushMappedRange

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (const NVNmemoryPool *p, ptrdiff_t off, size_t n)`；candidate-definition
- 使用：对映射区间执行HOST→GPU可见性维护
- 消费：host读取offset/size并维护CPU缓存；不隐式建立GPU执行完成
- 实现：cache_domain_ops(HOST→GPU,ResourceId,offset,size)；对齐cacheline且不丢邻近dirtybytes
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：相邻范围合并；coherent内存可省实际cache维护但保留发布协议
- 寿命/同步：Flush不等于GPU已读完；Invalidate不等于GPU已写完；执行依赖由sync/barrier负责
- 验收：非coherent双向写、共享cacheline、跨页、小范围、range溢出
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L105)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L277)

## nvnMemoryPoolInvalidateMappedRange

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；source-defined
- 候选ABI：`void (const NVNmemoryPool *p, ptrdiff_t off, size_t n)`；candidate-definition
- 使用：对映射区间执行GPU→HOST可见性维护
- 消费：host读取offset/size并维护CPU缓存；不隐式建立GPU执行完成
- 实现：cache_domain_ops(GPU→HOST,ResourceId,offset,size)；对齐cacheline且不丢邻近dirtybytes
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：相邻范围合并；coherent内存可省实际cache维护但保留发布协议
- 寿命/同步：Flush不等于GPU已读完；Invalidate不等于GPU已写完；执行依赖由sync/barrier负责
- 验收：非coherent双向写、共享cacheline、跨页、小范围、range溢出
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L106)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L286)

## nvnMemoryPoolGetBufferAddress

- 阶段/证据：P0-D2R静态调用集合；static=12；source-defined
- 候选ABI：`NVNbufferAddress (const NVNmemoryPool *p)`；candidate-definition
- 使用：查询pool的guest GPU虚拟基地址
- 消费：host返回guest GPUVA64，非CPU指针
- 实现：core GuestAddressSpace.lookup(pool)→后续ResourceId/MapEpoch重定位
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：稳定guest VA；native BO/IOVA变更不改变guest地址
- 寿命/同步：CPU map/guest GPUVA/native GPUVA三者分离；virtual pool允许没有CPU存储
- 验收：64GB虚拟池、物理映射、sameCPUaddr重用、跨pool
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L107)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L295)

## nvnMemoryPoolMapVirtual

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(NVNmemoryPool*, int, const NVNmappingRequest*)`；candidate-generated-header
- 使用：大guest虚拟池稀疏映射；MHR例64GB虚拟池分段4MB物理映射
- 消费：host复制mapping条目并创建映射版本
- 实现：Adreno内核sparse/VA或软件页表；Mali CSF VM_BIND；JM能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：区间树按被改区间更新，禁止按64GB分配/扫描
- 寿命/同步：映射重绑定不得改变在途旧版本；range alias和physicalpool寿命跟踪
- 验收：稀疏洞、别名、重绑、4MB块跨边界及GPU在途
- 门禁：内核导入/VA约束必须资格测试；无稀疏能力则显式限制
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L108)

## nvnMemoryPoolGetSize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNmemoryPool*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；精确逻辑大小；内部分配向上取整不可外露
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；精确逻辑大小；内部分配向上取整不可外露
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化Size后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolGetSize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L109)

## nvnMemoryPoolGetFlags

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNmemoryPoolFlags (*)(const NVNmemoryPool*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化Flags后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；MemoryPoolGetFlags参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L110)

## nvnTexturePoolInitialize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`NVNboolean (NVNtexturePool *tp, const NVNmemoryPool *p, ptrdiff_t off, int n)`；candidate-definition
- 使用：构造TexturePool对象
- 消费：host执行；保存p身份、off字节偏移、n槽容量；验证descriptor对齐及off+n×guestDescriptorSize不越界
- 实现：BackendOps.descriptor_encode；保存p身份、off字节偏移、n槽容量；验证descriptor对齐及off+n×guestDescriptorSize不越界
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L111)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L299)

## nvnTexturePoolFinalize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNtexturePool *tp)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；TexturePoolFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.descriptor_encode；TexturePoolFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：DescriptorVersion不可被在途draw覆盖；CPU表写与GPU描述符缓存失效都需排序
- 验收：T-DESC: 相同slot重注册、切pool同ID、in-flight版本、分离/组合句柄；TexturePoolFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L113)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L302)

## nvnTexturePoolRegisterTexture

- 阶段/证据：P0-D2R静态调用集合；static=5；source-defined
- 候选ABI：`void (const NVNtexturePool *tp, int id, const NVNtexture *t, const NVNtextureView *v)`；candidate-definition
- 使用：加载/更新资源槽；Texture注册
- 消费：host复制对象/view/参数元数据，发布DescriptorVersion；不快照像素
- 实现：encode Texture native descriptor→Adreno bindless / CSF32byte resource cells；JM类型表
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：register时encode；draw只索引，避免资源解析后才查缓存
- 寿命/同步：同slot in-flight覆盖须版本化/root重定向并保留提交可见语义
- 验收：同ID不同pool、重复注册、注册后改对象、在途重注册
- 门禁：Register时预编码不等于冻结每个已录制draw应见版本；record/submit/execute间重注册的消费时点必须oracle，按该时点选择DescriptorVersion，不能一律COW成旧描述符
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L114)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L303)

## nvnTexturePoolRegisterImage

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNtexturePool*, int, const NVNtexture*, const NVNtextureView*)`；candidate-generated-header
- 使用：加载/更新资源槽；Image注册
- 消费：host复制对象/view/参数元数据，发布DescriptorVersion；不快照像素
- 实现：encode Image native descriptor→Adreno bindless / CSF32byte resource cells；JM类型表
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：register时encode；draw只索引，避免资源解析后才查缓存
- 寿命/同步：同slot in-flight覆盖须版本化/root重定向并保留提交可见语义
- 验收：同ID不同pool、重复注册、注册后改对象、在途重注册
- 门禁：Register时预编码不等于冻结每个已录制draw应见版本；record/submit/execute间重注册的消费时点必须oracle，按该时点选择DescriptorVersion，不能一律COW成旧描述符
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L115)

## nvnTexturePoolGetMemoryPool

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`const NVNmemoryPool* (*)(const NVNtexturePool*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回对象原来的guest pool身份，不泄露host BO
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode；返回对象原来的guest pool身份，不泄露host BO
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：规范化MemoryPool后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：DescriptorVersion不可被在途draw覆盖；CPU表写与GPU描述符缓存失效都需排序
- 验收：T-DESC: 相同slot重注册、切pool同ID、in-flight版本、分离/组合句柄；TexturePoolGetMemoryPool参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L116)

## nvnTexturePoolGetMemoryOffset

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`ptrdiff_t (*)(const NVNtexturePool*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回相对guest pool的偏移，非host native重排偏移
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode；返回相对guest pool的偏移，非host native重排偏移
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：规范化MemoryOffset后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：DescriptorVersion不可被在途draw覆盖；CPU表写与GPU描述符缓存失效都需排序
- 验收：T-DESC: 相同slot重注册、切pool同ID、in-flight版本、分离/组合句柄；TexturePoolGetMemoryOffset参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L117)

## nvnTexturePoolGetSize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNtexturePool*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；精确逻辑大小；内部分配向上取整不可外露
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode；精确逻辑大小；内部分配向上取整不可外露
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：规范化Size后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：DescriptorVersion不可被在途draw覆盖；CPU表写与GPU描述符缓存失效都需排序
- 验收：T-DESC: 相同slot重注册、切pool同ID、in-flight版本、分离/组合句柄；TexturePoolGetSize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L118)

## nvnSamplerPoolInitialize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`NVNboolean (NVNsamplerPool *sp, const NVNmemoryPool *p, ptrdiff_t off, int n)`；candidate-definition
- 使用：构造SamplerPool对象
- 消费：host执行；保存p身份、off字节偏移、n槽容量；验证descriptor对齐及off+n×guestDescriptorSize不越界
- 实现：BackendOps.descriptor_encode；保存p身份、off字节偏移、n槽容量；验证descriptor对齐及off+n×guestDescriptorSize不越界
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L119)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L304)

## nvnSamplerPoolFinalize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNsamplerPool *sp)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；SamplerPoolFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.descriptor_encode；SamplerPoolFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：DescriptorVersion不可被在途draw覆盖；CPU表写与GPU描述符缓存失效都需排序
- 验收：T-DESC: 相同slot重注册、切pool同ID、in-flight版本、分离/组合句柄；SamplerPoolFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L121)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L307)

## nvnSamplerPoolRegisterSampler

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (const NVNsamplerPool *sp, int id, const NVNsampler *s)`；candidate-definition
- 使用：加载/更新资源槽；Sampler注册
- 消费：host复制对象/view/参数元数据，发布DescriptorVersion；不快照像素
- 实现：encode Sampler native descriptor→Adreno bindless / CSF32byte resource cells；JM类型表
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：register时encode；draw只索引，避免资源解析后才查缓存
- 寿命/同步：同slot in-flight覆盖须版本化/root重定向并保留提交可见语义
- 验收：同ID不同pool、重复注册、注册后改对象、在途重注册
- 门禁：Register时预编码不等于冻结每个已录制draw应见版本；record/submit/execute间重注册的消费时点必须oracle，按该时点选择DescriptorVersion，不能一律COW成旧描述符
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L122)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L308)

## nvnSamplerPoolRegisterSamplerBuilder

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsamplerPool*, int, const NVNsamplerBuilder*)`；candidate-generated-header
- 使用：加载/更新资源槽；SamplerBuilder注册
- 消费：host复制对象/view/参数元数据，发布DescriptorVersion；不快照像素
- 实现：encode SamplerBuilder native descriptor→Adreno bindless / CSF32byte resource cells；JM类型表
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：register时encode；draw只索引，避免资源解析后才查缓存
- 寿命/同步：同slot in-flight覆盖须版本化/root重定向并保留提交可见语义
- 验收：同ID不同pool、重复注册、注册后改对象、在途重注册
- 门禁：Register时预编码不等于冻结每个已录制draw应见版本；record/submit/execute间重注册的消费时点必须oracle，按该时点选择DescriptorVersion，不能一律COW成旧描述符
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L123)

## nvnSamplerPoolGetMemoryPool

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`const NVNmemoryPool* (*)(const NVNsamplerPool*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回对象原来的guest pool身份，不泄露host BO
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode；返回对象原来的guest pool身份，不泄露host BO
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：规范化MemoryPool后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：DescriptorVersion不可被在途draw覆盖；CPU表写与GPU描述符缓存失效都需排序
- 验收：T-DESC: 相同slot重注册、切pool同ID、in-flight版本、分离/组合句柄；SamplerPoolGetMemoryPool参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L124)

## nvnSamplerPoolGetMemoryOffset

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`ptrdiff_t (*)(const NVNsamplerPool*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回相对guest pool的偏移，非host native重排偏移
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode；返回相对guest pool的偏移，非host native重排偏移
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：规范化MemoryOffset后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：DescriptorVersion不可被在途draw覆盖；CPU表写与GPU描述符缓存失效都需排序
- 验收：T-DESC: 相同slot重注册、切pool同ID、in-flight版本、分离/组合句柄；SamplerPoolGetMemoryOffset参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L125)

## nvnSamplerPoolGetSize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNsamplerPool*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；精确逻辑大小；内部分配向上取整不可外露
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode；精确逻辑大小；内部分配向上取整不可外露
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：规范化Size后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：DescriptorVersion不可被在途draw覆盖；CPU表写与GPU描述符缓存失效都需排序
- 验收：T-DESC: 相同slot重注册、切pool同ID、in-flight版本、分离/组合句柄；SamplerPoolGetSize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L126)

## nvnBufferBuilderSetDevice

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNbufferBuilder*, NVNdevice*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；保存设备身份并验证同设备引用
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferBuilderSetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L127)

## nvnBufferBuilderSetDefaults

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNbufferBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferBuilderSetDefaults参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L128)

## nvnBufferBuilderSetStorage

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNbufferBuilder*, NVNmemoryPool*, ptrdiff_t, size_t)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；保存pool/offset/size或host指针；检查溢出、对齐和pool容量；不复制资源字节
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；保存pool/offset/size或host指针；检查溢出、对齐和pool容量；不复制资源字节
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Storage后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferBuilderSetStorage参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L129)

## nvnBufferBuilderGetDevice

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；保存设备身份并验证同设备引用
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferBuilderGetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L130)

## nvnBufferBuilderGetMemoryPool

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNmemoryPool (*)(const NVNbufferBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回对象原来的guest pool身份，不泄露host BO
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；返回对象原来的guest pool身份，不泄露host BO
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化MemoryPool后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferBuilderGetMemoryPool参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L131)

## nvnBufferBuilderGetMemoryOffset

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`ptrdiff_t (*)(const NVNbufferBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回相对guest pool的偏移，非host native重排偏移
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；返回相对guest pool的偏移，非host native重排偏移
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化MemoryOffset后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferBuilderGetMemoryOffset参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L132)

## nvnBufferBuilderGetSize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNbufferBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；精确逻辑大小；内部分配向上取整不可外露
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；精确逻辑大小；内部分配向上取整不可外露
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化Size后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferBuilderGetSize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L133)

## nvnBufferInitialize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(NVNbuffer*, const NVNbufferBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；BufferInitialize对象操作
- 消费：host验证builder并复制元数据；分配所需native对象，失败返回而非构造半对象
- 实现：BackendOps.memory_map_bind / cache_domain_ops；对象代数+设备归属+参数合法性校验
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：同参数派生描述intern，不共享guest可变对象身份
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferInitialize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L134)

## nvnBufferFinalize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNbuffer*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；BufferFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.memory_map_bind / cache_domain_ops；BufferFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L136)

## nvnBufferMap

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void* (*)(const NVNbuffer*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；BufferMap对象操作
- 消费：host返回有效guest字节映射；不是GPU完成保证
- 实现：memory_map_bind映射线性BO或guest shadow；不把native tiled像素当guest bytes
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：保持稳定mapping；发布epoch触发最小同步
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferMap参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L137)

## nvnBufferGetAddress

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNbufferAddress (*)(const NVNbuffer*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；Address字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：core返回guest地址空间身份；后端通过ResourceId/VA映射解码，绝不将guest pointer默认为native IOVA
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化Address后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferGetAddress参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L138)

## nvnBufferFlushMappedRange

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNbuffer*, ptrdiff_t, size_t)`；candidate-generated-header
- 使用：对映射区间执行HOST→GPU可见性维护
- 消费：host读取offset/size并维护CPU缓存；不隐式建立GPU执行完成
- 实现：cache_domain_ops(HOST→GPU,ResourceId,offset,size)；对齐cacheline且不丢邻近dirtybytes
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：相邻范围合并；coherent内存可省实际cache维护但保留发布协议
- 寿命/同步：Flush不等于GPU已读完；Invalidate不等于GPU已写完；执行依赖由sync/barrier负责
- 验收：非coherent双向写、共享cacheline、跨页、小范围、range溢出
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L139)

## nvnBufferInvalidateMappedRange

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNbuffer*, ptrdiff_t, size_t)`；candidate-generated-header
- 使用：对映射区间执行GPU→HOST可见性维护
- 消费：host读取offset/size并维护CPU缓存；不隐式建立GPU执行完成
- 实现：cache_domain_ops(GPU→HOST,ResourceId,offset,size)；对齐cacheline且不丢邻近dirtybytes
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：相邻范围合并；coherent内存可省实际cache维护但保留发布协议
- 寿命/同步：Flush不等于GPU已读完；Invalidate不等于GPU已写完；执行依赖由sync/barrier负责
- 验收：非coherent双向写、共享cacheline、跨页、小范围、range溢出
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L140)

## nvnBufferGetMemoryPool

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNmemoryPool* (*)(const NVNbuffer*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回对象原来的guest pool身份，不泄露host BO
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；返回对象原来的guest pool身份，不泄露host BO
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化MemoryPool后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferGetMemoryPool参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L141)

## nvnBufferGetMemoryOffset

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`ptrdiff_t (*)(const NVNbuffer*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回相对guest pool的偏移，非host native重排偏移
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；返回相对guest pool的偏移，非host native重排偏移
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化MemoryOffset后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferGetMemoryOffset参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L142)

## nvnBufferGetSize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNbuffer*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；精确逻辑大小；内部分配向上取整不可外露
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.memory_map_bind / cache_domain_ops；精确逻辑大小；内部分配向上取整不可外露
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_knl BO、IOVA和sparse hooks；coherent host import仅按内核资格
- 优化：规范化Size后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：pool generation、alias映射版本、CPU发布epoch、GPU完成ticket相互独立
- 验收：T-MEM: 双向CPU/GPU写、重叠alias、跨页/范围/退休、非coherent路径；BufferGetSize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L143)

## nvnCommandBufferSetTexturePool

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNtexturePool *p)`；candidate-definition
- 使用：切换当前descriptor池，不是全局ID空间
- 消费：record pool身份及有效代；descriptor槽可见时点依规范发布
- 实现：Adreno bindless root / Mali CSF resource table root；JM类型表根
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：每pool维护持久原生表；root同值不重发
- 寿命/同步：pool寿命延长至命令完成；槽变化只失效对应DescriptorVersion
- 验收：同ID切pool、在途销毁、register后复用命令
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L468)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L912)

## nvnCommandBufferSetSamplerPool

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNsamplerPool *p)`；candidate-definition
- 使用：切换当前descriptor池，不是全局ID空间
- 消费：record pool身份及有效代；descriptor槽可见时点依规范发布
- 实现：Adreno bindless root / Mali CSF resource table root；JM类型表根
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl视图编码+SP bindless根；SP_UPDATE_CNTL失效
- 优化：每pool维护持久原生表；root同值不重发
- 寿命/同步：pool寿命延长至命令完成；槽变化只失效对应DescriptorVersion
- 验收：同ID切pool、在途销毁、register后复用命令
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L469)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L913)
