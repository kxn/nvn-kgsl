# v3 10-api-04-fixed-state：Turnip调用合同
这是设计覆盖，不是已实现API。v3新增门禁和私有profile以本包normative contracts为准。

## nvnBlendStateSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNblendState *s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：*ST(blend_st, s) = (blend_st){0, NVN_BLEND_FUNC_ONE, NVN_BLEND_FUNC_ZERO, NVN_BLEND_FUNC_ONE, NVN_BLEND_FUNC_ZERO, NVN_BLEND_EQUATION_ADD, NVN_BLEND_EQUATION_ADD};
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L273)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L942)

## nvnBlendStateSetBlendTarget

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNblendState *s, int t)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；目标attachment index，独立MRT状态
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；目标attachment index，独立MRT状态
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化BlendTarget后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateSetBlendTarget参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L274)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L943)

## nvnBlendStateSetBlendFunc

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNblendState *s, NVNblendFunc sr, NVNblendFunc dr, NVNblendFunc sa, NVNblendFunc da)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；RGB/alpha的src/dst factor分别规范化
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；RGB/alpha的src/dst factor分别规范化
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化BlendFunc后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateSetBlendFunc参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L275)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L944)

## nvnBlendStateSetBlendEquation

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNblendState *s, NVNblendEquation er, NVNblendEquation ea)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；RGB/alpha equation分别规范化
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；RGB/alpha equation分别规范化
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化BlendEquation后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateSetBlendEquation参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L276)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L947)

## nvnBlendStateSetAdvancedMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNblendState*, NVNblendAdvancedMode)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；advanced blend方程；需framebuffer read或blend shader，不能以普通blend近似
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；advanced blend方程；需framebuffer read或blend shader，不能以普通blend近似
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化AdvancedMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateSetAdvancedMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L277)

## nvnBlendStateSetAdvancedOverlap

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNblendState*, NVNblendAdvancedOverlap)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；overlap控制影响片元有序性，决定tile-local interlock/串行化
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；overlap控制影响片元有序性，决定tile-local interlock/串行化
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化AdvancedOverlap后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateSetAdvancedOverlap参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L278)

## nvnBlendStateSetAdvancedPremultipliedSrc

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNblendState*, uint32_t)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；输入premultiply假设，shader选择/方程不能丢弃
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；输入premultiply假设，shader选择/方程不能丢弃
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化AdvancedPremultipliedSrc后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateSetAdvancedPremultipliedSrc参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L279)

## nvnBlendStateSetAdvancedNormalizedDst

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNblendState*, uint32_t)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；dst normalized语义并入blend variant
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；dst normalized语义并入blend variant
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化AdvancedNormalizedDst后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateSetAdvancedNormalizedDst参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L280)

## nvnBlendStateGetBlendTarget

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNblendState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；目标attachment index，独立MRT状态
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；目标attachment index，独立MRT状态
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化BlendTarget后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateGetBlendTarget参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L281)

## nvnBlendStateGetBlendFunc

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNblendState*, NVNblendFunc*, NVNblendFunc*, NVNblendFunc*, NVNblendFunc*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；RGB/alpha的src/dst factor分别规范化
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；RGB/alpha的src/dst factor分别规范化
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化BlendFunc后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateGetBlendFunc参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L282)

## nvnBlendStateGetBlendEquation

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNblendState*, NVNblendEquation*, NVNblendEquation*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；RGB/alpha equation分别规范化
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；RGB/alpha equation分别规范化
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化BlendEquation后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateGetBlendEquation参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L283)

## nvnBlendStateGetAdvancedMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNblendAdvancedMode (*)(const NVNblendState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；advanced blend方程；需framebuffer read或blend shader，不能以普通blend近似
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；advanced blend方程；需framebuffer read或blend shader，不能以普通blend近似
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化AdvancedMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateGetAdvancedMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L284)

## nvnBlendStateGetAdvancedOverlap

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNblendAdvancedOverlap (*)(const NVNblendState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；overlap控制影响片元有序性，决定tile-local interlock/串行化
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；overlap控制影响片元有序性，决定tile-local interlock/串行化
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化AdvancedOverlap后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateGetAdvancedOverlap参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L285)

## nvnBlendStateGetAdvancedPremultipliedSrc

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNblendState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；输入premultiply假设，shader选择/方程不能丢弃
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；输入premultiply假设，shader选择/方程不能丢弃
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化AdvancedPremultipliedSrc后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateGetAdvancedPremultipliedSrc参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L286)

## nvnBlendStateGetAdvancedNormalizedDst

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNblendState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；dst normalized语义并入blend variant
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；dst normalized语义并入blend variant
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化AdvancedNormalizedDst后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；BlendStateGetAdvancedNormalizedDst参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L287)

## nvnColorStateSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcolorState *s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：*ST(color_st, s) = (color_st){0, NVN_LOGIC_OP_COPY, NVN_ALPHA_FUNC_ALWAYS};
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；ColorStateSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L288)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L948)

## nvnColorStateSetBlendEnable

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcolorState *s, int i, uint32_t en)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按目标enable bitmask，未改变目标不可覆盖
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；按目标enable bitmask，未改变目标不可覆盖；guest NVNboolean为8位，当前shim用uint32接参并B(x)=x&0xff，按低8位规范化后转bool，非把上层寄存器垃圾当真
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化BlendEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；ColorStateSetBlendEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L289)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L949)

## nvnColorStateSetLogicOp

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcolorState*, NVNlogicOp)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；整数logic op与格式/混合互斥条件；优先ROP硬件
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；整数logic op与格式/混合互斥条件；优先ROP硬件
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化LogicOp后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；ColorStateSetLogicOp参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L290)

## nvnColorStateSetAlphaTest

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcolorState*, NVNalphaFunc)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；固定alpha compare移至fragment shader discard，保留early/late Z影响
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；固定alpha compare移至fragment shader discard，保留early/late Z影响
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化AlphaTest后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；ColorStateSetAlphaTest参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L291)

## nvnColorStateGetBlendEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNcolorState*, int)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；按目标enable bitmask，未改变目标不可覆盖
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；按目标enable bitmask，未改变目标不可覆盖
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化BlendEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；ColorStateGetBlendEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L292)

## nvnColorStateGetLogicOp

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNlogicOp (*)(const NVNcolorState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；整数logic op与格式/混合互斥条件；优先ROP硬件
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；整数logic op与格式/混合互斥条件；优先ROP硬件
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化LogicOp后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；ColorStateGetLogicOp参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L293)

## nvnColorStateGetAlphaTest

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNalphaFunc (*)(const NVNcolorState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；固定alpha compare移至fragment shader discard，保留early/late Z影响
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；固定alpha compare移至fragment shader discard，保留early/late Z影响
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化AlphaTest后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；ColorStateGetAlphaTest参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L294)

## nvnChannelMaskStateSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNchannelMaskState *s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：ST(chmask_st, s)->mask = 0xffffffffu;
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；ChannelMaskStateSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L295)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L953)

## nvnChannelMaskStateSetChannelMask

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNchannelMaskState *s, int i, uint32_t r, uint32_t g, uint32_t b, uint32_t a)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；每MRT的RGBA写掩码；清屏仅全mask可折成load-clear
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；每MRT的RGBA写掩码；清屏仅全mask可折成load-clear；guest NVNboolean为8位，当前shim用uint32接参并B(x)=x&0xff，按低8位规范化后转bool，非把上层寄存器垃圾当真
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化ChannelMask后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；ChannelMaskStateSetChannelMask参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L296)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L954)

## nvnChannelMaskStateGetChannelMask

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNchannelMaskState*, int, NVNboolean*, NVNboolean*, NVNboolean*, NVNboolean*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；每MRT的RGBA写掩码；清屏仅全mask可折成load-clear
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；每MRT的RGBA写掩码；清屏仅全mask可折成load-clear
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化ChannelMask后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；ChannelMaskStateGetChannelMask参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L297)

## nvnMultisampleStateSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNmultisampleState *s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：memset(s, 0, sizeof(ms_st)); ST(ms_st, s)->enable = 1;
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L298)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L959)

## nvnMultisampleStateSetMultisampleEnable

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNmultisampleState *s, uint32_t e)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；raster MSAA enable与attachment samples联合验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；raster MSAA enable与attachment samples联合验证；guest NVNboolean为8位，当前shim用uint32接参并B(x)=x&0xff，按低8位规范化后转bool，非把上层寄存器垃圾当真
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化MultisampleEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateSetMultisampleEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L299)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L960)

## nvnMultisampleStateSetSamples

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNmultisampleState *s, int n)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；采样数独立于raster samples；按后端1/2/4等真能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；采样数独立于raster samples；按后端1/2/4等真能力门控
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Samples后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateSetSamples参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L300)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L961)

## nvnMultisampleStateSetAlphaToCoverageEnable

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNmultisampleState *s, uint32_t e)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；alpha→coverage mask；与samplemask交集
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；alpha→coverage mask；与samplemask交集；guest NVNboolean为8位，当前shim用uint32接参并B(x)=x&0xff，按低8位规范化后转bool，非把上层寄存器垃圾当真
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化AlphaToCoverageEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateSetAlphaToCoverageEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L301)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L962)

## nvnMultisampleStateSetAlphaToCoverageDither

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNmultisampleState*, uint32_t)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；抖动样式不是普通A2C等价；需能力门控或shader/reference验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；抖动样式不是普通A2C等价；需能力门控或shader/reference验证
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化AlphaToCoverageDither后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateSetAlphaToCoverageDither参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L302)

## nvnMultisampleStateGetMultisampleEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNmultisampleState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；raster MSAA enable与attachment samples联合验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；raster MSAA enable与attachment samples联合验证
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化MultisampleEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateGetMultisampleEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L303)

## nvnMultisampleStateGetSamples

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNmultisampleState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；采样数独立于raster samples；按后端1/2/4等真能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；采样数独立于raster samples；按后端1/2/4等真能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化Samples后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateGetSamples参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L304)

## nvnMultisampleStateGetAlphaToCoverageEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNmultisampleState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；alpha→coverage mask；与samplemask交集
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；alpha→coverage mask；与samplemask交集
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化AlphaToCoverageEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateGetAlphaToCoverageEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L305)

## nvnMultisampleStateGetAlphaToCoverageDither

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNmultisampleState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；抖动样式不是普通A2C等价；需能力门控或shader/reference验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；抖动样式不是普通A2C等价；需能力门控或shader/reference验证
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化AlphaToCoverageDither后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateGetAlphaToCoverageDither参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L306)

## nvnMultisampleStateSetRasterSamples

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNmultisampleState*, int)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；mixed sample/coverage采样数；不支持时明确gated
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；mixed sample/coverage采样数；不支持时明确gated
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化RasterSamples后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateSetRasterSamples参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L307)

## nvnMultisampleStateGetRasterSamples

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(NVNmultisampleState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；mixed sample/coverage采样数；不支持时明确gated
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；mixed sample/coverage采样数；不支持时明确gated
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化RasterSamples后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateGetRasterSamples参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L308)

## nvnMultisampleStateSetCoverageModulationMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNmultisampleState*, NVNcoverageModulationMode)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；coverage到颜色调制；需验证fixed/fragment lowering
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；coverage到颜色调制；需验证fixed/fragment lowering
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化CoverageModulationMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateSetCoverageModulationMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L309)

## nvnMultisampleStateGetCoverageModulationMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNcoverageModulationMode (*)(const NVNmultisampleState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；coverage到颜色调制；需验证fixed/fragment lowering
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；coverage到颜色调制；需验证fixed/fragment lowering
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化CoverageModulationMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateGetCoverageModulationMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L310)

## nvnMultisampleStateSetCoverageToColorEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNmultisampleState*, uint32_t)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；coverage写颜色功能门控，不是普通MRT写
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；coverage写颜色功能门控，不是普通MRT写
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化CoverageToColorEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateSetCoverageToColorEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L311)

## nvnMultisampleStateGetCoverageToColorEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNmultisampleState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；coverage写颜色功能门控，不是普通MRT写
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；coverage写颜色功能门控，不是普通MRT写
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化CoverageToColorEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateGetCoverageToColorEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L312)

## nvnMultisampleStateSetCoverageToColorOutput

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNmultisampleState*, int)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；选择coverage输出MRT，与enable一起验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；选择coverage输出MRT，与enable一起验证
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化CoverageToColorOutput后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateSetCoverageToColorOutput参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L313)

## nvnMultisampleStateGetCoverageToColorOutput

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNmultisampleState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；选择coverage输出MRT，与enable一起验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；选择coverage输出MRT，与enable一起验证
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化CoverageToColorOutput后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateGetCoverageToColorOutput参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L314)

## nvnMultisampleStateSetSampleLocationsEnable

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNmultisampleState *s, uint32_t e)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；自定义采样位置开关
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；自定义采样位置开关；guest NVNboolean为8位，当前shim用uint32接参并B(x)=x&0xff，按低8位规范化后转bool，非把上层寄存器垃圾当真
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化SampleLocationsEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateSetSampleLocationsEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L315)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L963)

## nvnMultisampleStateGetSampleLocationsEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNmultisampleState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；自定义采样位置开关
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；自定义采样位置开关
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化SampleLocationsEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateGetSampleLocationsEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L316)

## nvnMultisampleStateGetSampleLocationsGrid

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNmultisampleState*, int*, int*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回硬件/实现的合法采样位置网格；非NVN原尺寸猜值
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；返回硬件/实现的合法采样位置网格；非NVN原尺寸猜值
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化SampleLocationsGrid后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateGetSampleLocationsGrid参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L317)

## nvnMultisampleStateSetSampleLocationsGridEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNmultisampleState*, uint32_t)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；网格重复语义单独保留
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；网格重复语义单独保留
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化SampleLocationsGridEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateSetSampleLocationsGridEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L318)

## nvnMultisampleStateGetSampleLocationsGridEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNmultisampleState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；网格重复语义单独保留
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；网格重复语义单独保留
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化SampleLocationsGridEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；MultisampleStateGetSampleLocationsGridEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L319)

## nvnMultisampleStateSetSampleLocations

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNmultisampleState *s, int start, int n, const float *v)`；candidate-definition
- 使用：设置自定义sample位置子区间
- 消费：host复制v[0:2*n] float到sample[start:start+n]，保留范围外位置；Bind时快照
- 实现：core SampleLocations→Adreno sample位置寄存器 / Mali能力可用的sample pattern编码
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：同量化后pattern重用state块；不支持任意位置时明确门控
- 寿命/同步：现source为空；NVN网格/量化/位置范围需oracle，不用默认pattern替代
- 验收：start非0、部分更新、count边界、MSAA像素oracle
- 门禁：sample grid及量化未完整恢复，后端cap不足拒绝
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L320)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L964)

## nvnPolygonStateSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNpolygonState *s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：*ST(poly_st, s) = (poly_st){NVN_FACE_NONE, NVN_FRONT_FACE_CCW, NVN_POLYGON_MODE_FILL, 0};
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；PolygonStateSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L321)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L965)

## nvnPolygonStateSetCullFace

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNpolygonState *s, NVNface f)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；front/back/both/none
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；front/back/both/none
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化CullFace后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；PolygonStateSetCullFace参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L322)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L966)

## nvnPolygonStateSetFrontFace

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNpolygonState *s, NVNfrontFace f)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；CW/CCW还受origin/viewport符号变换影响
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；CW/CCW还受origin/viewport符号变换影响
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化FrontFace后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；PolygonStateSetFrontFace参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L323)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L967)

## nvnPolygonStateSetPolygonMode

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNpolygonState *s, NVNpolygonMode m)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；fill/line/point；PanVK无fillModeNonSolid不能直接开放
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；fill/line/point；PanVK无fillModeNonSolid不能直接开放
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化PolygonMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；PolygonStateSetPolygonMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L324)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L968)

## nvnPolygonStateSetPolygonOffsetEnables

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNpolygonState *s, int e)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按primitive mode的offset开关；offset公式需格式/深度约定
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；按primitive mode的offset开关；offset公式需格式/深度约定
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化PolygonOffsetEnables后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；PolygonStateSetPolygonOffsetEnables参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L325)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L969)

## nvnPolygonStateGetCullFace

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNface (*)(const NVNpolygonState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；front/back/both/none
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；front/back/both/none
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化CullFace后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；PolygonStateGetCullFace参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L326)

## nvnPolygonStateGetFrontFace

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNfrontFace (*)(const NVNpolygonState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；CW/CCW还受origin/viewport符号变换影响
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；CW/CCW还受origin/viewport符号变换影响
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化FrontFace后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；PolygonStateGetFrontFace参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L327)

## nvnPolygonStateGetPolygonMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNpolygonMode (*)(const NVNpolygonState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；fill/line/point；PanVK无fillModeNonSolid不能直接开放
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；fill/line/point；PanVK无fillModeNonSolid不能直接开放
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化PolygonMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；PolygonStateGetPolygonMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L328)

## nvnPolygonStateGetPolygonOffsetEnables

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNpolygonOffsetEnable (*)(const NVNpolygonState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；按primitive mode的offset开关；offset公式需格式/深度约定
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；按primitive mode的offset开关；offset公式需格式/深度约定
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化PolygonOffsetEnables后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；PolygonStateGetPolygonOffsetEnables参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L329)

## nvnDepthStencilStateSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdepthStencilState *s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：*ST(ds_st, s) = (ds_st){0, 1, NVN_DEPTH_FUNC_LESS, 0, NVN_STENCIL_FUNC_ALWAYS, NVN_STENCIL_FUNC_ALWAYS, NVN_STENCIL_OP_KEEP, NVN_STENCIL_OP_KEEP, NVN_STENCIL_OP_KEEP, NVN_STENCIL_OP_KEEP, NVN_STENCIL_OP_KEEP, NVN_STENCIL_OP_KEEP};
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L330)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L970)

## nvnDepthStencilStateSetDepthTestEnable

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdepthStencilState *s, uint32_t e)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；深度比较开关
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；深度比较开关；guest NVNboolean为8位，当前shim用uint32接参并B(x)=x&0xff，按低8位规范化后转bool，非把上层寄存器垃圾当真
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化DepthTestEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateSetDepthTestEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L331)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L974)

## nvnDepthStencilStateSetDepthWriteEnable

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdepthStencilState *s, uint32_t e)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；只控制写，不能与depth test混为一项
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；只控制写，不能与depth test混为一项；guest NVNboolean为8位，当前shim用uint32接参并B(x)=x&0xff，按低8位规范化后转bool，非把上层寄存器垃圾当真
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化DepthWriteEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateSetDepthWriteEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L332)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L975)

## nvnDepthStencilStateSetDepthFunc

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdepthStencilState *s, NVNdepthFunc f)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；比较函数含never/always；LRZ方向知识跟随失效
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；比较函数含never/always；LRZ方向知识跟随失效
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化DepthFunc后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateSetDepthFunc参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L333)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L976)

## nvnDepthStencilStateSetStencilTestEnable

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNdepthStencilState *s, uint32_t e)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；stencil比较/操作开关
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；stencil比较/操作开关；guest NVNboolean为8位，当前shim用uint32接参并B(x)=x&0xff，按低8位规范化后转bool，非把上层寄存器垃圾当真
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化StencilTestEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateSetStencilTestEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L334)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L977)

## nvnDepthStencilStateSetStencilFunc

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNdepthStencilState *s, NVNface f, NVNstencilFunc fn)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；front/back函数分别存储
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；front/back函数分别存储
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化StencilFunc后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateSetStencilFunc参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L335)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L978)

## nvnDepthStencilStateSetStencilOp

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNdepthStencilState *s, NVNface f, NVNstencilOp fail, NVNstencilOp zfail, NVNstencilOp zpass)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；front/back fail/depthfail/pass三项分别存储
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；front/back fail/depthfail/pass三项分别存储
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化StencilOp后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateSetStencilOp参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L336)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L981)

## nvnDepthStencilStateGetDepthTestEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNdepthStencilState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；深度比较开关
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；深度比较开关
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化DepthTestEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateGetDepthTestEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L337)

## nvnDepthStencilStateGetDepthWriteEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNdepthStencilState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；只控制写，不能与depth test混为一项
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；只控制写，不能与depth test混为一项
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化DepthWriteEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateGetDepthWriteEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L338)

## nvnDepthStencilStateGetDepthFunc

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNdepthFunc (*)(const NVNdepthStencilState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；比较函数含never/always；LRZ方向知识跟随失效
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；比较函数含never/always；LRZ方向知识跟随失效
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化DepthFunc后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateGetDepthFunc参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L339)

## nvnDepthStencilStateGetStencilTestEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNdepthStencilState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；stencil比较/操作开关
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；stencil比较/操作开关
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化StencilTestEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateGetStencilTestEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L340)

## nvnDepthStencilStateGetStencilFunc

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNstencilFunc (*)(const NVNdepthStencilState*, NVNface)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；front/back函数分别存储
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；front/back函数分别存储
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化StencilFunc后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateGetStencilFunc参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L341)

## nvnDepthStencilStateGetStencilOp

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNdepthStencilState*, NVNface, NVNstencilOp*, NVNstencilOp*, NVNstencilOp*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；front/back fail/depthfail/pass三项分别存储
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；front/back fail/depthfail/pass三项分别存储
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化StencilOp后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；DepthStencilStateGetStencilOp参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L342)

## nvnVertexAttribStateSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNvertexAttribState *s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：*ST(va_st, s) = (va_st){0, 0, 0};
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；VertexAttribStateSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L343)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L986)

## nvnVertexAttribStateSetFormat

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNvertexAttribState *s, NVNformat f, ptrdiff_t off)`；candidate-definition
- 使用：设置顶点属性格式及stream内字节offset
- 消费：host规范化f及ptrdiff off同时更新
- 实现：core VertexAttrib.format/offset→Adreno VFD fetch/Mali attribute descriptor（Bind时）
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：字段变更dirty；属性格式可预解码成size/类型/归一化
- 寿命/同步：现source19bit offset packing不是generic最大值；校验真正caps与NVN ABI范围
- 验收：offset>19bit、packed格式、normalized/integer、stride交互
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L344)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L987)

## nvnVertexAttribStateSetStreamIndex

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNvertexAttribState *s, int i)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；vertex attribute→stream索引
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；vertex attribute→stream索引
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化StreamIndex后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；VertexAttribStateSetStreamIndex参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L345)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L991)

## nvnVertexAttribStateGetFormat

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNvertexAttribState*, NVNformat*, ptrdiff_t*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化Format后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；VertexAttribStateGetFormat参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L346)

## nvnVertexAttribStateGetStreamIndex

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNvertexAttribState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；vertex attribute→stream索引
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；vertex attribute→stream索引
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化StreamIndex后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；VertexAttribStateGetStreamIndex参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L347)

## nvnVertexStreamStateSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNvertexStreamState *s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：*ST(vs_st, s) = (vs_st){0, 0};
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；VertexStreamStateSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L348)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L992)

## nvnVertexStreamStateSetStride

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNvertexStreamState *s, ptrdiff_t st)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；字节行stride或vertex stride按对象类型；溢出/对齐校验，linear texture才可用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；字节行stride或vertex stride按对象类型；溢出/对齐校验，linear texture才可用
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Stride后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；VertexStreamStateSetStride参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L349)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L993)

## nvnVertexStreamStateSetDivisor

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNvertexStreamState *s, int d)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；0逐vertex，非零逐instance且baseinstance规则需测试
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；0逐vertex，非零逐instance且baseinstance规则需测试
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Divisor后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；VertexStreamStateSetDivisor参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L350)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L994)

## nvnVertexStreamStateGetStride

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`ptrdiff_t (*)(const NVNvertexStreamState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；字节行stride或vertex stride按对象类型；溢出/对齐校验，linear texture才可用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；字节行stride或vertex stride按对象类型；溢出/对齐校验，linear texture才可用
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化Stride后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；VertexStreamStateGetStride参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L351)

## nvnVertexStreamStateGetDivisor

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNvertexStreamState*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；0逐vertex，非零逐instance且baseinstance规则需测试
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；0逐vertex，非零逐instance且baseinstance规则需测试
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化Divisor后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；VertexStreamStateGetDivisor参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L352)

## nvnCommandBufferBindBlendState

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNblendState *st)`；candidate-definition
- 使用：将BlendState绑定到后续draw
- 消费：record复制st对象所有有效字段，不保留可变st指针
- 实现：NvnOp.BindBlendState(StateVersion)→state_patch；Adreno寄存器块/Mali DCD/RSD字段
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：按字段dirtymask并intern StateVersion；Bind时不解析纹理/Buffer
- 寿命/同步：record后修改st不能改变已录制状态；draw仍需当前资源依赖
- 验收：bind后修改源st、重复bind、数组长度0/边界与多stream
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L371)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L847)

## nvnCommandBufferBindChannelMaskState

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNchannelMaskState *st)`；candidate-definition
- 使用：将ChannelMaskState绑定到后续draw
- 消费：record复制st对象所有有效字段，不保留可变st指针
- 实现：NvnOp.BindChannelMaskState(StateVersion)→state_patch；Adreno寄存器块/Mali DCD/RSD字段
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：按字段dirtymask并intern StateVersion；Bind时不解析纹理/Buffer
- 寿命/同步：record后修改st不能改变已录制状态；draw仍需当前资源依赖
- 验收：bind后修改源st、重复bind、数组长度0/边界与多stream
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L372)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L849)

## nvnCommandBufferBindColorState

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNcolorState *st)`；candidate-definition
- 使用：将ColorState绑定到后续draw
- 消费：record复制st对象所有有效字段，不保留可变st指针
- 实现：NvnOp.BindColorState(StateVersion)→state_patch；Adreno寄存器块/Mali DCD/RSD字段
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：按字段dirtymask并intern StateVersion；Bind时不解析纹理/Buffer
- 寿命/同步：record后修改st不能改变已录制状态；draw仍需当前资源依赖
- 验收：bind后修改源st、重复bind、数组长度0/边界与多stream
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L373)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L848)

## nvnCommandBufferBindMultisampleState

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNmultisampleState *st)`；candidate-definition
- 使用：将MultisampleState绑定到后续draw
- 消费：record复制st对象所有有效字段，不保留可变st指针
- 实现：NvnOp.BindMultisampleState(StateVersion)→state_patch；Adreno寄存器块/Mali DCD/RSD字段
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：按字段dirtymask并intern StateVersion；Bind时不解析纹理/Buffer
- 寿命/同步：record后修改st不能改变已录制状态；draw仍需当前资源依赖
- 验收：bind后修改源st、重复bind、数组长度0/边界与多stream
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L374)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L850)

## nvnCommandBufferBindPolygonState

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNpolygonState *st)`；candidate-definition
- 使用：将PolygonState绑定到后续draw
- 消费：record复制st对象所有有效字段，不保留可变st指针
- 实现：NvnOp.BindPolygonState(StateVersion)→state_patch；Adreno寄存器块/Mali DCD/RSD字段
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：按字段dirtymask并intern StateVersion；Bind时不解析纹理/Buffer
- 寿命/同步：record后修改st不能改变已录制状态；draw仍需当前资源依赖
- 验收：bind后修改源st、重复bind、数组长度0/边界与多stream
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L375)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L851)

## nvnCommandBufferBindDepthStencilState

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNdepthStencilState *st)`；candidate-definition
- 使用：将DepthStencilState绑定到后续draw
- 消费：record复制st对象所有有效字段，不保留可变st指针
- 实现：NvnOp.BindDepthStencilState(StateVersion)→state_patch；Adreno寄存器块/Mali DCD/RSD字段
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：按字段dirtymask并intern StateVersion；Bind时不解析纹理/Buffer
- 寿命/同步：record后修改st不能改变已录制状态；draw仍需当前资源依赖
- 验收：bind后修改源st、重复bind、数组长度0/边界与多stream
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L376)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L852)

## nvnCommandBufferBindVertexAttribState

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, int n, const NVNvertexAttribState *st)`；candidate-definition
- 使用：将VertexAttribState绑定到后续draw
- 消费：record复制st[0:n]每项规范化字段，不保留可变st指针
- 实现：NvnOp.BindVertexAttribState(StateVersion,count)→state_patch；Adreno寄存器块/Mali DCD/RSD字段
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：按字段dirtymask并intern StateVersion；Bind时不解析纹理/Buffer
- 寿命/同步：record后修改st不能改变已录制状态；draw仍需当前资源依赖
- 验收：bind后修改源st、重复bind、数组长度0/边界与多stream
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L377)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L853)

## nvnCommandBufferBindVertexStreamState

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, int n, const NVNvertexStreamState *st)`；candidate-definition
- 使用：将VertexStreamState绑定到后续draw
- 消费：record复制st[0:n]每项规范化字段，不保留可变st指针
- 实现：NvnOp.BindVertexStreamState(StateVersion,count)→state_patch；Adreno寄存器块/Mali DCD/RSD字段
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：按字段dirtymask并intern StateVersion；Bind时不解析纹理/Buffer
- 寿命/同步：record后修改st不能改变已录制状态；draw仍需当前资源依赖
- 验收：bind后修改源st、重复bind、数组长度0/边界与多stream
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L378)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L857)

## nvnCommandBufferBindProgram

- 阶段/证据：P0-D2R静态调用集合；static=4；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const NVNprogram *p, int stages)`；candidate-definition
- 使用：按stages mask更新绑定程序
- 消费：record ProgramVersion及stages mask；编译代码版本固定
- 实现：NvnOp.BindProgram(version,stageMask)→ProgramVariant ir3/panfrost state块
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：ProgramVariant按link interface缓存；未变stage不重发
- 寿命/同步：清除/保留未选stage遵循mask语义；program原对象Finalize不释放在途代码
- 验收：subset stages、setshaders后旧命令、stage link mismatch
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L379)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L843)

## nvnCommandBufferBindVertexBuffer

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, NVNbufferAddress, size_t)`；candidate-generated-header
- 使用：绑定Vertex buffer范围供后续draw/dispatch
- 消费：record 复制address=a、size=n到slot=i；exec读取/写入对应buffer内容
- 实现：NvnOp.BindBufferRange(kind=Vertex, stage, first, ranges[])→guestVA区间解析→ResourceId+offset→nativeVA；顶点无shaderstage
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：缓存(addr,size,mapGeneration)解析；UBO/SSBO根表只patch变化slot，不按内容改写重建描述符
- 寿命/同步：address映射代变化失效解析；内容epoch仅更新hazard；XFB在Mali能力门控
- 验收：跨pool范围、sameaddr重映射、CPU动态内容、GPU写后shader读
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L380)

## nvnCommandBufferBindVertexBuffers

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, int first, int n, const NVNbufferRange *rg)`；candidate-definition
- 使用：绑定Vertex buffer范围供后续draw/dispatch
- 消费：record 复制rg[0:n]的address/size到slot[first:first+n]；exec读取/写入对应buffer内容
- 实现：NvnOp.BindBufferRange(kind=Vertex, stage, first, ranges[])→guestVA区间解析→ResourceId+offset→nativeVA；顶点无shaderstage
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：缓存(addr,size,mapGeneration)解析；UBO/SSBO根表只patch变化slot，不按内容改写重建描述符
- 寿命/同步：address映射代变化失效解析；内容epoch仅更新hazard；XFB在Mali能力门控
- 验收：跨pool范围、sameaddr重映射、CPU动态内容、GPU写后shader读
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L381)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L861)

## nvnCommandBufferBindUniformBuffer

- 阶段/证据：P0-D2R静态调用集合；static=9；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNshaderStage st, int i, NVNbufferAddress a, size_t n)`；candidate-definition
- 使用：绑定Uniform buffer范围供后续draw/dispatch
- 消费：record 复制address=a、size=n到slot=i；exec读取/写入对应buffer内容
- 实现：NvnOp.BindBufferRange(kind=Uniform, stage, first, ranges[])→guestVA区间解析→ResourceId+offset→nativeVA；顶点无shaderstage
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：缓存(addr,size,mapGeneration)解析；UBO/SSBO根表只patch变化slot，不按内容改写重建描述符
- 寿命/同步：address映射代变化失效解析；内容epoch仅更新hazard；XFB在Mali能力门控
- 验收：跨pool范围、sameaddr重映射、CPU动态内容、GPU写后shader读
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L382)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L865)

## nvnCommandBufferBindUniformBuffers

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNshaderStage st, int first, int n, const NVNbufferRange *rg)`；candidate-definition
- 使用：绑定Uniform buffer范围供后续draw/dispatch
- 消费：record 复制rg[0:n]的address/size到slot[first:first+n]；exec读取/写入对应buffer内容
- 实现：NvnOp.BindBufferRange(kind=Uniform, stage, first, ranges[])→guestVA区间解析→ResourceId+offset→nativeVA；顶点无shaderstage
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：缓存(addr,size,mapGeneration)解析；UBO/SSBO根表只patch变化slot，不按内容改写重建描述符
- 寿命/同步：address映射代变化失效解析；内容epoch仅更新hazard；XFB在Mali能力门控
- 验收：跨pool范围、sameaddr重映射、CPU动态内容、GPU写后shader读
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L383)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L868)

## nvnCommandBufferBindTransformFeedbackBuffer

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, NVNbufferAddress, size_t)`；candidate-generated-header
- 使用：绑定TransformFeedback buffer范围供后续draw/dispatch
- 消费：record 复制address=a、size=n到slot=i；exec读取/写入对应buffer内容
- 实现：NvnOp.BindBufferRange(kind=TransformFeedback, stage, first, ranges[])→guestVA区间解析→ResourceId+offset→nativeVA；顶点无shaderstage
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：缓存(addr,size,mapGeneration)解析；UBO/SSBO根表只patch变化slot，不按内容改写重建描述符
- 寿命/同步：address映射代变化失效解析；内容epoch仅更新hazard；XFB在Mali能力门控
- 验收：跨pool范围、sameaddr重映射、CPU动态内容、GPU写后shader读
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L384)

## nvnCommandBufferBindTransformFeedbackBuffers

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, int, const NVNbufferRange*)`；candidate-generated-header
- 使用：绑定TransformFeedback buffer范围供后续draw/dispatch
- 消费：record 复制rg[0:n]的address/size到slot[first:first+n]；exec读取/写入对应buffer内容
- 实现：NvnOp.BindBufferRange(kind=TransformFeedback, stage, first, ranges[])→guestVA区间解析→ResourceId+offset→nativeVA；顶点无shaderstage
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：缓存(addr,size,mapGeneration)解析；UBO/SSBO根表只patch变化slot，不按内容改写重建描述符
- 寿命/同步：address映射代变化失效解析；内容epoch仅更新hazard；XFB在Mali能力门控
- 验收：跨pool范围、sameaddr重映射、CPU动态内容、GPU写后shader读
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L385)

## nvnCommandBufferBindStorageBuffer

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNshaderStage st, int i, NVNbufferAddress a, size_t n)`；candidate-definition
- 使用：绑定Storage buffer范围供后续draw/dispatch
- 消费：record 复制address=a、size=n到slot=i；exec读取/写入对应buffer内容
- 实现：NvnOp.BindBufferRange(kind=Storage, stage, first, ranges[])→guestVA区间解析→ResourceId+offset→nativeVA；顶点无shaderstage
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：缓存(addr,size,mapGeneration)解析；UBO/SSBO根表只patch变化slot，不按内容改写重建描述符
- 寿命/同步：address映射代变化失效解析；内容epoch仅更新hazard；XFB在Mali能力门控
- 验收：跨pool范围、sameaddr重映射、CPU动态内容、GPU写后shader读
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L386)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L871)

## nvnCommandBufferBindStorageBuffers

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNshaderStage st, int first, int n, const NVNbufferRange *rg)`；candidate-definition
- 使用：绑定Storage buffer范围供后续draw/dispatch
- 消费：record 复制rg[0:n]的address/size到slot[first:first+n]；exec读取/写入对应buffer内容
- 实现：NvnOp.BindBufferRange(kind=Storage, stage, first, ranges[])→guestVA区间解析→ResourceId+offset→nativeVA；顶点无shaderstage
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：缓存(addr,size,mapGeneration)解析；UBO/SSBO根表只patch变化slot，不按内容改写重建描述符
- 寿命/同步：address映射代变化失效解析；内容epoch仅更新hazard；XFB在Mali能力门控
- 验收：跨pool范围、sameaddr重映射、CPU动态内容、GPU写后shader读
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L387)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L874)

## nvnCommandBufferBindTexture

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNshaderStage, int, NVNtextureHandle)`；candidate-generated-header
- 使用：绑定Texture句柄到shader stage槽位
- 消费：record复制stage/first/count及handle数组或标量；descriptor槽内容按经oracle确认的消费边界选择版本
- 实现：NvnOp.BindHandle(Texture,stage,first,handles[])→shader根slot/handle table；Adreno bindless；CSF table/index
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅修改变化slot；DescriptorVersion已解析则O(1)复用，不重做image查找/布局分配
- 寿命/同步：handle不是对象快照；pool root和slot版本分开；Register后何时可见设资格门禁
- 验收：同slot注册改变、pool切换、record后register、submit后register
- 门禁：descriptor消费时点必须实测，不能一律Register时COW冻结
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L388)

## nvnCommandBufferBindTextures

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNshaderStage, int, int, const NVNtextureHandle*)`；candidate-generated-header
- 使用：绑定Texture句柄到shader stage槽位
- 消费：record复制stage/first/count及handle数组或标量；descriptor槽内容按经oracle确认的消费边界选择版本
- 实现：NvnOp.BindHandle(Texture,stage,first,handles[])→shader根slot/handle table；Adreno bindless；CSF table/index
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅修改变化slot；DescriptorVersion已解析则O(1)复用，不重做image查找/布局分配
- 寿命/同步：handle不是对象快照；pool root和slot版本分开；Register后何时可见设资格门禁
- 验收：同slot注册改变、pool切换、record后register、submit后register
- 门禁：descriptor消费时点必须实测，不能一律Register时COW冻结
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L389)

## nvnCommandBufferBindImage

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNshaderStage, int, NVNimageHandle)`；candidate-generated-header
- 使用：绑定Image句柄到shader stage槽位
- 消费：record复制stage/first/count及handle数组或标量；descriptor槽内容按经oracle确认的消费边界选择版本
- 实现：NvnOp.BindHandle(Image,stage,first,handles[])→shader根slot/handle table；Adreno bindless；CSF table/index
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅修改变化slot；DescriptorVersion已解析则O(1)复用，不重做image查找/布局分配
- 寿命/同步：handle不是对象快照；pool root和slot版本分开；Register后何时可见设资格门禁
- 验收：同slot注册改变、pool切换、record后register、submit后register
- 门禁：descriptor消费时点必须实测，不能一律Register时COW冻结
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L390)

## nvnCommandBufferBindImages

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNshaderStage st, int first, int n, const NVNimageHandle *h)`；candidate-definition
- 使用：绑定Image句柄到shader stage槽位
- 消费：record复制stage/first/count及handle数组或标量；descriptor槽内容按经oracle确认的消费边界选择版本
- 实现：NvnOp.BindHandle(Image,stage,first,handles[])→shader根slot/handle table；Adreno bindless；CSF table/index
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅修改变化slot；DescriptorVersion已解析则O(1)复用，不重做image查找/布局分配
- 寿命/同步：handle不是对象快照；pool root和slot版本分开；Register后何时可见设资格门禁
- 验收：同slot注册改变、pool切换、record后register、submit后register
- 门禁：descriptor消费时点必须实测，不能一律Register时COW冻结
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L391)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L883)

## nvnCommandBufferSetPatchSize

- 阶段/证据：P1-静态可达/历史运行报零；仍需显式能力门控；static=1；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；tess patch控制点数量；Adreno可能强制SYSMEM；Mali能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(PatchSize)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；tess patch控制点数量；Adreno可能强制SYSMEM；Mali能力门控；专用能力：Adreno对应streamout/tess/metadata路径；Mali无stock GS/Tess/XFB完整路径，需独立lowering/资格门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化PatchSize后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetPatchSize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L392)

## nvnCommandBufferSetInnerTessellationLevels

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, const float*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；InnerTessellationLevels字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(InnerTessellationLevels)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；InnerTessellationLevels字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控；专用能力：Adreno对应streamout/tess/metadata路径；Mali无stock GS/Tess/XFB完整路径，需独立lowering/资格门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化InnerTessellationLevels后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetInnerTessellationLevels参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L393)

## nvnCommandBufferSetOuterTessellationLevels

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, const float*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；OuterTessellationLevels字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(OuterTessellationLevels)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；OuterTessellationLevels字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控；专用能力：Adreno对应streamout/tess/metadata路径；Mali无stock GS/Tess/XFB完整路径，需独立lowering/资格门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化OuterTessellationLevels后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetOuterTessellationLevels参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L394)

## nvnCommandBufferSetPrimitiveRestart

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t, int)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；启用和restart index，索引宽度/转换需一致
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(PrimitiveRestart)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；启用和restart index，索引宽度/转换需一致
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化PrimitiveRestart后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetPrimitiveRestart参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L395)

## nvnCommandBufferBeginTransformFeedback

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNbufferAddress)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferBeginTransformFeedback对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；CommandBufferBeginTransformFeedback对象操作；专用能力：Adreno对应streamout/tess/metadata路径；Mali无stock GS/Tess/XFB完整路径，需独立lowering/资格门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferBeginTransformFeedback参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L396)

## nvnCommandBufferEndTransformFeedback

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNbufferAddress)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferEndTransformFeedback对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；CommandBufferEndTransformFeedback对象操作；专用能力：Adreno对应streamout/tess/metadata路径；Mali无stock GS/Tess/XFB完整路径，需独立lowering/资格门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferEndTransformFeedback参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L397)

## nvnCommandBufferPauseTransformFeedback

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNbufferAddress)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferPauseTransformFeedback对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；CommandBufferPauseTransformFeedback对象操作；专用能力：Adreno对应streamout/tess/metadata路径；Mali无stock GS/Tess/XFB完整路径，需独立lowering/资格门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferPauseTransformFeedback参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L398)

## nvnCommandBufferResumeTransformFeedback

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNbufferAddress)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferResumeTransformFeedback对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；CommandBufferResumeTransformFeedback对象操作；专用能力：Adreno对应streamout/tess/metadata路径；Mali无stock GS/Tess/XFB完整路径，需独立lowering/资格门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferResumeTransformFeedback参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L399)

## nvnCommandBufferSetViewport

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, int, int, int)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；Viewport字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(Viewport)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；Viewport字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化Viewport后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetViewport参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L416)

## nvnCommandBufferSetViewports

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, int first, int n, const float *v)`；candidate-definition
- 使用：设置后续操作的Viewports
- 消费：record复制参数；复制v中n组x,y,width,height到[first,first+n)；checked范围；Mali多viewport能力门控
- 实现：NvnOp.SetViewports(复制v中n组x,y,width,height到[first,first+n)；checked范围；Mali多viewport能力门控)→state_patch
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅相应字段dirty；不会因状态改变重做不相干descriptor解析
- 寿命/同步：后续setter不能追溯改变已录制命令；上下文独立；不支持能力明确拒绝
- 验收：默认/边界/重复值、录制后改源、状态组合像素oracle
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L417)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L836)

## nvnCommandBufferSetViewportSwizzles

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, int, const NVNviewportSwizzle*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；逐分量符号和坐标交换，shader builtin变换与raster约定一致
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(ViewportSwizzles)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；逐分量符号和坐标交换，shader builtin变换与raster约定一致
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化ViewportSwizzles后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetViewportSwizzles参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L418)

## nvnCommandBufferSetScissor

- 阶段/证据：P0-D2R静态调用集合；static=4；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, int x, int y, int w, int h)`；candidate-definition
- 使用：设置后续操作的Scissor
- 消费：record复制参数；保存x/y有符号位置和w/h非负尺寸；以checked arithmetic计算右/下边界，按origin约定转换；不能把负x当uint巨值
- 实现：NvnOp.SetScissor(保存x/y有符号位置和w/h非负尺寸；以checked arithmetic计算右/下边界，按origin约定转换；不能把负x当uint巨值)→state_patch
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅相应字段dirty；不会因状态改变重做不相干descriptor解析
- 寿命/同步：后续setter不能追溯改变已录制命令；上下文独立；不支持能力明确拒绝
- 验收：默认/边界/重复值、录制后改源、状态组合像素oracle
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L419)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L841)

## nvnCommandBufferSetScissors

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, int, const int*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；Scissors字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(Scissors)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；Scissors字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化Scissors后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetScissors参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L420)

## nvnCommandBufferSetDepthRange

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, float n, float f)`；candidate-definition
- 使用：设置后续操作的DepthRange
- 消费：record复制参数；保存near/far两个float包括反向区间；按DeviceDepthMode生成viewport scale/bias，LRZ方向随反向区间失效
- 实现：NvnOp.SetDepthRange(保存near/far两个float包括反向区间；按DeviceDepthMode生成viewport scale/bias，LRZ方向随反向区间失效)→state_patch
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅相应字段dirty；不会因状态改变重做不相干descriptor解析
- 寿命/同步：后续setter不能追溯改变已录制命令；上下文独立；不支持能力明确拒绝
- 验收：默认/边界/重复值、录制后改源、状态组合像素oracle
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L421)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L842)

## nvnCommandBufferSetDepthBounds

- 阶段/证据：P0-D2R静态调用集合；static=3；resolver-nop
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t, float, float)`；candidate-generated-header
- 使用：设置后续操作的DepthBounds
- 消费：record复制参数；复制enable/min/max；禁用时不影响测试；Mali stock不支持，不能继承resolver nop
- 实现：NvnOp.SetDepthBounds(复制enable/min/max；禁用时不影响测试；Mali stock不支持，不能继承resolver nop)→state_patch
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅相应字段dirty；不会因状态改变重做不相干descriptor解析
- 寿命/同步：后续setter不能追溯改变已录制命令；上下文独立；不支持能力明确拒绝
- 验收：默认/边界/重复值、录制后改源、状态组合像素oracle
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L422)

## nvnCommandBufferSetDepthRanges

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, int, const float*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；DepthRanges字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(DepthRanges)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；DepthRanges字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化DepthRanges后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetDepthRanges参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L423)

## nvnCommandBufferBindSeparateTexture

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNshaderStage, int, NVNseparateTextureHandle)`；candidate-generated-header
- 使用：绑定SeparateTexture句柄到shader stage槽位
- 消费：record复制stage/first/count及handle数组或标量；descriptor槽内容按经oracle确认的消费边界选择版本
- 实现：NvnOp.BindHandle(SeparateTexture,stage,first,handles[])→shader根slot/handle table；Adreno bindless；CSF table/index
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅修改变化slot；DescriptorVersion已解析则O(1)复用，不重做image查找/布局分配
- 寿命/同步：handle不是对象快照；pool root和slot版本分开；Register后何时可见设资格门禁
- 验收：同slot注册改变、pool切换、record后register、submit后register
- 门禁：descriptor消费时点必须实测，不能一律Register时COW冻结
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L426)

## nvnCommandBufferBindSeparateSampler

- 阶段/证据：P0-D2R静态调用集合；static=4；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNshaderStage st, int i, NVNseparateSamplerHandle h)`；candidate-definition
- 使用：绑定SeparateSampler句柄到shader stage槽位
- 消费：record复制stage/first/count及handle数组或标量；descriptor槽内容按经oracle确认的消费边界选择版本
- 实现：NvnOp.BindHandle(SeparateSampler,stage,first,handles[])→shader根slot/handle table；Adreno bindless；CSF table/index
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅修改变化slot；DescriptorVersion已解析则O(1)复用，不重做image查找/布局分配
- 寿命/同步：handle不是对象快照；pool root和slot版本分开；Register后何时可见设资格门禁
- 验收：同slot注册改变、pool切换、record后register、submit后register
- 门禁：descriptor消费时点必须实测，不能一律Register时COW冻结
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L427)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L892)

## nvnCommandBufferBindSeparateTextures

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNshaderStage st, int first, int n, const NVNseparateTextureHandle *h)`；candidate-definition
- 使用：绑定SeparateTexture句柄到shader stage槽位
- 消费：record复制stage/first/count及handle数组或标量；descriptor槽内容按经oracle确认的消费边界选择版本
- 实现：NvnOp.BindHandle(SeparateTexture,stage,first,handles[])→shader根slot/handle table；Adreno bindless；CSF table/index
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅修改变化slot；DescriptorVersion已解析则O(1)复用，不重做image查找/布局分配
- 寿命/同步：handle不是对象快照；pool root和slot版本分开；Register后何时可见设资格门禁
- 验收：同slot注册改变、pool切换、record后register、submit后register
- 门禁：descriptor消费时点必须实测，不能一律Register时COW冻结
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L428)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L886)

## nvnCommandBufferBindSeparateSamplers

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNshaderStage st, int first, int n, const NVNseparateSamplerHandle *h)`；candidate-definition
- 使用：绑定SeparateSampler句柄到shader stage槽位
- 消费：record复制stage/first/count及handle数组或标量；descriptor槽内容按经oracle确认的消费边界选择版本
- 实现：NvnOp.BindHandle(SeparateSampler,stage,first,handles[])→shader根slot/handle table；Adreno bindless；CSF table/index
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅修改变化slot；DescriptorVersion已解析则O(1)复用，不重做image查找/布局分配
- 寿命/同步：handle不是对象快照；pool root和slot版本分开；Register后何时可见设资格门禁
- 验收：同slot注册改变、pool切换、record后register、submit后register
- 门禁：descriptor消费时点必须实测，不能一律Register时COW冻结
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L429)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L889)

## nvnCommandBufferSetStencilValueMask

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNface f, int m)`；candidate-definition
- 使用：设置后续操作的StencilValueMask
- 消费：record复制参数；按face更新front/back比较read mask；只取该stencil格式有效bits，不与write mask混淆
- 实现：NvnOp.SetStencilValueMask(按face更新front/back比较read mask；只取该stencil格式有效bits，不与write mask混淆)→state_patch
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅相应字段dirty；不会因状态改变重做不相干descriptor解析
- 寿命/同步：后续setter不能追溯改变已录制命令；上下文独立；不支持能力明确拒绝
- 验收：默认/边界/重复值、录制后改源、状态组合像素oracle
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L430)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L895)

## nvnCommandBufferSetStencilMask

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNface f, int m)`；candidate-definition
- 使用：设置后续操作的StencilMask
- 消费：record复制参数；按face更新front/back write mask；depth/stencil清除的mask仍独立
- 实现：NvnOp.SetStencilMask(按face更新front/back write mask；depth/stencil清除的mask仍独立)→state_patch
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅相应字段dirty；不会因状态改变重做不相干descriptor解析
- 寿命/同步：后续setter不能追溯改变已录制命令；上下文独立；不支持能力明确拒绝
- 验收：默认/边界/重复值、录制后改源、状态组合像素oracle
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L431)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L896)

## nvnCommandBufferSetStencilRef

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, NVNface f, int v)`；candidate-definition
- 使用：设置后续操作的StencilRef
- 消费：record复制参数；按face更新front/back reference；比较和值写入所需bit宽按格式规范化
- 实现：NvnOp.SetStencilRef(按face更新front/back reference；比较和值写入所需bit宽按格式规范化)→state_patch
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅相应字段dirty；不会因状态改变重做不相干descriptor解析
- 寿命/同步：后续setter不能追溯改变已录制命令；上下文独立；不支持能力明确拒绝
- 验收：默认/边界/重复值、录制后改源、状态组合像素oracle
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L432)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L897)

## nvnCommandBufferSetBlendColor

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, const float *v)`；candidate-definition
- 使用：设置后续操作的BlendColor
- 消费：record复制参数；立即复制v[0:4] float常量；用于constant color/alpha因子，保留NaN/精度策略
- 实现：NvnOp.SetBlendColor(立即复制v[0:4] float常量；用于constant color/alpha因子，保留NaN/精度策略)→state_patch
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅相应字段dirty；不会因状态改变重做不相干descriptor解析
- 寿命/同步：后续setter不能追溯改变已录制命令；上下文独立；不支持能力明确拒绝
- 验收：默认/边界/重复值、录制后改源、状态组合像素oracle
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L433)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L937)

## nvnCommandBufferSetPointSize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, float)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；PointSize字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(PointSize)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；PointSize字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化PointSize后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetPointSize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L434)

## nvnCommandBufferSetLineWidth

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, float)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；LineWidth字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(LineWidth)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；LineWidth字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化LineWidth后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetLineWidth参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L435)

## nvnCommandBufferSetPolygonOffsetClamp

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNcommandBuffer *c, float fa, float un, float cl)`；candidate-definition
- 使用：设置后续操作的PolygonOffsetClamp
- 消费：record复制参数；复制factor/fa、units/un、clamp/cl；结合深度格式有效精度和clip约定映射bias slope/constant/clamp
- 实现：NvnOp.SetPolygonOffsetClamp(复制factor/fa、units/un、clamp/cl；结合深度格式有效精度和clip约定映射bias slope/constant/clamp)→state_patch
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：仅相应字段dirty；不会因状态改变重做不相干descriptor解析
- 寿命/同步：后续setter不能追溯改变已录制命令；上下文独立；不支持能力明确拒绝
- 验收：默认/边界/重复值、录制后改源、状态组合像素oracle
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L436)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L898)

## nvnCommandBufferSetAlphaRef

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, float)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；AlphaRef字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(AlphaRef)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；AlphaRef字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化AlphaRef后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetAlphaRef参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L437)

## nvnCommandBufferSetSampleMask

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；SampleMask字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(SampleMask)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；SampleMask字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化SampleMask后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetSampleMask参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L438)

## nvnCommandBufferSetRasterizerDiscard

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；可跳过raster不能省略VS/XFB/SSBO sideeffect
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(RasterizerDiscard)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；可跳过raster不能省略VS/XFB/SSBO sideeffect
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化RasterizerDiscard后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetRasterizerDiscard参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L439)

## nvnCommandBufferSetDepthClamp

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；clip/clamp模式要与NVN depth range统一
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(DepthClamp)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；clip/clamp模式要与NVN depth range统一
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化DepthClamp后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetDepthClamp参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L440)

## nvnCommandBufferSetConservativeRasterEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；ConservativeRasterEnable字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(ConservativeRasterEnable)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；ConservativeRasterEnable字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化ConservativeRasterEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetConservativeRasterEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L441)

## nvnCommandBufferSetConservativeRasterDilate

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, float)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；ConservativeRasterDilate字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(ConservativeRasterDilate)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；ConservativeRasterDilate字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化ConservativeRasterDilate后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetConservativeRasterDilate参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L442)

## nvnCommandBufferSetSubpixelPrecisionBias

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, int, int)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；SubpixelPrecisionBias字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(SubpixelPrecisionBias)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；SubpixelPrecisionBias字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化SubpixelPrecisionBias后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetSubpixelPrecisionBias参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L443)

## nvnCommandBufferSetRenderEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, uint32_t)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；布尔条件控制draw，不得影响无关query/transfer
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(RenderEnable)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；布尔条件控制draw，不得影响无关query/transfer
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化RenderEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetRenderEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L456)

## nvnCommandBufferSetRenderEnableConditional

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, NVNconditionalRenderMode, NVNbufferAddress)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；GPU执行读取report地址和compare模式，不得record时CPU读
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(RenderEnableConditional)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；GPU执行读取report地址和compare模式，不得record时CPU读
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化RenderEnableConditional后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetRenderEnableConditional参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L457)

## nvnCommandBufferBindCoverageModulationTable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNcommandBuffer*, const float*)`；candidate-generated-header
- 使用：推断：录制渲染状态/资源绑定；CommandBufferBindCoverageModulationTable对象操作
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR Bind(CoverageModulationTable, stage/index/range)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；数组只复制count项；descriptor/data root与fixed-state分离
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：按CoverageModulationTable的stage/slot范围dirty bit更新；相同绑定重用已解析ResourceId/view/native编码，但保留本draw访问依赖
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferBindCoverageModulationTable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L480)

## nvnCommandBufferSetColorReductionEnable

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：录制渲染状态/资源绑定；颜色压缩hint不改变像素值；安全忽略只限已证非语义hint
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(ColorReductionEnable)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；颜色压缩hint不改变像素值；安全忽略只限已证非语义hint
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化ColorReductionEnable后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetColorReductionEnable参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L482)

## nvnCommandBufferSetColorReductionThresholds

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：录制渲染状态/资源绑定；lossy/阈值语义未确认前禁用优化而非随意量化
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(ColorReductionThresholds)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；lossy/阈值语义未确认前禁用优化而非随意量化
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化ColorReductionThresholds后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetColorReductionThresholds参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L483)

## nvnCommandBufferSetStencilCullCriteria

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：录制渲染状态/资源绑定；early stencil分类hint；不能取代实际stencil test
- 消费：录制时复制标量及count长度数组；资源地址/内容在GPU执行读取，不能整buffer快照
- 实现：IR StatePatch(StencilCullCriteria)；nvn_core状态对象 + BackendOps.state_patch/render_epoch_emit；early stencil分类hint；不能取代实际stencil test
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：tu_pipeline/tu_cmd_buffer对应寄存器状态块；CP_SET_DRAW_STATE
- 优化：规范化StencilCullCriteria后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：录制时固定参数值；绝不因相同state而跳过本draw资源访问；nativecap不足时能力门控
- 验收：T-STATE: setter/getter roundtrip、默认值、录制后改对象、不支持枚举和状态交互；CommandBufferSetStencilCullCriteria参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L516)
