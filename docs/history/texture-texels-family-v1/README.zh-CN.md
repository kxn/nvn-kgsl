> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公开 Texture texel 读写与 cache 家族

本批新增TextureWriteTexels、WriteTexelsStrided、FlushTexels三个已知ABI入口，入口数346→349，并把既有ReadTexels(s)扩到37种canonical原始块格式的2D/array子集。沿用正常raw-block GPU kernel；CPU只复制代码/命令/64B参数，原pool/offset/GPUVA不替换，没有CPU像素格式转换或BC解码。每次成功返回前完成本次私有GPU搬运与cache可见性处理。

行为证据来自本游戏SDK53.314的明确方法/helper/PLT身份，摘要及指纹在sdk-facts-sanitized.json；不保存ASM/原字节。该release Read/Write直接使用CPU映射/layout helper，memcpy的导入符号已核定；不能推导它会自动等待任意未完成producer。本native子集先要求所有已知同backing/别名资源idle，busy、NO_ACCESS/UNCACHED数据传输、3D、LINEAR Write及未知视图仍拒绝，不作为NVN跨版本通用限制。

本release Flush/Invalidate忽略region及format/mip，按view所选整layer stride范围cache。实现不解引用unused region；不把旧RGBA/region限制当契约。外部分配器/未知ownership仍拒绝。

Caller内存优先借用实际覆盖它的原BO，或导入核验过的完整private/RW页范围；同BO真实不相交窗口允许，不能以不同GPUVA证明无别名。目的区不能覆盖live命令/control。未知Submit/Finish以及cleanup失败保留整个owner、queue、texture、pool及caller页；不会报告成功，也不允许caller复用仍在GPU访问的内存。

Root正常工具针对最终生产源码：两sanitizer各40002项新家族检查，原资源/LINEAR Read各49452项回归，以及block-copy开关0/1+实际完整API29 backend严格链接全部通过。HOST队列不执行指令，caller字节保持原样；不能据此宣称Texel API GPU通过。pending-gpu-matrix.json的156场景/312调用仍待新普通UID qualifier执行，先前公开Copy468不能继承到本族。
