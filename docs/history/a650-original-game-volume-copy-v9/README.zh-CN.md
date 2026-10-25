> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始D2R ARM64 v9：越过三维BC1资源与复制录制

独立分支源码固定77bd268。由原始3.3.94012 ExeFS main/sdk/subsdk0通过同一rewrite-native入口重新生成，原始输入哈希、上游、规则/工具/源码指纹、全部产物与APK部署前均正常verify。执行原host ABI2/elfdesc、装载/重定位、宿主门与36个原host/HLE源文件；实际编译闭包66源文件，依赖libm/libdl/libandroid/libc，没有旧renderer或CPU解释/JIT/DBT。沿用原规则及游戏流程，手机46个RomFS文件8,973,547,258B完整逐hash复核复用。

普通APP_UID、pipa/Adreno650/Android14实际运行原模块和nnMain。此前v8阻塞的flags0/BC1格式66/true3D32³单mip资源，本次GetStorageSize=16384、alignment512、Initialize成功、descriptor注册成功，并成功录制CopyBufferToTexture(flags0,rowStride256,region32×32×32)。之后游戏继续初始化其它二维资源，成功录制其复制。

首个未支持操作仍名为TextureBuilderGetStorageSize，但资源已不同：flags4（IMAGE）、format41（RGBA16F）、target2（3D）、32×32×32、levels1、samples0，返回errno95。当前3D source-admission只允许flags0，需独立核查IMAGE权限的canonical布局和no-compression policy；不能用清空公开flags或放开未实现ImageStore consumer来冒充完整能力。

本轮**没有真实游戏QueueSubmitCommands、QueueFinish、Draw、Present或首帧证据**；通过的是三维资源创建、注册及复制录制，不能将另外456次三维资格GPU提交算作游戏执行。Activity无waitpid，OS进程退出码unknown；adapter的exit78是边界诊断。runner finally force-stop并确认退出，测试结束。

evidence.redacted.json与两份stderr保留实际阶段/参数，所有hex=快照均逐块替换为长度/SHA256，原始文件哈希见archive-manifest.json。未归档原始/加工模块、shader/control或ASM；正常host/APK、原始输入重写manifest与link audit完整保存。后续实现不得倒写本次历史证据。
