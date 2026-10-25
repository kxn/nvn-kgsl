> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Adreno原生深度纹理非比较采样整组资格

D16/D24X8/D32F × SAMPLE nearest/linear、FETCH+SIZE、SIZE、GatherR × 两种奇数尺寸/选定mip-layer × 更新canonical输入后同一job重放，共60次Draw、180次Submit、60次Finish、60份完整目标读回。188400输出通道最大误差1；794640保护字节、9747760只读字节及2592次占用检查通过。私有深度图像不CPU映射或转换；由原有CS untile→A2D upload→真实IR3 shader读取实际FDL深度布局。

正常工具记录含双san HOST完整60路径、故障/未知完成保活、原生4shader编译及API29真实KGSL严格链接、normal APK。基础owner两san各2031508断言/9owner/72故障owner，公共原始a767 SAMPLE和CASC GatherR+FragDepth有12组HOST元数据/生命周期检查，各8831断言。后者不是公共NVN像素或原游戏绘制证据。

普通应用APP_UID、fresh nonce和严格文件清单由现有runner核验，force-stop0及pidof1空输出证明测试后退出。保持独立KGSL/PM4/IR3/FDL路线，无Metal/Vulkan渲染器、CPU图像转换或游戏状态补丁。原始数据、正常记录及本次实际构建的34个源码均封存并逐ZIP成员核验。详见manifest边界；不增加已注册NVN入口数。
