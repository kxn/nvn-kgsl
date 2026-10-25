> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 预打包布局：限定BNTX8B实验与实际GPU搬运资格

普通APP_UID/Adreno650/Android14，公开NVN B2T/T2B/T2T及CopyCommands，72案例×两次同handle重放，144次实际Submit/Finish。RGBA8、BC3；BH0..5；128²/mip8普通2D和高形状/array3/mip4；partial/shared/奇数stride/邻域/inactive mip/layer均比对。高形状选择mip0，163个block rows真正使用BH5，不以自动缩小后的BH4冒充。

全144原始BO读回合41,328,640字节严格零差异：active8,288,352、unchanged33,040,288；另核对source readonly19,961,472及command/control readonly18,874,368。完整ZIP重新逐文件解压核对长度与哈希。无CPU image conversion/BC decode/探针raw submit或private encode；录制、复制、提交均经公共NVN入口。runner正常verified，finally强停并确认进程已退出。

仅接受公开writer实际生成的8B{BH0..5,0x10007}，其余字节明确95，不忽略packaged标志、不默用隐式布局。原BntxLibrary FileWriter/FileReader保持原源码实际编译，六个BH序列化/读回与该字节模式一致；独立作者地址函数比较1,426,560个numeric offsets。真实D2R v12观察到{2,0x10007}，仅构成关联，绝非完整NVN编码规范证明。

实验--bntx-packaged-layout-experiment默认OFF，正常Activity/CLI源码、有效选项和APK/host verify均记录/转发。source policy仍是既有Generic16Bx2/no-NVIDIA-memory-compression，target仅2D/2D-array、flags0；不扩大sampled consumer/3D/ImageStore/格式重解释。两san下ON371/OFF179 metadata checks，各72实际public admission/144非执行HOST submit与unknown owner保活通过；42oracle/transport负例及原234cases/468 HOST submit回归通过。normal ARM64真实KGSL严格链接通过。HOST数字不算GPU。

本资格只证明限定实验profile的硬件搬运互通，不证明真实游戏资产语义/取样/显示或D2R出帧。接着用未改写原始NSO按正常入口重新生成真实D2R，显式开启实验，观察v12阻塞是否被越过。
