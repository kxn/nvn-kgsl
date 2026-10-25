> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 实际运行时FDL目标：公开NVN真机资格

Adreno650/Android14普通APP_UID，原始source Program与public NVN Draw，三种静态表之外尺寸73×41、257×113、1153×641，三种vertex layout×四种direct/index/indirect/count Draw×两次同handle重放。LINEAR与canonical GOB各72Submit/Finish、36handles；均明确runtime_target_views=true、static_view_registrations=0。实际FDL由Device cache在Draw路径生成，资格metadata只供分配和oracle，不注册到Device。

两批各74,026,272颜色通道，全144张完整BO读回合170,557,440字节，RGB最大误差1（事前限定容差），alpha完全一致。LINEAR完整guard5,993,184字节，GOB完整guard16,511,712字节；涵盖行填充、GOB填充、末级分配填充和前后邻域。两批各1,474,560只读输入字节及13,865,616重放保留通道核验；无CPU image conversion，原输入/shader identity与生命周期不变。完整原始读回ZIP逐文件重新解压核对长度/哈希。

正常构建含实际KGSL、pipeline_views.c及FDL archive；Ninja实际编译命令核对两个开启宏，ELF无旧渲染器依赖。两次runner正常passed/verified，finally严格停止并确认进程退出。离线oracle每种29项，包括完整合成buffer正例及损坏像素/alpha/填充/边界/身份/计数/nonce负例；这些合成测试不是GPU资格。

保留首次LINEAR v1的失败原verdict：设备72次提交和像素检查已通过，但资格工具误用pitch×height作公共分配大小，遗漏末级行高及页对齐；其原check中已有实际FDL bytes。独立follow-up仅改为原有FDL分配元数据后，全原始读回核验通过，未修改原证据或放宽像素容差。随后正常生成v2输入、重建APK并完整重跑，得到本归档两个正式通过批次。该额外诊断72次不混入本批144次计数。

这证明公开NVN目标/绘制/重放的运行时元数据路径，不代表真实D2R Draw或出帧；当前真实D2R仍待预打包纹理布局接入。未新增公开NVN入口。
