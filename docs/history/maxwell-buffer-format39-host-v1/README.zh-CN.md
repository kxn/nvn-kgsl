> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Maxwell BUFFER 查询的39个准确语义枚举

StrictAccessor 通过真实 vendored TextureFormat enum 验证39个canonical名字，保留实际 public NVN→shader enum helper、handle/cbuf身份、全原source/control/C1哈希。拒绝数字字符串、空白、错误大小写、未知值与NVN/FDL namespace，不能注入expected-query答案。HostSnorm仍false。

正常root前端重编后，实际enum/accessor385检查、39接受/31拒绝、7完整fixture负例、39原C1 FS前端/SPIR-V/IR3、双san与API29严格链接通过。原C1保留256B且为运行时CB1→native UBO4，不做CPU读取/folding；sourceCB3→native3独立。图形control是原先明确界定的0x880资格域，不声称等于实际游戏的完整caller域。

33个非SNORM特化全部SPIR-V/native code及link metadata逐字节相同；6个SNORM走已有SINT替代假设的normalization。编译成功不代表SINT alias已资格，native消费者保持拒绝。所有这些原C1输出的SPIR-V sampled type仍FP32，UINT/SINT的格式查询通过不代表已支持整数OpTypeImage。

合并原12种已声明specialization与39种新query输入，正常生产包生成51个stage requirements、188种VS/FS组合中的94个合法pair。保留全部精确source身份与实际查询；无archive-order fallback。provenance记录原normal package完整哈希、输入/输出指纹和typed事实，不归档原shader字节/ASM。公共Draw是否消费这些BUFFER特化仍有独立准入与GPU资格，不能据离线包报游戏帧。

共享前端的原4个full0x898 compute shader已正常重编，每项9个真实文件（共36个）重哈希后与原产物逐字节一致，未编辑旧记录。Mesa IR3/FDL/转换kernel未因本C#查询修改而重写；正常host后续必须使用新compute/stage/pipeline provenance。

此包GPU执行0；原始D2Rv15的BUFFER创建记录与原创RAW-NATIVE BUFFER实际GPU资格分别归档，不合并为本查询包的GPU或游戏证明。
