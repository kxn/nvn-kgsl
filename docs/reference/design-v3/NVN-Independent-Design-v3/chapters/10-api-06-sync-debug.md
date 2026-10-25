# v3 10-api-06-sync-debug：Turnip调用合同
这是设计覆盖，不是已实现API。v3新增门禁和私有profile以本包normative contracts为准。

## nvnDeviceSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNdevice*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L6)

## nvnDeviceInstallDebugCallback

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdevice *d, const PFNNVNDEBUGCALLBACKPROC cb, void *data, uint32_t enable)`；candidate-definition
- 使用：安装/移除诊断回调及userdata
- 消费：host保存callback/data并按enable低8位开启
- 实现：core diagnostic callback bridge；无GPU包
- Turnip：仅host resolver/callback桥；无GPU操作
- 优化：未安装时branch-only；不能在draw热路径构造字符串
- 寿命/同步：调用时不持不可重入内部锁；回调ABI/thread与userdata寿命明确
- 验收：重入API、卸载、错误消息、user data改写
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L14)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L238)

## nvnDeviceGenerateDebugDomainId

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNdebugDomainId (*)(const NVNdevice*, const char*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；DeviceGenerateDebugDomainId对象操作
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；DeviceGenerateDebugDomainId对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；DeviceGenerateDebugDomainId参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L15)

## nvnDeviceWalkDebugDatabase

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNdevice*, NVNdebugObjectType, PFNNVNWALKDEBUGDATABASECALLBACKPROC, void*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；DeviceWalkDebugDatabase对象操作
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；DeviceWalkDebugDatabase对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；DeviceWalkDebugDatabase参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L27)

## nvnDeviceIsExternalDebuggerAttached

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNdevice*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；DeviceIsExternalDebuggerAttached对象操作
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；DeviceIsExternalDebuggerAttached对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；DeviceIsExternalDebuggerAttached参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L30)

## nvnQueueSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNqueue*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L57)

## nvnWindowSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNwindow*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L77)

## nvnProgramSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNprogram*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L89)

## nvnMemoryPoolSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNmemoryPool*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L102)

## nvnTexturePoolSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNtexturePool*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L112)

## nvnSamplerPoolSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNsamplerPool*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L120)

## nvnBufferSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNbuffer*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L135)

## nvnBufferGetDebugID

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`uint64_t (*)(const NVNbuffer*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；DebugID字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；DebugID字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化DebugID后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；BufferGetDebugID参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L144)

## nvnTextureBuilderGetGLTextureName

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`uint32_t (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；GLTextureName字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；GLTextureName字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化GLTextureName后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；TextureBuilderGetGLTextureName参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L183)

## nvnTextureSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNtexture*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L205)

## nvnTextureGetDebugID

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`uint64_t (*)(const NVNtexture*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；DebugID字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；DebugID字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化DebugID后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；TextureGetDebugID参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L231)

## nvnSamplerSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNsampler*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L260)

## nvnSamplerGetDebugID

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`uint64_t (*)(const NVNsampler*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；DebugID字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；DebugID字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化DebugID后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；SamplerGetDebugID参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L272)

## nvnCommandBufferSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：本入口仅host recorder/control操作，不发GPU包；后续draw/submit才消费记录产物
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L355)

## nvnCommandBufferReportCounter

- 阶段/证据：P0-D2R静态调用集合；static=8；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNcounterType t, NVNbufferAddress a)`；candidate-definition
- 使用：计数/时间戳/值写回guest内存
- 消费：录制counter/value/address；执行时取计数并写报告
- 实现：Adreno query events；Mali occlusion/timestamp endpoint；其他counter能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：query pool slab与report地址稳定；无CPU阻塞读回
- 寿命/同步：NVN report结构/时间单位/availability需精确；多tile重复不得重复累计
- 验收：多tile计数与SYSMEM对照、reset位置、CPU读取完成
- 门禁：Mali pipeline statistics无stock实现，不能返回伪0
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L453)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L789)

## nvnCommandBufferResetCounter

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNcounterType t)`；candidate-definition
- 使用：重置指定counter，不生成report
- 消费：record counterType；GPU执行于命令位置reset accumulator
- 实现：NvnOp.ResetCounter(type)→Adreno counter init事件 / Mali counter accumulator reset
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：同区间无观测前冗余reset可合并；不得跨query consumer
- 寿命/同步：没有目标报告地址；tile重复不能重复reset全局累计
- 验收：reset/draw/report/draw/report，跨tile求和
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L454)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L786)

## nvnCommandBufferReportValue

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t, NVNbufferAddress)`；candidate-generated-header
- 使用：计数/时间戳/值写回guest内存
- 消费：录制counter/value/address；执行时取计数并写报告
- 实现：Adreno query events；Mali occlusion/timestamp endpoint；其他counter能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：query pool slab与report地址稳定；无CPU阻塞读回
- 寿命/同步：NVN report结构/时间单位/availability需精确；多tile重复不得重复累计
- 验收：多tile计数与SYSMEM对照、reset位置、CPU读取完成
- 门禁：Mali pipeline statistics无stock实现，不能返回伪0
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L455)

## nvnCommandBufferBarrier

- 阶段/证据：P0-D2R静态调用集合；static=33；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, int bits)`；candidate-definition
- 使用：游戏显式发布先前GPU访问
- 消费：录制bits；执行在对应domain完成可见性
- 实现：NVN bits规范化为producer/consumer/cache scope；Adreno UCHE/CCU/CP；Mali texture L1/L2/LS
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：精确域合并；只删被先前更强屏障支配的屏障
- 寿命/同步：不能单以同队列或共享BO省略cache maintenance；未知bit门控
- 验收：SSBO→indirect、RT→sample、copy→vertex、纹理别名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L465)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L911)

## nvnCommandBufferWaitSync

- 阶段/证据：P0-D2R静态调用集合；static=4；resolver-nop
- 候选ABI：`void (*)(NVNcommandBuffer*, const NVNsync*)`；candidate-generated-header
- 使用：使后续GPU工作等待指定sync
- 消费：record仅保留sync对象身份/generation；执行等待其ticket
- 实现：NvnOp.WaitSync(syncRef)→Adreno event/kernel dependency；Mali CSF/JM依赖
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：同queue支配且cache可见性充分才折叠
- 寿命/同步：无value/mode/location/flags参数；不能继承D2R nop；sync寿命至完成；v3：Sync ObjectGeneration与每次FenceEpoch分离，wait必须按已确定锁存边界引用对应epoch
- 验收：双queue生产者、同queue旧fence、尚未signal、回收重用
- 门禁：v3 A7：录制后重Fence的epoch锁存需资格，不允许简单读取最新对象ticket
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L466)

## nvnCommandBufferFenceSync

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNsync*, NVNsyncCondition, int)`；candidate-generated-header
- 使用：在GPU命令位置signal sync
- 消费：record sync/condition/flags；执行到指定condition后发布
- 实现：NvnOp.FenceSync(syncRef,condition,flags)→native event/timeline及cache范围
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：仅与等价位置/condition的提交尾fence合并
- 寿命/同步：保留signal位置；不能偷换成CPU录制完成或整个queue finish；v3：Sync ObjectGeneration与每次FenceEpoch分离，wait必须按已确定锁存边界引用对应epoch
- 验收：不同condition、CPUwait、跨queuewait、pass中fence
- 门禁：v3 A7：录制后重Fence的epoch锁存需资格，不允许简单读取最新对象ticket
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L467)

## nvnCommandBufferPushDebugGroupStatic

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t, const char*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferPushDebugGroupStatic对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core诊断/interop接口；CommandBufferPushDebugGroupStatic对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；CommandBufferPushDebugGroupStatic参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L484)

## nvnCommandBufferPushDebugGroupDynamic

- 阶段/证据：P0-D2R静态调用集合；static=2；resolver-nop
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t, const char*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferPushDebugGroupDynamic对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core诊断/interop接口；CommandBufferPushDebugGroupDynamic对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；CommandBufferPushDebugGroupDynamic参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L485)

## nvnCommandBufferPushDebugGroup

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t, const char*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferPushDebugGroup对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core诊断/interop接口；CommandBufferPushDebugGroup对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；CommandBufferPushDebugGroup参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L486)

## nvnCommandBufferPopDebugGroup

- 阶段/证据：P0-D2R静态调用集合；static=2；resolver-nop
- 候选ABI：`void (*)(NVNcommandBuffer*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferPopDebugGroup对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core诊断/interop接口；CommandBufferPopDebugGroup对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；CommandBufferPopDebugGroup参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L487)

## nvnCommandBufferPopDebugGroupId

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferPopDebugGroupId对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core诊断/interop接口；CommandBufferPopDebugGroupId对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；CommandBufferPopDebugGroupId参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L488)

## nvnCommandBufferInsertDebugMarkerStatic

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t, const char*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferInsertDebugMarkerStatic对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core诊断/interop接口；CommandBufferInsertDebugMarkerStatic对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；CommandBufferInsertDebugMarkerStatic参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L489)

## nvnCommandBufferInsertDebugMarkerDynamic

- 阶段/证据：P0-D2R静态调用集合；static=2；resolver-nop
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t, const char*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferInsertDebugMarkerDynamic对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core诊断/interop接口；CommandBufferInsertDebugMarkerDynamic对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；CommandBufferInsertDebugMarkerDynamic参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L490)

## nvnCommandBufferInsertDebugMarker

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, const char*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferInsertDebugMarker对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core诊断/interop接口；CommandBufferInsertDebugMarker对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；CommandBufferInsertDebugMarker参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L491)

## nvnSyncInitialize

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`NVNboolean (NVNsync *s, NVNdevice *d)`；candidate-definition
- 使用：构造Sync对象
- 消费：host执行；保存传入device身份；初始化空对象和generation，不存在builder参数
- 实现：BackendOps.cache_domain_ops / event / query / queue；保存传入device身份；初始化空对象和generation，不存在builder参数
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L495)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L588)

## nvnSyncFinalize

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNsync *s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；SyncFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.cache_domain_ops / event / query / queue；SyncFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：signal位置/flags保留；CPU可见点先GPU完成再cachemaintenance；查询报告保持NVN字节ABI
- 验收：T-SYNC: 双队列、CPU/GPU事件SET/ADD、masked wait、query可用性、同tile/跨tilebarrier；SyncFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L496)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L589)

## nvnSyncSetDebugLabel

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNsync*, const char*)`；candidate-generated-header
- 使用：设置对象调试名称
- 消费：host复制label字符串/保存debug对象元数据；不录制GPU state
- 实现：core debug_label(object,label)；可选给host调试层标记
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：关闭debug时不格式化、不发GPU包
- 寿命/同步：字符串不得悬空；对象身份与label分别管理
- 验收：调用后修改/释放字符串、NULLlabel、在途对象改名
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L497)

## nvnQueueFenceSync

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNqueue *q, NVNsync *s, NVNsyncCondition c, int f)`；candidate-definition
- 使用：在queue当前位置产生fence
- 消费：host将fence操作排序到pending queue；GPU到达条件signal
- 实现：timeline ticket+指定condition/cache domain
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：与同一submit尾fence合并仅当condition等价
- 寿命/同步：sync对象保存generation/ticket；早于fence的工作可见范围依据flags；v3：Sync ObjectGeneration与每次FenceEpoch分离，wait必须按已确定锁存边界引用对应epoch
- 验收：不同condition、跨queue等待、CPUwait与queueflush组合
- 门禁：v3 A7：录制后重Fence的epoch锁存需资格，不允许简单读取最新对象ticket
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L498)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L592)

## nvnSyncWait

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`NVNsyncWaitResult (const NVNsync *s, uint64_t timeout)`；candidate-definition
- 使用：CPU等待sync，可能轮询或有限超时
- 消费：host读取sync ticket并等待指定deadline
- 实现：kernel syncobj/native fence wait并做host可见维护
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：timeout0直接查询；不唤醒整翻译线程
- 寿命/同步：返回already/signaled/timeout/failed区分；纳秒溢出与无限值按ABI
- 验收：0超时、有限/无限、未signal、device lost
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L499)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L608)

## nvnQueueWaitSync

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(NVNqueue*, const NVNsync*)`；candidate-generated-header
- 使用：在队列后续工作之前插入sync依赖
- 消费：host将依赖加入queue pending graph；GPU执行等待，不必CPU阻塞
- 实现：queue_wait(ticket,scope)→kernel提交依赖/CSF wait/Adreno fence
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：已在同queue支配且可见性满足才消除
- 寿命/同步：不得等待未来自身导致死锁；sync generation保留；v3：Sync ObjectGeneration与每次FenceEpoch分离，wait必须按已确定锁存边界引用对应epoch
- 验收：双queue、未来signal、samequeue支配、CPU不阻塞
- 门禁：v3 A7：录制后重Fence的epoch锁存需资格，不允许简单读取最新对象ticket
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L500)

## nvnSyncInitializeFromFencedGLSync

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：导入外部GL fence
- 消费：host验证GL handle归属并导入同步依赖
- 实现：外部native fence/syncobj桥；不存在合法桥则拒绝
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：不CPU busywait模拟外部同步
- 寿命/同步：GL对象及native fence所有权独立；导入时retain
- 验收：有效/无效handle、GL→NVN跨API、销毁顺序
- 门禁：平台GL互操作可选，不默认声称支持
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L501)

## nvnSyncCreateGLSync

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；SyncCreateGLSync对象操作
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.cache_domain_ops / event / query / queue；SyncCreateGLSync对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：signal位置/flags保留；CPU可见点先GPU完成再cachemaintenance；查询报告保持NVN字节ABI
- 验收：T-SYNC: 双队列、CPU/GPU事件SET/ADD、masked wait、query可用性、同tile/跨tilebarrier；SyncCreateGLSync参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L502)

## nvnEventBuilderSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNeventBuilder *b)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：memset(b, 0, sizeof(ev_builder_t));
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：signal位置/flags保留；CPU可见点先GPU完成再cachemaintenance；查询报告保持NVN字节ABI
- 验收：T-SYNC: 双队列、CPU/GPU事件SET/ADD、masked wait、query可用性、同tile/跨tilebarrier；EventBuilderSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L503)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L633)

## nvnEventBuilderSetStorage

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNeventBuilder *b, const NVNmemoryPool *p, int64_t off)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存pool/offset/size或host指针；检查溢出、对齐和pool容量；不复制资源字节
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.cache_domain_ops / event / query / queue；保存pool/offset/size或host指针；检查溢出、对齐和pool容量；不复制资源字节
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Storage后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：signal位置/flags保留；CPU可见点先GPU完成再cachemaintenance；查询报告保持NVN字节ABI
- 验收：T-SYNC: 双队列、CPU/GPU事件SET/ADD、masked wait、query可用性、同tile/跨tilebarrier；EventBuilderSetStorage参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L504)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L634)

## nvnEventBuilderGetStorage

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；保存pool/offset/size或host指针；检查溢出、对齐和pool容量；不复制资源字节
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.cache_domain_ops / event / query / queue；保存pool/offset/size或host指针；检查溢出、对齐和pool容量；不复制资源字节
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：规范化Storage后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：signal位置/flags保留；CPU可见点先GPU完成再cachemaintenance；查询报告保持NVN字节ABI
- 验收：T-SYNC: 双队列、CPU/GPU事件SET/ADD、masked wait、query可用性、同tile/跨tilebarrier；EventBuilderGetStorage参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L505)

## nvnEventBuilderGetMemoryPool

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；返回对象原来的guest pool身份，不泄露host BO
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.cache_domain_ops / event / query / queue；返回对象原来的guest pool身份，不泄露host BO
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：规范化MemoryPool后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：signal位置/flags保留；CPU可见点先GPU完成再cachemaintenance；查询报告保持NVN字节ABI
- 验收：T-SYNC: 双队列、CPU/GPU事件SET/ADD、masked wait、query可用性、同tile/跨tilebarrier；EventBuilderGetMemoryPool参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L506)

## nvnEventBuilderGetMemoryOffset

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；返回相对guest pool的偏移，非host native重排偏移
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.cache_domain_ops / event / query / queue；返回相对guest pool的偏移，非host native重排偏移
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：规范化MemoryOffset后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：signal位置/flags保留；CPU可见点先GPU完成再cachemaintenance；查询报告保持NVN字节ABI
- 验收：T-SYNC: 双队列、CPU/GPU事件SET/ADD、masked wait、query可用性、同tile/跨tilebarrier；EventBuilderGetMemoryOffset参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L507)

## nvnEventInitialize

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`NVNboolean (NVNevent *e, const NVNeventBuilder *b)`；candidate-definition
- 使用：构造Event对象
- 消费：host执行；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- 实现：BackendOps.cache_domain_ops / event / query / queue；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L508)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L635)

## nvnEventFinalize

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNevent *e)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；EventFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.cache_domain_ops / event / query / queue；EventFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：signal位置/flags保留；CPU可见点先GPU完成再cachemaintenance；查询报告保持NVN字节ABI
- 验收：T-SYNC: 双队列、CPU/GPU事件SET/ADD、masked wait、query可用性、同tile/跨tilebarrier；EventFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L509)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L641)

## nvnEventGetValue

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`uint32_t (const NVNevent *e)`；candidate-definition
- 使用：CPU观察事件当前位置值
- 消费：host读取经invalidate的event bytes；不隐式等待无关queue
- 实现：host acquire/cache domain；共享BO或有序读回
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：仅刷新event cacheline
- 寿命/同步：GPU写须按该API可见语义处理；不返回缓存过期影子
- 验收：GPUsignal后读、并发递增、未完成时合法旧值
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L510)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L642)

## nvnEventSignal

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNevent *e, NVNeventSignalMode m, uint32_t v)`；candidate-definition
- 使用：CPU设置或加到内存事件值；旧D2R有初始化使用
- 消费：host按mode更新对应共享event字节并发布
- 实现：host atomic/release+cache publish；GPU sync32/64消费
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：同cacheline事件避免无谓整poolflush
- 寿命/同步：CPU和GPU竞争的原子性/SET/ADD按ABI；不能只改host影子；v3 M10：精确event word走SYNC_POLL/SIGNAL协议，不加入阻塞producer的普通WAR reader；普通邻接数据仍跟踪
- 验收：CPUsignal解除GPUwait、ADD竞争、非coherent路径
- 门禁：v3：CPU/GPU原子和cache scope独立资格；未知stale-raw event word不得lazy fault形成自锁
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L511)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L649)

## nvnEventGetMemoryPool

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；返回对象原来的guest pool身份，不泄露host BO
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.cache_domain_ops / event / query / queue；返回对象原来的guest pool身份，不泄露host BO
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：规范化MemoryPool后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：signal位置/flags保留；CPU可见点先GPU完成再cachemaintenance；查询报告保持NVN字节ABI
- 验收：T-SYNC: 双队列、CPU/GPU事件SET/ADD、masked wait、query可用性、同tile/跨tilebarrier；EventGetMemoryPool参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L512)

## nvnEventGetMemoryOffset

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；返回相对guest pool的偏移，非host native重排偏移
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.cache_domain_ops / event / query / queue；返回相对guest pool的偏移，非host native重排偏移
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：规范化MemoryOffset后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：signal位置/flags保留；CPU可见点先GPU完成再cachemaintenance；查询报告保持NVN字节ABI
- 验收：T-SYNC: 双队列、CPU/GPU事件SET/ADD、masked wait、query可用性、同tile/跨tilebarrier；EventGetMemoryOffset参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L513)

## nvnCommandBufferWaitEvent

- 阶段/证据：P0-D2R静态调用集合；static=4；resolver-nop
- 候选ABI：`void (*)(NVNcommandBuffer*, const NVNevent*, NVNeventWaitMode, uint32_t)`；candidate-generated-header
- 使用：按mode/value等待内存event
- 消费：record eventRef、waitMode、value；GPU执行读取event的uint32字节
- 实现：NvnOp.WaitEvent(eventRef,mode,value)→Adreno mem wait / Mali cs_sync条件或已资格helper
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：已证明先前signal满足条件才折叠；不泛化单queue nop
- 寿命/同步：没有location/flags参数；CPU更新与cache可见性分离；mask/compare规则需oracle；v3 M10：精确event word走SYNC_POLL/SIGNAL协议，不加入阻塞producer的普通WAR reader；普通邻接数据仍跟踪
- 验收：CPU解除等待、GPU先signal、值变化、不同比较mode
- 门禁：waitMode精确比较规则需oracle；v3：CPU/GPU原子和cache scope独立资格；未知stale-raw event word不得lazy fault形成自锁
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L514)

## nvnCommandBufferSignalEvent

- 阶段/证据：P0-D2R静态调用集合；static=8；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNevent *e, NVNeventSignalMode m, NVNeventSignalLocation loc, int flags, uint32_t v)`；candidate-definition
- 使用：显式SignalEvent；旧D2R同队列nop不能继承
- 消费：录制对象身份及value/mode/location/flags；执行消费
- 实现：Adreno event/write/wait+cache；Mali CSF sync32/64/JM依赖作业
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_cmd_buffer域flush+CP_EVENT_WRITE/WAIT/MEM操作；tu_query_pool计数
- 优化：只有支配关系+域可见性证明成立才折叠
- 寿命/同步：保留CPU event竞争、跨queue、signal位置；有等待不等于自动cacheflush；v3 M10：精确event word走SYNC_POLL/SIGNAL协议，不加入阻塞producer的普通WAR reader；普通邻接数据仍跟踪
- 验收：CPU→GPU、GPU→CPU、双queue、masked/value变化、超时
- 门禁：event flags/比较模式的重建ABI须逐枚举验证；v3：CPU/GPU原子和cache scope独立资格；未知stale-raw event word不得lazy fault形成自锁
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L515)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L780)

## nvnCommandBufferInsertCheckpointStatic

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：录制渲染状态/资源绑定；CommandBufferInsertCheckpointStatic对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core诊断/interop接口；CommandBufferInsertCheckpointStatic对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；CommandBufferInsertCheckpointStatic参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L517)

## nvnCommandBufferInsertCheckpointDynamic

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：录制渲染状态/资源绑定；CommandBufferInsertCheckpointDynamic对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core诊断/interop接口；CommandBufferInsertCheckpointDynamic对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；CommandBufferInsertCheckpointDynamic参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L518)

## nvnDeviceSetAutomaticCheckpointsEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；AutomaticCheckpointsEnable字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；AutomaticCheckpointsEnable字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化AutomaticCheckpointsEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；DeviceSetAutomaticCheckpointsEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L519)

## nvnDeviceSetAutomaticCheckpointsSamplingInterval

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；AutomaticCheckpointsSamplingInterval字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；AutomaticCheckpointsSamplingInterval字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化AutomaticCheckpointsSamplingInterval后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；DeviceSetAutomaticCheckpointsSamplingInterval参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L520)

## nvnDeviceSetAutomaticCheckpointsByDebugGroup

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；AutomaticCheckpointsByDebugGroup字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；AutomaticCheckpointsByDebugGroup字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化AutomaticCheckpointsByDebugGroup后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；DeviceSetAutomaticCheckpointsByDebugGroup参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L521)

## nvnDeviceSetCheckpointCallStacksEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；CheckpointCallStacksEnable字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；CheckpointCallStacksEnable字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化CheckpointCallStacksEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；DeviceSetCheckpointCallStacksEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L522)

## nvnDeviceSetCheckpointMaxCallStacksDepth

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；CheckpointMaxCallStacksDepth字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core诊断/interop接口；CheckpointMaxCallStacksDepth字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：命令标签/RMV/trace可选；GL互操作需显式外部同步与对象桥
- 优化：规范化CheckpointMaxCallStacksDepth后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：debug字符串按API生命周期复制；互操作不是无条件空函数
- 验收：T-DEBUG: 生命周期/重入/回调ABI；不支持interop明确拒绝而非假成功；DeviceSetCheckpointMaxCallStacksDepth参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L523)
