> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始模块与正式宿主采样整族构建v18

从未改写的原始main/sdk/subsdk0，通过原统一规则入口重新生成并核对缓存产物。原输入SHA/BuildID、上游、规则/工具/选项及全部产物哈希保留在正常记录，没有给加工后的游戏binary叠补丁。

实际链接72个源文件，其中36个原HOST/HLE源；ARM64硬件原生执行路线、TLSDESC、原装载/宿主门/系统HLE继续复用。新增公共采样模块与共享ledger只各编译一次，保留正常计算缓存、桥接、颜色目标和清除路径；没有旧Metal/Vulkan renderer或CPU模拟器。ELF仅依赖libm/libdl/libandroid/libc。

三个显式采样/mip/Gather开关贯通正常HOST构建、--verify、原Activity APK打包及APK --verify；实际重新校验命令含全部开关。APK沿用既有签名key。本记录只证明规范构建/链接/打包/完整性校验，没有部署、Android执行或原游戏模块执行，也没有性能结论。

完整CASC source可注册，但其原FragDepth Draw仍明确deferred，公共depth整链尚未接完。采样/mip/Gather开关是本次构建的显式选择，并非默认启用或GPU兼容性承诺。实际D2R最新仍是v16，无Draw/Present/首帧，不能以本构建替代。

正常记录ZIP已逐成员解压核长度及SHA256；facts.json列明各成员原路径和哈希。
