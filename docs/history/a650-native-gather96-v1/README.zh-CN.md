> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原生 Gather96 真机批量资格与二维采样基础

Adreno 650 替换机（普通应用 APP_UID）直接执行 KGSL/PM4/IR3。四个 Gather 分量 × 原生绑定1/13 × repeat/mirror/edge × 两种 origin × 同一 job 两次回放，共96次真实提交和完成。完整读回1179648字节，196608颜色通道独立公式验证，实际最大误差0；983040保护字节和7470720只读字节严格相同。稀疏 descriptor 空洞、未消费资源不忙、在途资源/Program禁止释放均通过。结束 force-stop 成功且 pidof 为空。

本批 shader 是正常编译的原创 GLSL，使用原有独立 pipeline Activity/runner；没有伪造 Maxwell code/control/C1，没有执行公共 NVN 调用者或真实游戏模块，不代表 D2R 首帧。GLSL4.60§8.9.4 定义四邻点顺序、base level 和忽略 min/mag；HOST 公式在GPU前确定，误差上限预定1LSB，实测0。

同批合入的二维采样基础由实际 FDL 格式表区分22种未压缩 FLOAT、18种整数和8种BC；新增全mip fresh GPU转换 owner，不改 canonical 地址，无CPU像素转换。双san各5371501次检查、88个owner、原正常Program事实校验与Android API29严格链接通过。它尚未执行采样GPU资格，未开放公共采样路径，不能借用本Gather结果或旧颜色目标结果宣称22格式采样已通过。

ZIP 每个成员解压后核对长度及 SHA256。normal-records 保存原始正常生产/检查/构建记录；历史记录不会改写为新源码证据。后续按完整功能族接通采样、mip过滤和深度链。
