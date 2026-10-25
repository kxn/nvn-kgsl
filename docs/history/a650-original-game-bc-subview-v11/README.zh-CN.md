> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始 D2R ARM64 v11：数组层/mip视图越过，新的BC3资源边界

源码46e9ce0，正常原始3.3.94012 ExeFS生成、host/APK构建及部署前完整verify。首次显式启用已核FDL archive的生产BC路径；实际编译67源单元，含36原host/HLE源，动态依赖仅libm/libdl/libandroid/libc。原main/sdk/subsdk0的各7项生成产物与v10逐hash完全相同，共21项：没有为本次测试叠游戏binary补丁。46项RomFS完整逐hash复核复用。

普通APP_UID、pipa/Adreno650/Android14真实进入原模块/nnMain。v10阻塞的format70 BC1 sRGB、128²、84层、7mip的layer0/count1登记通过；后续多个数组和数百次层/mip选择登记继续成功。最后可见id697/mip6登记成功。

新首个错误为TextureBuilderGetStorageSize errno95：flags0、format68 BC3 UNORM、target1普通2D、128²、depth1、8mip、samples0。**不能仅凭此元组归因mip数量**：当前nn_tex_source_layout在HOST直接计算同元组成功，payload24064；尚缺builder其余有效状态/packaged layout/stride/device选择观测。下一步增加最小只读builder诊断，再判断所需通用布局支持；不清空packaged标志或猜布局绕过。

没有真实游戏QueueSubmit/Finish/Draw/Present或首帧。BC实际public24资格是独立证据，不能算成本轮游戏提交。Activity无waitpid，OS退出码unknown；本轮非timeout，runner finally force-stop并核实退出。所有hex快照按块长度/SHA替换，原始/加工NSO、shader/control/ASM未归档。历史record保持原状。
