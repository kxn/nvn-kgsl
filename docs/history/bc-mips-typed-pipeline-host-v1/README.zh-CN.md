> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# BC mip视图与实际着色器类型传播：当前根源码HOST资格

本批没有新增GPU结果。公共BC多mip资格计划96次提交，须另外完成；既有单mip和原创3D硬件通过不替代它。NVN入口数仍406。

BC1/2/3及sRGB共8格式，在ordinary2D/layer1的多mip/selected base/count视图进入正常查询、录制、late registration、入队和退休链。原canonical pool/offset保留，既有GPU copy owner处理全mip，未另写CPU转换。私有资源转移全部2+2N+(N>1)节点与join，动态aux refs独立于graphics固定资源表；16纹理×15mip可需528个私有节点。控制预算/overflow预检在分配前完成，失败/未知完成按整组保活，后续Finish释放。nearest/no-mip/clamp有限采样状态不等于过滤mip、array或3D。

根源码正常checker：48格式/view/metadata组合，双san，单mip16组合回归，packaged128²/8mip/BH2的明确实验ON构造及unknown marker拒绝；更新descriptor至另一backing/视图、旧CPU Texture释放、全mip未知完成保活和资源清理通过。shadow各san258,014项，包含actual FDL视图、15mip metadata、失败owner及默认OFF拒绝。查询检查与真实API29 KGSL/FDL严格链接通过。HOST mock submit从未计作GPU执行。

着色器部分复用同一Maxwell前端、SPIR-V及NIR/IR3工具，无compiler/schema/recipe改写。正常package把本次actual common NIR的texture类型侧表，按精确VS/FS variant、recipe和完整SPV身份传到不可变Program及enqueue/encode；不从expected query反填actual资源。原archive/pair/recipe ABI与旧注册API保留；新normal header使原host_bridge选择可选typed注册。Program构造/表身份、可选注册ON/OFF、96个producer否定域及实际ARM64全后端和原host桥严格链接通过。

根源码重新正常生成16pair，80项VS/FS binary、pipeline.h/json及types.json与先前正常产物逐字节相同，见native-byte-regression.json。当前实际编译素材无dim3；不得把C1 specialization或原创3D SPV冒充游戏实际3D Maxwell。公共guest3D resolver/owner仍明确拒绝，真正guest3D还需对应实际Draw资源/Program的完整观察与重新正常编译。

两个改动均附当前根源码正常证明，候选proof仅作来源。builder栈地址跨类型复用的独立生命周期缺口尚未在本批修复；新测试使用稳定opaque keys不等于已修复该生产问题。
