> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

正式root正常HOST/NDK及同签名APK重建后，普通APP_UID、pipa/Adreno650/Android14实际完成18case/96Submit/36Finish/18同owner重放/36全BO读回，6946816B零差异。其中active1220688B、canonical readonly1998848B、native guard/padding3727280B；metadata readonly1474176B、72次inflight destroy拒绝、96 completion字检查。无保留资源/cleanup error，runner force-stop并严格确认退出。

生产nn_volume_shadow_*实际调用，6格式RGBA8/RGBA16F/BC1RGB+RGBA/BC2/BC3、三种3D形状、多mip、非零selected mip；每mip独立immutable UBO/IB。测试CPU只填原始输入和a5 guard基线(native初始化4947968B)，不生成转换结果；生产owner constructor仍不map图像。全部原始读回压缩并逐hash重验。

本证据是canonical→actual FDL native sampled storage的GPU转换和owner回收，nvn_api=false/sampling=false/game_shader=false。不是3D texture-unit采样、ImageStore、原Maxwell/D2R shader、游戏Draw或显示帧；typed3D consumer另在推进。候选HANDOFF里的“预期/待root”保留历史原文，以本ROOT文件及actual evidence为新增实测。
