> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Gather consumer 与真实深度配方接入

本批为正常源码/构建、HOST 与实际 Android ARM64 严格链接证据，没有新的 GPU 或原始游戏执行结果。公共清屏536和公共GOB22目标132的真实GPU结果另见相邻档案，不能移作本批资格。

Gather 使用实际 NIR tg4 的 operation16 与独立 count/component/sampler/offset 事实；保留 source resource.other，要求它与实际 Gather 数量一致。运行时通过既有描述符快照区分 native binding1 与公共 T10/S3，覆盖重注册、旧快照保活和公开 Finalize。新宏默认关闭，当前仅既有 RGBA8 normalized2D/singlemip/nooffset 域；整数、BC、BUFFER/3D等不继承资格。24个 producer 负例、defaultOFF/ON 双 sanitizer、真实KGSL两模式NDK链接通过；原18个类型负例及owner回归通过。结构尾部追加要求所有consumer按新源码重编译，不混用旧预编译结构。

原始 CASC code/control/C1 未改写，正常 IR3 导出真实 FragDepth 寄存器14和零颜色输出。新深度配方明确version2，并由同次编译生成指向确切配方的sidecar；不伪造DATA0、不删Gather或C1、不复制并改回version1。旧Program/shader入口在分配前拒绝裸v2。新49 DWORD助手只生成实际FDL附件及late-Z/LRZ-off状态，尚无depth Draw或GPU读回资格。

原始 shader 机器码、汇编和link元数据保持相同；三组旧0/1/2纹理的原生代码、配方与types元数据逐字节一致。正常 color archive producer 显式拒绝深度v2，在真正typed depth consumer接入前不发布错误颜色目录。

继续工作为真正depth Program/encoder/CP_BLIT读回、Gather真机邻域与分量验证，以及原D2R资源与调用链接入。最近真实游戏仍是v16，未到原游戏Draw/QueueSubmit/Present或首帧。所有ZIP成员已重新解压并核对大小和SHA256。
