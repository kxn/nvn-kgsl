> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 显式深度运行时与完整源码登记

本批把真实原生v2 FragDepth recipe接到Program-owned sidecar、shader/VFD验证、资源预检和Draw前附件/lateZ状态。正常原创color+depth及depth-only shader编译成功；D16/D24X8/D32F的HOST双san各9171检查、24编码job，defaultOFF各7检查、0job，真实KGSL API29严格链接通过。附件存储与Program/job在不明完成时保留；旧v1入口拒绝裸v2。**没有深度GPU写入/测试/读回或公共NVN深度资格。**

正常stage并集合并原main与真实首个CASC程序：8个完整container、7个unique stage、4个已正常链接native pair（7次原attempt，3次原失败保留），不创建跨目录配对。87检查/29负例、实际完整源码查找与双san通过。code/control/C1保持原始完整输入，未裁剪、伪造或改游戏shader。

正常pipeline producer默认仍拒绝depthv2。显式defer-depth-recipes只登记已编译完整source及实际查询依赖，发布3个已有color recipe，不发布那1个depth Draw recipe。真实public ProgramSetShaders/reader HOST检查遍历4pair，完整source/control/C1任一变动拒绝、失败不改原Program、source pool可释放；对延后pair即使HOST负例满足全部recorded query仍无Draw recipe并拒绝。HOST查询fixture不是生产runtime查询答案；这项不是原游戏shader实际执行或新游戏里程碑。

shared pipeline运行时变化后，重新正常生产Gather资格/构建APK，独立新进程再执行96次真GPU提交，196608通道误差0；完整raw、所有保护/只读检查及strict-stop再次通过。这是回归，不新增Gather能力或NVN入口，也不把它计作depth GPU资格。

normal-records与完整raw ZIP全部逐成员解压核对长度及SHA256，原记录保留。实际游戏最新仍无Draw/Present/第一帧。后续按functional-families.zh-CN.md并行推进采样、mip与深度完整族。
