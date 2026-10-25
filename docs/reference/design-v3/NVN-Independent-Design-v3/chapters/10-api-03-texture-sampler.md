# v3 10-api-03-texture-sampler：Turnip调用合同
这是设计覆盖，不是已实现API。v3新增门禁和私有profile以本包normative contracts为准。

## nvnProgramInitialize

- 阶段/证据：P0-D2R静态调用集合；static=7；source-defined
- 候选ABI：`NVNboolean (NVNprogram *p, NVNdevice *d)`；candidate-definition
- 使用：构造Program对象
- 消费：host执行；保存传入device身份；初始化空对象和generation，不存在builder参数
- 实现：BackendOps.shader_compile；保存传入device身份；初始化空对象和generation，不存在builder参数
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：guest Maxwell→IR/NIR→ir3；NVN槽位/handle lowering；可复用shader state块
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L87)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L574)

## nvnProgramFinalize

- 阶段/证据：P0-D2R静态调用集合；static=7；source-defined
- 候选ABI：`void (NVNprogram *p)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；ProgramFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.shader_compile；ProgramFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：guest Maxwell→IR/NIR→ir3；NVN槽位/handle lowering；可复用shader state块
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：程序和编译派生对象均不可变版本；编译期间不能读取已改写guest control/data
- 验收：T-SHADER: stage link、间接资源索引、builtin、subgroup32、精度和cache identity；ProgramFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L88)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L577)

## nvnProgramSetShaders

- 阶段/证据：P0-D2R静态调用集合；static=7；source-defined
- 候选ABI：`NVNboolean (NVNprogram *p, int n, const NVNshaderData *sd)`；candidate-definition
- 使用：加载/切换Maxwell shader集合；现实现serial化
- 消费：host复制shader control元数据与需编译的code版本；后续Bind记录ProgramVersion
- 实现：Maxwell decoder→共同IR/NIR→ir3/panfrost；链接NVN常量/descriptor ABI
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：guest Maxwell→IR/NIR→ir3；NVN槽位/handle lowering；可复用shader state块
- 优化：编译持久缓存key包含shader bytes、ABI、compiler、GPU caps、mathmode
- 寿命/同步：不得异步编译时读取已被guest复用的code；各stage缺失清空旧stage
- 验收：同program对象反复SetShaders、修改code、stage缺失、缓存碰撞
- 门禁：现源码只复制NVNshaderData记录；code/control字节合法消费时点需oracle，新异步编译必须持有相应版本，不能误称源码已深拷贝
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L90)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L578)

## nvnProgramSetShadersExt

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；ShadersExt字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.shader_compile；ShadersExt字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：guest Maxwell→IR/NIR→ir3；NVN槽位/handle lowering；可复用shader state块
- 优化：规范化ShadersExt后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：程序和编译派生对象均不可变版本；编译期间不能读取已改写guest control/data
- 验收：T-SHADER: stage link、间接资源索引、builtin、subgroup32、精度和cache identity；ProgramSetShadersExt参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L91)

## nvnProgramSetSampleShading

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；SampleShading字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.shader_compile；SampleShading字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：guest Maxwell→IR/NIR→ir3；NVN槽位/handle lowering；可复用shader state块
- 优化：规范化SampleShading后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：程序和编译派生对象均不可变版本；编译期间不能读取已改写guest control/data
- 验收：T-SHADER: stage link、间接资源索引、builtin、subgroup32、精度和cache identity；ProgramSetSampleShading参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L92)

## nvnTextureBuilderSetDevice

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, NVNdevice *d)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存设备身份并验证同设备引用
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L145)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L317)

## nvnTextureBuilderSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNtextureBuilder *b)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：NVNdevice *dev = TB(b)->dev; memset(b, 0, sizeof(tex_builder_t)); TB(b)->dev = dev; TB(b)->d.target = NVN_TEXTURE_TARGET_2D; TB(b)->d.width = TB(b)->d.height = TB(b)->d.depth = TB(b)->d.levels = 1;
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L146)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L313)

## nvnTextureBuilderSetFlags

- 阶段/证据：P0-D2R静态调用集合；static=4；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, int f)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Flags后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetFlags参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L147)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L318)

## nvnTextureBuilderSetTarget

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, NVNtextureTarget t)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；1D/2D/3D/cube/buffer/array类型及view兼容性；不把array层数当depth
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；1D/2D/3D/cube/buffer/array类型及view兼容性；不把array层数当depth
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Target后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetTarget参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L148)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L319)

## nvnTextureBuilderSetWidth

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, int v)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；宽度字段；>0和最大尺寸检查
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；宽度字段；>0和最大尺寸检查
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Width后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetWidth参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L149)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L320)

## nvnTextureBuilderSetHeight

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, int v)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；高度字段；与target约束联合验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；高度字段；与target约束联合验证
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Height后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetHeight参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L150)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L321)

## nvnTextureBuilderSetDepth

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, int v)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；3D深度或array/cube层数按target区分
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；3D深度或array/cube层数按target区分
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Depth后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetDepth参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L151)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L322)

## nvnTextureBuilderSetSize1D

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, int w)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；写width；height/depth置1
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；写width；height/depth置1
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Size1D后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetSize1D参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L152)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L323)

## nvnTextureBuilderSetSize2D

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNtextureBuilder*, int, int)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；写width/height；depth置1
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；写width/height；depth置1
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Size2D后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetSize2D参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L153)

## nvnTextureBuilderSetSize3D

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNtextureBuilder*, int, int, int)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；写width/height/depth
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；写width/height/depth
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Size3D后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetSize3D参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L154)

## nvnTextureBuilderSetLevels

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, int v)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；base/count或mip总数按签名；校验最大mip及零值含义
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；base/count或mip总数按签名；校验最大mip及零值含义
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Levels后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetLevels参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L155)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L324)

## nvnTextureBuilderSetFormat

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, NVNformat f)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Format后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetFormat参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L156)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L325)

## nvnTextureBuilderSetSamples

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, int s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；采样数独立于raster samples；按后端1/2/4等真能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；采样数独立于raster samples；按后端1/2/4等真能力门控
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Samples后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetSamples参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L157)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L326)

## nvnTextureBuilderSetSwizzle

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNtextureBuilder*, NVNtextureSwizzle, NVNtextureSwizzle, NVNtextureSwizzle, NVNtextureSwizzle)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；RGBA selector组合；优先硬件texture descriptor，必要时shader swizzle
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；RGBA selector组合；优先硬件texture descriptor，必要时shader swizzle
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Swizzle后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetSwizzle参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L158)

## nvnTextureBuilderSetDepthStencilMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNtextureBuilder*, NVNtextureDepthStencilMode)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；depth或stencil取样aspect，不能只改view数值格式
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；depth或stencil取样aspect，不能只改view数值格式
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化DepthStencilMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetDepthStencilMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L159)

## nvnTextureBuilderGetStorageSize

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`size_t (const NVNtextureBuilder *b)`；candidate-definition
- 使用：为guest布局/存储预算查询TextureBuilderGetStorageSize
- 消费：host计算guest逻辑布局；不分配或等待GPU
- 实现：GuestTegraLayout.query(TextureBuilderGetStorageSize)；blockHeight/mip/layer/sample/压缩块；host fdl/Mali size另存
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：布局key缓存；不要按每次draw重算
- 寿命/同步：返回大小与初始化接受范围一致；TextureGetStorageSize为int32而BuilderSize为size_t；ZCull opaque协议单独定义
- 验收：NPOT/mip/MSAA/array/BC/sparse、溢出和return宽度
- 门禁：NVN稀疏/zcull布局数据结构需原机oracle，不得任意0
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L160)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L354)

## nvnTextureBuilderGetStorageAlignment

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`size_t (const NVNtextureBuilder *b)`；candidate-definition
- 使用：为guest布局/存储预算查询TextureBuilderGetStorageAlignment
- 消费：host计算guest逻辑布局；不分配或等待GPU
- 实现：GuestTegraLayout.query(TextureBuilderGetStorageAlignment)；blockHeight/mip/layer/sample/压缩块；host fdl/Mali size另存
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：布局key缓存；不要按每次draw重算
- 寿命/同步：返回大小与初始化接受范围一致；TextureGetStorageSize为int32而BuilderSize为size_t；ZCull opaque协议单独定义
- 验收：NPOT/mip/MSAA/array/BC/sparse、溢出和return宽度
- 门禁：NVN稀疏/zcull布局数据结构需原机oracle，不得任意0
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L161)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L360)

## nvnTextureBuilderSetStorage

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, NVNmemoryPool *p, ptrdiff_t off)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存pool/offset/size或host指针；检查溢出、对齐和pool容量；不复制资源字节
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存pool/offset/size或host指针；检查溢出、对齐和pool容量；不复制资源字节
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Storage后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetStorage参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L162)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L328)

## nvnTextureBuilderSetPackagedTextureData

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNtextureBuilder*, const void*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；保存外部packaged字节源描述；明确消费时刻，generic模式保持Tegra格式
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存外部packaged字节源描述；明确消费时刻，generic模式保持Tegra格式
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化PackagedTextureData后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetPackagedTextureData参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L163)

## nvnTextureBuilderSetPackagedTextureLayout

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, const NVNpackagedTextureLayout *l)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；复制布局tag并区分Tegra布局与显式qualified-native资产tag；不能仅据设备名猜布局
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；复制布局tag并区分Tegra布局与显式qualified-native资产tag；不能仅据设备名猜布局
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化PackagedTextureLayout后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetPackagedTextureLayout参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L164)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L329)

## nvnTextureBuilderSetStride

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNtextureBuilder *b, ptrdiff_t s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；字节行stride或vertex stride按对象类型；溢出/对齐校验，linear texture才可用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；字节行stride或vertex stride按对象类型；溢出/对齐校验，linear texture才可用
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Stride后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetStride参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L165)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L327)

## nvnTextureBuilderSetGLTextureName

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNtextureBuilder*, uint32_t)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；GLTextureName字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；GLTextureName字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化GLTextureName后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderSetGLTextureName参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L166)

## nvnTextureBuilderGetStorageClass

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNstorageClass (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；StorageClass字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；StorageClass字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化StorageClass后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetStorageClass参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L167)

## nvnTextureBuilderGetDevice

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存设备身份并验证同设备引用
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L168)

## nvnTextureBuilderGetFlags

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`NVNtextureFlags (const NVNtextureBuilder *b)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Flags后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetFlags参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L169)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L333)

## nvnTextureBuilderGetTarget

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNtextureTarget (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；1D/2D/3D/cube/buffer/array类型及view兼容性；不把array层数当depth
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；1D/2D/3D/cube/buffer/array类型及view兼容性；不把array层数当depth
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Target后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetTarget参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L170)

## nvnTextureBuilderGetWidth

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；宽度字段；>0和最大尺寸检查
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；宽度字段；>0和最大尺寸检查
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Width后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetWidth参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L171)

## nvnTextureBuilderGetHeight

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；高度字段；与target约束联合验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；高度字段；与target约束联合验证
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Height后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetHeight参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L172)

## nvnTextureBuilderGetDepth

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；3D深度或array/cube层数按target区分
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；3D深度或array/cube层数按target区分
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Depth后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetDepth参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L173)

## nvnTextureBuilderGetLevels

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；base/count或mip总数按签名；校验最大mip及零值含义
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；base/count或mip总数按签名；校验最大mip及零值含义
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Levels后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetLevels参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L174)

## nvnTextureBuilderGetFormat

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNformat (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Format后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetFormat参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L175)

## nvnTextureBuilderGetSamples

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；采样数独立于raster samples；按后端1/2/4等真能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；采样数独立于raster samples；按后端1/2/4等真能力门控
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Samples后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetSamples参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L176)

## nvnTextureBuilderGetSwizzle

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNtextureBuilder*, NVNtextureSwizzle*, NVNtextureSwizzle*, NVNtextureSwizzle*, NVNtextureSwizzle*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；RGBA selector组合；优先硬件texture descriptor，必要时shader swizzle
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；RGBA selector组合；优先硬件texture descriptor，必要时shader swizzle
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Swizzle后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetSwizzle参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L177)

## nvnTextureBuilderGetDepthStencilMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNtextureDepthStencilMode (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；depth或stencil取样aspect，不能只改view数值格式
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；depth或stencil取样aspect，不能只改view数值格式
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化DepthStencilMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetDepthStencilMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L178)

## nvnTextureBuilderGetPackagedTextureData

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`const void* (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；保存外部packaged字节源描述；明确消费时刻，generic模式保持Tegra格式
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存外部packaged字节源描述；明确消费时刻，generic模式保持Tegra格式
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化PackagedTextureData后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetPackagedTextureData参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L179)

## nvnTextureBuilderGetPackagedTextureLayout

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；复制布局tag并区分Tegra布局与显式qualified-native资产tag；不能仅据设备名猜布局
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；复制布局tag并区分Tegra布局与显式qualified-native资产tag；不能仅据设备名猜布局
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化PackagedTextureLayout后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetPackagedTextureLayout参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L180)

## nvnTextureBuilderGetStride

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`ptrdiff_t (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；字节行stride或vertex stride按对象类型；溢出/对齐校验，linear texture才可用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；字节行stride或vertex stride按对象类型；溢出/对齐校验，linear texture才可用
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Stride后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetStride参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L181)

## nvnTextureBuilderGetSparseTileLayout

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNtextureBuilder*, NVNtextureSparseTileLayout*)`；candidate-generated-header
- 使用：为guest布局/存储预算查询TextureBuilderGetSparseTileLayout
- 消费：host计算guest逻辑布局；不分配或等待GPU
- 实现：GuestTegraLayout.query(TextureBuilderGetSparseTileLayout)；blockHeight/mip/layer/sample/压缩块；host fdl/Mali size另存
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：布局key缓存；不要按每次draw重算
- 寿命/同步：返回大小与初始化接受范围一致；TextureGetStorageSize为int32而BuilderSize为size_t；ZCull opaque协议单独定义
- 验收：NPOT/mip/MSAA/array/BC/sparse、溢出和return宽度
- 门禁：NVN稀疏/zcull布局数据结构需原机oracle，不得任意0
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L182)

## nvnTextureBuilderGetZCullStorageSize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；ZCullStorageSize字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；ZCullStorageSize字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化ZCullStorageSize后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetZCullStorageSize参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L184)

## nvnTextureBuilderGetMemoryPool

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNmemoryPool (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回对象原来的guest pool身份，不泄露host BO
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；返回对象原来的guest pool身份，不泄露host BO
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化MemoryPool后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetMemoryPool参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L185)

## nvnTextureBuilderGetMemoryOffset

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`ptrdiff_t (*)(const NVNtextureBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回相对guest pool的偏移，非host native重排偏移
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；返回相对guest pool的偏移，非host native重排偏移
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化MemoryOffset后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetMemoryOffset参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L186)

## nvnTextureBuilderGetRawStorageClass

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；RawStorageClass字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；RawStorageClass字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化RawStorageClass后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同；现实现无原型返回1并标注unknown；不可将该常数当规范
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureBuilderGetRawStorageClass参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L187)

## nvnTextureViewSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=8；source-defined
- 候选ABI：`void (NVNtextureView *v)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：memset(v, 0, sizeof(view_t));
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L188)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L482)

## nvnTextureViewSetLevels

- 阶段/证据：P0-D2R静态调用集合；static=10；source-defined
- 候选ABI：`void (NVNtextureView *v, int base, int n)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；base/count或mip总数按签名；校验最大mip及零值含义
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；base/count或mip总数按签名；校验最大mip及零值含义
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Levels后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewSetLevels参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L189)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L483)

## nvnTextureViewSetLayers

- 阶段/证据：P0-D2R静态调用集合；static=9；source-defined
- 候选ABI：`void (NVNtextureView *v, int min, int n)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；base/count层范围；cube面与array层精确计算
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；base/count层范围；cube面与array层精确计算
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Layers后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewSetLayers参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L190)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L484)

## nvnTextureViewSetFormat

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNtextureView *v, NVNformat f)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Format后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewSetFormat参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L191)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L485)

## nvnTextureViewSetSwizzle

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNtextureView*, NVNtextureSwizzle, NVNtextureSwizzle, NVNtextureSwizzle, NVNtextureSwizzle)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；RGBA selector组合；优先硬件texture descriptor，必要时shader swizzle
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；RGBA selector组合；优先硬件texture descriptor，必要时shader swizzle
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Swizzle后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewSetSwizzle参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L192)

## nvnTextureViewSetDepthStencilMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNtextureView*, NVNtextureDepthStencilMode)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；depth或stencil取样aspect，不能只改view数值格式
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；depth或stencil取样aspect，不能只改view数值格式
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化DepthStencilMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewSetDepthStencilMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L193)

## nvnTextureViewSetTarget

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (NVNtextureView *v, NVNtextureTarget t)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；1D/2D/3D/cube/buffer/array类型及view兼容性；不把array层数当depth
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；1D/2D/3D/cube/buffer/array类型及view兼容性；不把array层数当depth
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Target后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewSetTarget参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L194)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L486)

## nvnTextureViewGetLevels

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNtextureView*, int*, int*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；base/count或mip总数按签名；校验最大mip及零值含义
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；base/count或mip总数按签名；校验最大mip及零值含义
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Levels后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewGetLevels参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L195)

## nvnTextureViewGetLayers

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`NVNboolean (const NVNtextureView *v, int *min, int *n)`；candidate-definition
- 使用：查询view层范围及是否显式设置
- 消费：host写min/n两个int32并返回boolean
- 实现：core读取view.set_layers/min/num；无GPU包
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：直接读已规范化字段
- 寿命/同步：未设置字段返回值/输出按ABI；现source写两个0但需oracle验证
- 验收：默认view、显式0/多层、返回值及输出内存哨兵
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L196)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L487)

## nvnTextureViewGetFormat

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNtextureView*, NVNformat*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Format后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewGetFormat参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L197)

## nvnTextureViewGetSwizzle

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNtextureView*, NVNtextureSwizzle*, NVNtextureSwizzle*, NVNtextureSwizzle*, NVNtextureSwizzle*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；RGBA selector组合；优先硬件texture descriptor，必要时shader swizzle
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；RGBA selector组合；优先硬件texture descriptor，必要时shader swizzle
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Swizzle后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewGetSwizzle参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L198)

## nvnTextureViewGetDepthStencilMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNtextureView*, NVNtextureDepthStencilMode*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；depth或stencil取样aspect，不能只改view数值格式
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；depth或stencil取样aspect，不能只改view数值格式
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化DepthStencilMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewGetDepthStencilMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L199)

## nvnTextureViewGetTarget

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNtextureView*, NVNtextureTarget*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；1D/2D/3D/cube/buffer/array类型及view兼容性；不把array层数当depth
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；1D/2D/3D/cube/buffer/array类型及view兼容性；不把array层数当depth
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Target后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewGetTarget参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L200)

## nvnTextureViewCompare

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNtextureView*, const NVNtextureView*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；TextureViewCompare对象操作
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；TextureViewCompare对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl布局与view编码；CCU/UCHE/LRZ域；GMEM物化
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureViewCompare参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L201)

## nvnTextureInitialize

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`NVNboolean (NVNtexture *t, const NVNtextureBuilder *b)`；candidate-definition
- 使用：构造Texture对象
- 消费：host执行；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl布局与view编码；CCU/UCHE/LRZ域；GMEM物化
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L202)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L437)

## nvnTextureGetZCullStorageSize

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`size_t (*)(const NVNtexture*)`；candidate-generated-header
- 使用：为guest布局/存储预算查询TextureGetZCullStorageSize
- 消费：host计算guest逻辑布局；不分配或等待GPU
- 实现：GuestTegraLayout.query(TextureGetZCullStorageSize)；blockHeight/mip/layer/sample/压缩块；host fdl/Mali size另存
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：布局key缓存；不要按每次draw重算
- 寿命/同步：返回大小与初始化接受范围一致；TextureGetStorageSize为int32而BuilderSize为size_t；ZCull opaque协议单独定义
- 验收：NPOT/mip/MSAA/array/BC/sparse、溢出和return宽度
- 门禁：NVN稀疏/zcull布局数据结构需原机oracle，不得任意0
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L203)

## nvnTextureFinalize

- 阶段/证据：P0-D2R静态调用集合；static=4；source-defined
- 候选ABI：`void (NVNtexture *t)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；TextureFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；TextureFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl布局与view编码；CCU/UCHE/LRZ域；GMEM物化
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L204)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L465)

## nvnTextureGetStorageClass

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNstorageClass (*)(const NVNtexture*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；StorageClass字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；StorageClass字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化StorageClass后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetStorageClass参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L206)

## nvnTextureGetViewOffset

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`ptrdiff_t (const NVNtexture *t, const NVNtextureView *v)`；candidate-definition
- 使用：查询指定view在guest texture storage中的偏移
- 消费：host读取texture/view元数据并返回ptrdiff64
- 实现：GuestTegraLayout.view_offset(baseMip,baseLayer,aspect)；不返回native重排offset
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：复用guest layout缓存
- 寿命/同步：view默认继承和不兼容格式拒绝；guest逻辑字节真值保持
- 验收：mip/layer/3Dslice、压缩格式、与ReadTexels定位一致
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L207)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L490)

## nvnTextureGetFlags

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`NVNtextureFlags (const NVNtexture *t)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存合法bit集合；未知bit拒绝，不把host扩展bit混入guest
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Flags后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetFlags参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L208)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L466)

## nvnTextureGetTarget

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNtextureTarget (*)(const NVNtexture*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；1D/2D/3D/cube/buffer/array类型及view兼容性；不把array层数当depth
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；1D/2D/3D/cube/buffer/array类型及view兼容性；不把array层数当depth
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Target后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetTarget参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L209)

## nvnTextureGetWidth

- 阶段/证据：P0-D2R静态调用集合；static=7；source-defined
- 候选ABI：`int (const NVNtexture *t)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；宽度字段；>0和最大尺寸检查
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；宽度字段；>0和最大尺寸检查
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Width后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetWidth参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L210)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L467)

## nvnTextureGetHeight

- 阶段/证据：P0-D2R静态调用集合；static=7；source-defined
- 候选ABI：`int (const NVNtexture *t)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；高度字段；与target约束联合验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；高度字段；与target约束联合验证
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Height后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetHeight参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L211)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L468)

## nvnTextureGetDepth

- 阶段/证据：P0-D2R静态调用集合；static=7；source-defined
- 候选ABI：`int (const NVNtexture *t)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；3D深度或array/cube层数按target区分
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；3D深度或array/cube层数按target区分
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Depth后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetDepth参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L212)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L469)

## nvnTextureGetLevels

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNtexture*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；base/count或mip总数按签名；校验最大mip及零值含义
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；base/count或mip总数按签名；校验最大mip及零值含义
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Levels后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetLevels参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L213)

## nvnTextureGetFormat

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNformat (*)(const NVNtexture*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；保存NVN格式；字节大小、数值类型、压缩block、depth/stencil aspect分别验证
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Format后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetFormat参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L214)

## nvnTextureGetSamples

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`int (*)(const NVNtexture*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；采样数独立于raster samples；按后端1/2/4等真能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；采样数独立于raster samples；按后端1/2/4等真能力门控
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Samples后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetSamples参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L215)

## nvnTextureGetSwizzle

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNtexture*, NVNtextureSwizzle*, NVNtextureSwizzle*, NVNtextureSwizzle*, NVNtextureSwizzle*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；RGBA selector组合；优先硬件texture descriptor，必要时shader swizzle
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；RGBA selector组合；优先硬件texture descriptor，必要时shader swizzle
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Swizzle后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetSwizzle参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L216)

## nvnTextureGetDepthStencilMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNtextureDepthStencilMode (*)(const NVNtexture*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；depth或stencil取样aspect，不能只改view数值格式
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；depth或stencil取样aspect，不能只改view数值格式
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化DepthStencilMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetDepthStencilMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L217)

## nvnTextureGetStride

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`ptrdiff_t (*)(const NVNtexture*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；字节行stride或vertex stride按对象类型；溢出/对齐校验，linear texture才可用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；字节行stride或vertex stride按对象类型；溢出/对齐校验，linear texture才可用
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Stride后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetStride参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L218)

## nvnTextureGetTextureAddress

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNtextureAddress (*)(const NVNtexture*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；TextureAddress字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；TextureAddress字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化TextureAddress后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetTextureAddress参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L219)

## nvnTextureGetSparseTileLayout

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNtexture*, NVNtextureSparseTileLayout*)`；candidate-generated-header
- 使用：为guest布局/存储预算查询TextureGetSparseTileLayout
- 消费：host计算guest逻辑布局；不分配或等待GPU
- 实现：GuestTegraLayout.query(TextureGetSparseTileLayout)；blockHeight/mip/layer/sample/压缩块；host fdl/Mali size另存
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：布局key缓存；不要按每次draw重算
- 寿命/同步：返回大小与初始化接受范围一致；TextureGetStorageSize为int32而BuilderSize为size_t；ZCull opaque协议单独定义
- 验收：NPOT/mip/MSAA/array/BC/sparse、溢出和return宽度
- 门禁：NVN稀疏/zcull布局数据结构需原机oracle，不得任意0
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L220)

## nvnTextureWriteTexels

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNtexture*, const NVNtextureView*, const NVNcopyRegion*, const void*)`；candidate-generated-header
- 使用：CPU访问texture区域；TextureWriteTexels
- 消费：host读取区域/stride；Read等待GPU写并转换，Write按API复制输入，Flush/Invalidate发布缓存域
- 实现：guest layout accessor+native materialization；Adreno fdl/Mali layout不能直接当Tegra字节
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl布局与view编码；CCU/UCHE/LRZ域；GMEM物化
- 优化：按区域/压缩块更新，不整纹理下载；只转换需要CPU观察部分
- 寿命/同步：CPU/GPU producer authority按子资源及byte range；cache维护和tiling转换分开
- 验收：非对齐小区域、row/image stride、BC块、mip/layer、GPU写后读
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L221)

## nvnTextureWriteTexelsStrided

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNtexture*, const NVNtextureView*, const NVNcopyRegion*, const void*, ptrdiff_t, ptrdiff_t)`；candidate-generated-header
- 使用：CPU访问texture区域；TextureWriteTexelsStrided
- 消费：host读取区域/stride；Read等待GPU写并转换，Write按API复制输入，Flush/Invalidate发布缓存域
- 实现：guest layout accessor+native materialization；Adreno fdl/Mali layout不能直接当Tegra字节
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl布局与view编码；CCU/UCHE/LRZ域；GMEM物化
- 优化：按区域/压缩块更新，不整纹理下载；只转换需要CPU观察部分
- 寿命/同步：CPU/GPU producer authority按子资源及byte range；cache维护和tiling转换分开
- 验收：非对齐小区域、row/image stride、BC块、mip/layer、GPU写后读
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L222)

## nvnTextureReadTexels

- 阶段/证据：P0-D2R静态调用集合；static=16；source-defined
- 候选ABI：`void (const NVNtexture *t, const NVNtextureView *v, const NVNcopyRegion *r, void *dst)`；candidate-definition
- 使用：CPU同步读出texture指定区域至dst
- 消费：host返回前填dst；仅等待相关producer ticket；复制region/view和stride参数
- 实现：texture_materialize guest linear readback；wb=ceil(width/blockWidth)*bytesPerBlock，hb=ceil(height/blockHeight)，row=0→wb，img=0→row*hb
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl布局与view编码；CCU/UCHE/LRZ域；GMEM物化
- 优化：按请求区域读回；dst footprint=(depth-1)*img+(hb-1)*row+wb，逐行拷贝不覆盖padding
- 寿命/同步：checked arithmetic，零维单独处理；输出是API线性texel规则而非整个Tegra池raw bytes
- 验收：padding哨兵、BCblock、zero strides、mip/array、GPUproducer完成
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L223)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L534)

## nvnTextureReadTexelsStrided

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (const NVNtexture *t, const NVNtextureView *v, const NVNcopyRegion *r, void *dst, ptrdiff_t row, ptrdiff_t img)`；candidate-definition
- 使用：CPU同步读出texture指定区域至dst
- 消费：host返回前填dst；仅等待相关producer ticket；复制region/view和stride参数
- 实现：texture_materialize guest linear readback；wb=ceil(width/blockWidth)*bytesPerBlock，hb=ceil(height/blockHeight)，row=0→wb，img=0→row*hb
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl布局与view编码；CCU/UCHE/LRZ域；GMEM物化
- 优化：按请求区域读回；dst footprint=(depth-1)*img+(hb-1)*row+wb，逐行拷贝不覆盖padding
- 寿命/同步：checked arithmetic，零维单独处理；输出是API线性texel规则而非整个Tegra池raw bytes
- 验收：padding哨兵、BCblock、zero strides、mip/array、GPUproducer完成
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L224)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L537)

## nvnTextureFlushTexels

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNtexture*, const NVNtextureView*, const NVNcopyRegion*)`；candidate-generated-header
- 使用：CPU访问texture区域；TextureFlushTexels
- 消费：host读取区域/stride；Read等待GPU写并转换，Write按API复制输入，Flush/Invalidate发布缓存域
- 实现：guest layout accessor+native materialization；Adreno fdl/Mali layout不能直接当Tegra字节
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl布局与view编码；CCU/UCHE/LRZ域；GMEM物化
- 优化：按区域/压缩块更新，不整纹理下载；只转换需要CPU观察部分
- 寿命/同步：CPU/GPU producer authority按子资源及byte range；cache维护和tiling转换分开
- 验收：非对齐小区域、row/image stride、BC块、mip/layer、GPU写后读
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L225)

## nvnTextureInvalidateTexels

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (const NVNtexture *t, const NVNtextureView *v, const NVNcopyRegion *r)`；candidate-definition
- 使用：CPU观察GPU写纹理前的host缓存失效
- 消费：host读取texture/view/region，无dst和stride；执行GPU→HOST cache失效，不自动假定GPU已完成
- 实现：cache_domain_ops(GPU→HOST,subresource region)；若表示转换必要按已完成producer版本进行
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl布局与view编码；CCU/UCHE/LRZ域；GMEM物化
- 优化：只invalidate受影响块/cacheline；不用无条件全queue drain
- 寿命/同步：调用者等待义务和invalidate分开；guest布局转换也不等于cacheinvalidate；现source nop需补实现
- 验收：GPU完成+invalidate后读、缺等待行为、BC区域、别名cacheline
- 门禁：NVN确切同步前提需oracle；不将nop当通用实现
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L226)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L540)

## nvnTextureGetMemoryPool

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNmemoryPool (*)(const NVNtexture*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回对象原来的guest pool身份，不泄露host BO
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；返回对象原来的guest pool身份，不泄露host BO
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化MemoryPool后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetMemoryPool参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L227)

## nvnTextureGetMemoryOffset

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`ptrdiff_t (*)(const NVNtexture*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；返回相对guest pool的偏移，非host native重排偏移
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.texture_materialize / descriptor_encode / transfer_emit；返回相对guest pool的偏移，非host native重排偏移
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化MemoryOffset后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：guest Tegra字节是generic模式真值；native影像是版本化派生表示；不能将页共享当tiling相同
- 验收：T-TEX: mip/layer/格式别名/压缩块边界、CPU改写后采样、GPU写后CPU读；TextureGetMemoryOffset参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L228)

## nvnTextureGetStorageSize

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`int (const NVNtexture *t)`；candidate-definition
- 使用：为guest布局/存储预算查询TextureGetStorageSize
- 消费：host计算guest逻辑布局；不分配或等待GPU
- 实现：GuestTegraLayout.query(TextureGetStorageSize)；blockHeight/mip/layer/sample/压缩块；host fdl/Mali size另存
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：布局key缓存；不要按每次draw重算
- 寿命/同步：返回大小与初始化接受范围一致；TextureGetStorageSize为int32而BuilderSize为size_t；ZCull opaque协议单独定义
- 验收：NPOT/mip/MSAA/array/BC/sparse、溢出和return宽度
- 门禁：NVN稀疏/zcull布局数据结构需原机oracle，不得任意0
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L229)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L470)

## nvnTextureCompare

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`NVNboolean (const NVNtexture *a, const NVNtexture *b)`；candidate-definition
- 使用：比较两个texture的逻辑等价关系
- 消费：host读取两对象元数据，不读像素
- 实现：core texture equivalence；poolidentity+offset+layout+format+target+flags等经NVNoracle确认字段
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：fdl布局与view编码；CCU/UCHE/LRZ域；GMEM物化
- 优化：比较规范化key；不创建native image
- 寿命/同步：不能用host image指针相等代替，也不比较全部像素
- 验收：两个对象同storage、不同view/format、相同像素不同storage
- 门禁：参与比较的完整字段需oracle；现实现不是闭源规范
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L230)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L471)

## nvnTextureGetRawStorageClass

- 阶段/证据：P0-D2R静态调用集合；static=8；source-defined
- 候选ABI：`int (const NVNtexture *t)`；candidate-definition
- 使用：读取guest texture raw storage class
- 消费：host根据guest格式/布局返回枚举
- 实现：GuestTegraLayout.storage_class；不返回Mali/Adreno格式号
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：格式分类表缓存
- 寿命/同步：现源码无原型且返回1标注unknown；保留明确语义门禁
- 验收：已知颜色/深度/压缩/线性类与原机对照
- 门禁：rawStorageClass枚举含义未恢复，不能宣称通用支持
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L232)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L480)

## nvnSamplerBuilderSetDevice

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNsamplerBuilder *b, NVNdevice *d)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；保存设备身份并验证同设备引用
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L233)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L552)

## nvnSamplerBuilderSetDefaults

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNsamplerBuilder *b)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；按重建ABI规定设置默认值；保留Device字段与否逐对象对照已有实现/原机oracle，不能统一memset所有对象
- 消费：host立即写默认字段，无GPU指令；保留device与否按该对象规则，不统一memset
- 实现：core按字段初始化；仓库候选默认行为（不是规范证明）：NVNdevice *dev = ((samp_builder_t *)b)->dev; memset(b, 0, sizeof(samp_builder_t)); ((samp_builder_t *)b)->dev = dev; samp_par *o = SB(b); o->minf = NVN_MIN_FILTER_NEAREST_MIPMAP_LINEAR; o->magf = NVN_MAG_FILTER_LINEAR; // GL 默认 | GL defaults o->ws = o->wt = o->wr = NVN_WRAP_MODE_REPEAT; o->cmp_func = NVN_COMPARE_FUNC_LESS; o->lod_min = -1000.f; o->lod_max = 1000.f; o->aniso = 1.f;
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Defaults后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetDefaults参数边界/默认值/错误路径
- 门禁：默认值/Device保留规则来自固定源码，须由getter/oracle验收；不认为所有对象相同
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L234)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L545)

## nvnSamplerBuilderSetMinMagFilter

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNsamplerBuilder *b, NVNminFilter mi, NVNmagFilter ma)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；min/mag及mipmap过滤枚举分别编码
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；min/mag及mipmap过滤枚举分别编码
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化MinMagFilter后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetMinMagFilter参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L235)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L553)

## nvnSamplerBuilderSetWrapMode

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNsamplerBuilder *b, NVNwrapMode s, NVNwrapMode t, NVNwrapMode r)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；S/T/R wrap分别编码，border/镜像clamp按caps处理
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；S/T/R wrap分别编码，border/镜像clamp按caps处理
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化WrapMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetWrapMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L236)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L554)

## nvnSamplerBuilderSetLodClamp

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNsamplerBuilder *b, float lo, float hi)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；min/maxLOD；保留浮点值及NVN clamp规则
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；min/maxLOD；保留浮点值及NVN clamp规则
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化LodClamp后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetLodClamp参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L237)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L555)

## nvnSamplerBuilderSetLodBias

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNsamplerBuilder *b, float v)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；有符号LOD bias，硬件定点精度的误差需oracle
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；有符号LOD bias，硬件定点精度的误差需oracle
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化LodBias后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetLodBias参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L238)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L556)

## nvnSamplerBuilderSetCompare

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNsamplerBuilder *b, NVNcompareMode m, NVNcompareFunc f)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；enable+compare func；比较采样与普通采样不同shader路径
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；enable+compare func；比较采样与普通采样不同shader路径
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化Compare后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetCompare参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L239)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L557)

## nvnSamplerBuilderSetBorderColor

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNsamplerBuilder *b, const float *c)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；float RGBA边框，类型与格式联合验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；float RGBA边框，类型与格式联合验证
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化BorderColor后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetBorderColor参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L240)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L558)

## nvnSamplerBuilderSetBorderColori

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNsamplerBuilder*, const int*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；signed int RGBA边框，不按float转换
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；signed int RGBA边框，不按float转换
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化BorderColori后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetBorderColori参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L241)

## nvnSamplerBuilderSetBorderColorui

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNsamplerBuilder*, const uint32_t*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；unsigned RGBA边框，不按float转换
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；unsigned RGBA边框，不按float转换
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化BorderColorui后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetBorderColorui参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L242)

## nvnSamplerBuilderSetMaxAnisotropy

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNsamplerBuilder *b, float v)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；合法范围并量化到后端支持档位；NVN可观察差异需测试
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；合法范围并量化到后端支持档位；NVN可观察差异需测试
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化MaxAnisotropy后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetMaxAnisotropy参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L243)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L559)

## nvnSamplerBuilderSetReductionFilter

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(NVNsamplerBuilder*, NVNsamplerReduction)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；weighted/min/max filtering；无硬件支持不得伪称普通线性等价
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；weighted/min/max filtering；无硬件支持不得伪称普通线性等价
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化ReductionFilter后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetReductionFilter参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L244)

## nvnSamplerBuilderSetLodSnap

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (NVNsamplerBuilder *b, float v)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；专有LOD取整/吸附语义尚需原机oracle；目前不得静默忽略
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；专有LOD取整/吸附语义尚需原机oracle；目前不得静默忽略
- Turnip：本入口仅host规范化字段；后续Initialize/Register/Bind/Draw才消费硬件编码，不发GPU指令
- 优化：规范化LodSnap后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderSetLodSnap参数边界/默认值/错误路径
- 门禁：；现源码函数存在但实现未完成/空操作，不代表语义正确，必须实现或能力门控
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L245)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L560)

## nvnSamplerBuilderGetDevice

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；ABI门禁；static=0；resolver-redstub
- 候选ABI：`ABI未收录：必须恢复参数/返回/调用约定后才可实现`；ABI-GATE
- 使用：推断：初始化/资源管理时构建或查询对象；保存设备身份并验证同设备引用
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；保存设备身份并验证同设备引用
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Device后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetDevice参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L246)

## nvnSamplerBuilderGetMinMagFilter

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`void (const NVNsamplerBuilder *b, NVNminFilter *mi, NVNmagFilter *ma)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；min/mag及mipmap过滤枚举分别编码
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；min/mag及mipmap过滤枚举分别编码
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化MinMagFilter后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetMinMagFilter参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L247)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L561)

## nvnSamplerBuilderGetWrapMode

- 阶段/证据：P0-D2R静态调用集合；static=3；source-defined
- 候选ABI：`void (const NVNsamplerBuilder *b, NVNwrapMode *s, NVNwrapMode *t, NVNwrapMode *r)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；S/T/R wrap分别编码，border/镜像clamp按caps处理
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；S/T/R wrap分别编码，border/镜像clamp按caps处理
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化WrapMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetWrapMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L248)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L564)

## nvnSamplerBuilderGetLodClamp

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsamplerBuilder*, float*, float*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；min/maxLOD；保留浮点值及NVN clamp规则
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；min/maxLOD；保留浮点值及NVN clamp规则
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化LodClamp后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetLodClamp参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L249)

## nvnSamplerBuilderGetLodBias

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`float (*)(const NVNsamplerBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；有符号LOD bias，硬件定点精度的误差需oracle
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；有符号LOD bias，硬件定点精度的误差需oracle
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化LodBias后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetLodBias参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L250)

## nvnSamplerBuilderGetCompare

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsamplerBuilder*, NVNcompareMode*, NVNcompareFunc*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；enable+compare func；比较采样与普通采样不同shader路径
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；enable+compare func；比较采样与普通采样不同shader路径
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Compare后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetCompare参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L251)

## nvnSamplerBuilderGetBorderColor

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsamplerBuilder*, float*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；float RGBA边框，类型与格式联合验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；float RGBA边框，类型与格式联合验证
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化BorderColor后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetBorderColor参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L252)

## nvnSamplerBuilderGetBorderColori

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsamplerBuilder*, int*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；signed int RGBA边框，不按float转换
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；signed int RGBA边框，不按float转换
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化BorderColori后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetBorderColori参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L253)

## nvnSamplerBuilderGetBorderColorui

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsamplerBuilder*, uint32_t*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；unsigned RGBA边框，不按float转换
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；unsigned RGBA边框，不按float转换
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化BorderColorui后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetBorderColorui参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L254)

## nvnSamplerBuilderGetMaxAnisotropy

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`float (*)(const NVNsamplerBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；合法范围并量化到后端支持档位；NVN可观察差异需测试
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；合法范围并量化到后端支持档位；NVN可观察差异需测试
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化MaxAnisotropy后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetMaxAnisotropy参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L255)

## nvnSamplerBuilderGetReductionFilter

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNsamplerReduction (*)(const NVNsamplerBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；weighted/min/max filtering；无硬件支持不得伪称普通线性等价
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；weighted/min/max filtering；无硬件支持不得伪称普通线性等价
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化ReductionFilter后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetReductionFilter参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L256)

## nvnSamplerBuilderGetLodSnap

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`float (*)(const NVNsamplerBuilder*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；专有LOD取整/吸附语义尚需原机oracle；目前不得静默忽略
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；专有LOD取整/吸附语义尚需原机oracle；目前不得静默忽略
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化LodSnap后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerBuilderGetLodSnap参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L257)

## nvnSamplerInitialize

- 阶段/证据：P0-D2R静态调用集合；static=1；source-defined
- 候选ABI：`NVNboolean (NVNsampler *s, const NVNsamplerBuilder *b)`；candidate-definition
- 使用：构造Sampler对象
- 消费：host执行；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- 实现：BackendOps.descriptor_encode(sampler)；读取builder各字段并复制为不可变初始ObjectVersion，验证设备/尺寸/flags/offset
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：A6xx纹理采样器状态/边框色表；限制按设备caps
- 优化：对象身份独立；派生不可变编码按内容intern
- 寿命/同步：失败保持可检测未初始化态；对象大小遵循guest ABI；native寿命独立
- 验收：参数边界、错误返回、同地址重建、零初始化与重复初始化
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L258)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L567)

## nvnSamplerFinalize

- 阶段/证据：P0-D2R静态调用集合；static=2；source-defined
- 候选ABI：`void (NVNsampler *s)`；candidate-definition
- 使用：推断：初始化/资源管理时构建或查询对象；SamplerFinalize对象操作
- 消费：host使guest对象失效；native引用按已提交completion ticket延迟回收
- 实现：BackendOps.descriptor_encode(sampler)；SamplerFinalize对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：A6xx纹理采样器状态/边框色表；限制按设备caps
- 优化：退休队列批量回收，禁止每次Finalize等待GPU空闲
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerFinalize参数边界/默认值/错误路径；未提交引用、在途引用、重复Finalize
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L259)；[源码2](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/hle_nvn.c#L570)

## nvnSamplerGetMinMagFilter

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsampler*, NVNminFilter*, NVNmagFilter*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；min/mag及mipmap过滤枚举分别编码
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；min/mag及mipmap过滤枚举分别编码
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化MinMagFilter后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerGetMinMagFilter参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L261)

## nvnSamplerGetWrapMode

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsampler*, NVNwrapMode*, NVNwrapMode*, NVNwrapMode*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；S/T/R wrap分别编码，border/镜像clamp按caps处理
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；S/T/R wrap分别编码，border/镜像clamp按caps处理
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化WrapMode后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerGetWrapMode参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L262)

## nvnSamplerGetLodClamp

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsampler*, float*, float*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；min/maxLOD；保留浮点值及NVN clamp规则
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；min/maxLOD；保留浮点值及NVN clamp规则
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化LodClamp后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerGetLodClamp参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L263)

## nvnSamplerGetLodBias

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`float (*)(const NVNsampler*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；有符号LOD bias，硬件定点精度的误差需oracle
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；有符号LOD bias，硬件定点精度的误差需oracle
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化LodBias后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerGetLodBias参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L264)

## nvnSamplerGetCompare

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsampler*, NVNcompareMode*, NVNcompareFunc*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；enable+compare func；比较采样与普通采样不同shader路径
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；enable+compare func；比较采样与普通采样不同shader路径
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化Compare后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerGetCompare参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L265)

## nvnSamplerGetBorderColor

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsampler*, float*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；float RGBA边框，类型与格式联合验证
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；float RGBA边框，类型与格式联合验证
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化BorderColor后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerGetBorderColor参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L266)

## nvnSamplerGetBorderColori

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsampler*, int*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；signed int RGBA边框，不按float转换
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；signed int RGBA边框，不按float转换
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化BorderColori后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerGetBorderColori参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L267)

## nvnSamplerGetBorderColorui

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`void (*)(const NVNsampler*, uint32_t*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；unsigned RGBA边框，不按float转换
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；unsigned RGBA边框，不按float转换
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化BorderColorui后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerGetBorderColorui参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L268)

## nvnSamplerGetMaxAnisotropy

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`float (*)(const NVNsampler*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；合法范围并量化到后端支持档位；NVN可观察差异需测试
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；合法范围并量化到后端支持档位；NVN可观察差异需测试
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化MaxAnisotropy后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerGetMaxAnisotropy参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L269)

## nvnSamplerGetReductionFilter

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNsamplerReduction (*)(const NVNsampler*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；weighted/min/max filtering；无硬件支持不得伪称普通线性等价
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；weighted/min/max filtering；无硬件支持不得伪称普通线性等价
- Turnip：本入口host读取guest逻辑元数据或GuestTegraLayout计算；不materialize、不编码native descriptor、不发GPU指令
- 优化：规范化ReductionFilter后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerGetReductionFilter参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L270)

## nvnSamplerCompare

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(const NVNsampler*, const NVNsampler*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；SamplerCompare对象操作
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.descriptor_encode(sampler)；SamplerCompare对象操作
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：A6xx纹理采样器状态/边框色表；限制按设备caps
- 优化：规范化后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：采样参数版本与纹理内容版本分离；不支持的过滤语义走显式shader方案或拒绝
- 验收：T-SAMP: 边框色类型、LOD、比较过滤、各向异性、reduction与精度；SamplerCompare参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L271)

## nvnProgramSetSubroutineLinkage

- 阶段/证据：P2-完整前端扩展（无静态调用证据不代表未使用）；static=0；resolver-redstub
- 候选ABI：`NVNboolean (*)(NVNprogram*, int, const NVNsubroutineLinkageMapPtr*)`；candidate-generated-header
- 使用：推断：初始化/资源管理时构建或查询对象；SubroutineLinkage字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- 消费：host立即操作/返回对象元数据；无GPU资源字节读取
- 实现：BackendOps.shader_compile；SubroutineLinkage字段必须按参数逐项规范化，保留index/count/type；未知枚举走能力门控
- Turnip：共享家族下游锚点（不是本入口必发指令；具体动作/时点以上述mapping/timing为准）：guest Maxwell→IR/NIR→ir3；NVN槽位/handle lowering；可复用shader state块
- 优化：规范化SubroutineLinkage后比较实际字段，仅改变时提升相应state/version；getter直接读取guest逻辑元数据
- 寿命/同步：程序和编译派生对象均不可变版本；编译期间不能读取已改写guest control/data
- 验收：T-SHADER: stage link、间接资源索引、builtin、subgroup32、精度和cache identity；ProgramSetSubroutineLinkage参数边界/默认值/错误路径
- 门禁：共享ABI/设备资格仍须满足，不能解释为已经验证
- 来源：[源码1](https://github.com/kxn/d2r-mobile/blob/5803e32592ba2cdfc6d3460bb61267453c2ee5fe/dm/gfx/nvn/names.txt#L478)
