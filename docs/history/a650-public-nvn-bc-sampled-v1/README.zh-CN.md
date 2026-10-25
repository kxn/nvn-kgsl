> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Adreno650：公开 NVN BC 采样实际验证

普通 APP_UID、pipa/Adreno650/Android14，实际公开 NVN loader/ProgramSetShaders/Draw/QueueSubmitCommands/QueueFinish 路径；KGSL/PM4、Maxwell→SPIR-V→NIR/IR3、原 FDL 和 GPU raw-block 布局转换，没有旧 renderer、CPU 图像解码/转换或游戏修改。正常 producer、source closure、原 stage 输入、NDK 整库严格链接、APK 和部署前 verification 全部保留。沿用现有签名 install-r，未清空 9GB 游戏资源。

24 次实际提交/等待和完整24576字节读回，12 个 handle 各重放一次。BC1 RGB/RGBA、BC2、BC3 UNORM，8²/37×21/127×65，共393216通道零差异、196608 RT guard 字节及1703936只读字节一致。注册在 EndRecording 之后；同 T257/S259 重新注册、另一 canonical backing、旧源 poison；36 次已注册 CPU 对象释放、24 次 caller binding array copy；Finish 后才修改/读回。最终无保留资源、cleanup errno0，进程 force-stop 并确认退出。

完整24份raw压缩归档并逐项hash复核，不只抽样像素。测试原 VS/FS 是声明0x880的有界 graphics profile；它不证明实际游戏caller完整0x898来源。当前资格是普通2D、mip1/layer1、nearest/clamp、normalized采样到LINEAR RGBA8 RT；不声称sRGB、array、多mip、真正3D、通用sampler、GOB RT或真实D2R帧。

HOST另有两san各6116累计检查/8格式与pool组合，每组合2次MOCK提交；30 collector负例及4构建互斥负例。第一次 enqueue CLI误把qualification record传作pipeline record而失败，保留ignored v1；修正输入、fresh v2后通过。APK v1未指定旧签名，仅构建未部署；fresh v2显式复用已安装签名后正常测试。没有倒写失败或旧记录。
