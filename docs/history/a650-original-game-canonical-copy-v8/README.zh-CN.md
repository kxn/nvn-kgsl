> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始 D2R ARM64 v8：越过二维 BC1 命令录制，阻塞三维存储布局

源码固定于独立分支提交6e735fd。原始3.3.94012 ExeFS的main/sdk/subsdk0按仓库同一入口重新生成，执行前再次正常verify原始输入、规则/工具指纹、全部产物与APK；未在旧加工binary上叠补丁。保留原host ABI2/elfdesc、硬件ARM64执行、原规则、装载/重定位、宿主门和36个原host/HLE源文件。实际编译闭包66源文件，依赖libm/libdl/libandroid/libc，没有旧Metal/Vulkan renderer或CPU模拟器。手机已有46个RomFS文件8,973,547,258B逐hash核对后复用，未重传或改游戏逻辑。

普通应用APP_UID / pipa / Adreno650 / Android14实际执行原模块初始化和nnMain路径，随后观察到真实游戏NVN调用。此前v7阻塞的BC1二维64×64资源本次storage size2048B/alignment512、Initialize与descriptor注册完成，CopyBufferToTexture以真实flags0、row stride256、region(0,0,0;64,64,1)成功录制，随后继续创建下一资源。

首个未支持操作现在是TextureBuilderGetStorageSize，实际输入format66（RGBA_DXT1/BC1）、target2（3D）、width/height/depth均32、levels1、samples0、flags0，返回errno95。当前source布局仅支持2D/array；不能把三维切片冒充array来给出大小。后续需补真实3D block-depth寻址、每mip深度/extent、区域Z与搬运，三维采样另行资格。

**本次只证明越过二维BC1复制的录制，尚无真实游戏QueueSubmit/Finish、Draw、Present或首帧证据。** 不能把另外468次公开复制探针和72次资格Draw当作游戏结果。Activity没有waitpid，process exit code仍unknown；adapter记录终止boundary，不能把日志中的exit78当成OS测得退出码。runner finally force-stop并核实进程退出，测试已结束。

evidence.redacted.json及两份stderr记录保留真实参数/阶段与原始文件hash，原游戏control hex替换为逐块SHA256；未归档NSO、加工binary、shader/control/ASM。正常host/APK构建记录、rewrite manifest和link audit均完整保留，来源与去字节处理见archive-manifest.json。
