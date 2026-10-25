> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 与源 NVN 语义分开的动态 Adreno pipeline

正常编译入口新增 shader-only recipe 和 device-only platform 两个目标，继续使用已有 Maxwell前端 → SPIR-V → NIR/IR3，不另写翻译器。原输入 code/control/C1 及生成 shader 保留 ignored；归档仅保存哈希、资源元数据和复现记录。正常新构建包含5个独立stage、3对VS/FS，所有native机器码均与同轮stage archive逐字节一致。

新的runtime使用实际FDL view、动态RT尺寸/viewport/scissor、32项attribute/stream、RGBA8/float VFD及direct/index/indirect/count命令。完整shader code、C1、UBO、descriptors、job和target BO在去重资源表中返回，未知完成不得退休。它不读取或扫描旧nn_gfx_image，不限定64×64目标/2×2 source，也不把离线shader身份当作游戏场景判断。当前仍是有限A650 RGBA8单层单mip，无深度/UBWC/MSAA或未知shader功能。

source_prepare用完整source+control及code/C1边界确认不可变archive身份，不保留caller指针、不选择首个variant；实际query选择延后到Draw快照。query键含类型及handle/cbuf参数，未知值明确拒绝。C1全量上传，原CB1内部数据与API UBO分开。现已核的color/sampled输入不能推出所有guest UBO→source CB或任意T10句柄映射。CB0 support语义和另一对C1 shader的未知handle仍是实际接线缺口。

正常HOST检查两套sanitizer各通过591720个动态编码/所有权检查和942个query/源身份检查，实际Android API29 KGSL严格完整链接通过。记录见 dynamic-pipeline-host-proof.json。本提交尚未接入公开NVN Program/Draw主路径，也未执行这套新pipeline的GPU资格；不能计作新增NVN调用或真实游戏Draw。source-only Program接线和raw-native多尺寸资格分别并行推进。

## compute 状态与资格诊断

公共Gallium fd6_compute.cc/fd6_program.cc及a6xx.xml证明SP_UPDATE_CNTL的CS_UAV重置位缺失，正常producer现补bit6。新正常compiler重建4个原CS与2个原创转换kernel：机器码/汇编不变，每份PM4仅word37从0xbf变0xff，见 pipeline-cs-uav-normal-continuity.json。实际consumer v2仍EDEADLK，不能把该位当作已经解决死锁；它有独立的状态合同依据，保留该修正。

已有工具能力和设备访问限制见kgsl-fault-tool-audit.zh-CN.md。诊断仅在显式 --texture-bridge-diagnostics 编译：每wrapper在现有Normal-NC owner内部额外保留一个进度DWORD，少数命令边界执行CP_MEM_WRITE。该BO仍在原提交/退休账本中，未增加隐蔽GPU地址；只在已确认accepted而Finish未完成时读取，在提交歧义/正常退休后不观察悬空指针。记录既不是故障PC也不是完成/计时/采样证据。正常D2R/CLI构建强制关闭此选项。

带标记的v3实际4submit/4Finish、4份64KiB GPU输出通过：65536 active+65536 canonical Buffer alias通道、131072 guards、311296 readonly字节、最大像素误差1（既定容差），资源释放与进程停止确认。但关闭标记的v4又复现Finish35、cleanup16、raw0。分别保存诊断成功和正式路径失败，**正式混合消费者仍未修好**；不能保留marker作为同步修复，也不能用诊断成功覆盖失败。主机marker包/地址/owner检查和实际Android诊断backend链接均通过，见source-consumer-diagnostic-host-proof.json。


后续按公共fd6_query sentinel/event合同，在正常A6 producer的sequence0写后加入CP_WAIT_MEM_WRITES。完整正常重建6kernel，机器码/汇编/程序元数据不变，每份PM4只增加一个包；原4CS新产物8次真实NVN间接派发与重放通过并单独停止确认。uninstrumented consumer v5仍EDEADLK35，无像素；这项排序修正仍不能称作混合路径根因解法。所有结果与差异见对应summary及pipeline-sequence-wait-normal-continuity.json。

独立原SDK审计进一步确认graphics API UBO0..13→sourceCB3..16（五个graphics stage），deko独立API实际是+2；不能以相似API替代NVN合同。sourceCB槽、public API索引及nativeIR3binding分别映射。完整方法表/RELA/字段和compute例外见uniform-binding-sdk-research.zh-CN.md。
