> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公开 NVN Draw → Adreno/KGSL 首轮真机验证

本轮在替换机 Adreno650 的普通应用 APP_UID 上，通过公开 NVN 的资源、ProgramSetShaders、绑定、Draw 和 Queue 接口完成24次实际提交/完成。三种顶点布局分别使用槽3、槽7、分离槽1/11；四种绘制方式为 DrawArrays、索引绘制、数组间接绘制、带GPU读取计数的索引间接绘制。十二个命令句柄各重放一次，重放前修改原UBO、顶点、间接参数及计数。393216个颜色通道与独立数学预期误差0，196608字节边界及491520字节只读输入检查通过；资源全部回收、进程严格停止。

实际链路为：原shader完整code/control/C1身份匹配 → Draw时按实际状态选择编译变体 → 原SDK已核的API UBO0映射到源CB3 → reflection确定native binding → 公共顶点/绘制状态映射 → IR3/Adreno PM4 → KGSL。SetShaders成功后立即销毁原source pool，随后绘制仍正确，验证代码身份和生命周期没有依赖悬空调用方存储。资格程序显式选择已知原始shader作为输入；生产选择从完整source identity开始，不使用资格程序的数学oracle、archive顺序或游戏地址。

这次资格范围是LINEAR64、RGBA8、无纹理的原VS/FS。它没有执行真实游戏模块，不代表GOB目标、C1纹理取数、全部NVN绘制状态或游戏帧已经完成。此前raw-native72例只证明底层编码器，本轮才闭合该子集的公开NVN接线；两者不互相替代。原游戏下一次运行必须从原NSO经正常规则重新构建并验证，不能复用过期构建记录。

NVN行为按独立公开声明、原SDK53.314静态事实和明确的后端子集实现。NVN属性/顶点流槽位上限按原SDK的16校验，硬件编码器的32槽容量独立保留。count0/短数组会禁用余下属性或流，不能当作保留旧状态。合法的宽泛格式/stride元数据可记录，实际取数能力在Draw处校验；原A650 stride字段12bit，当前四字节对齐子集最大4092。默认source cache仍关闭；启用后缺少typed资源、实际FDL view或精确变体均明确失败。

每次提交有独立硬件表、命令与必要shadow；CopyCommands/重放不共享可变job。代码/C1目前也每Draw重新分配和上传，是尚未消除的成本。按完整stage变体、linked pair和C1身份保留Device级不可变owner，可以减少重复BO分配、CPU复制和cache publication；需保持每次提交的可变表及target shadow独立，并按完成状态退休。本轮不声称已做该缓存或获得FPS收益。

`gpu-evidence.json`含正常APK记录、真实结果、离线逐像素复验与严格停止回执。`lossless-readbacks.zip`去重保存24份完整24576B pool读回中的3个唯一payload；`summary.json`逐项记录SHA与成员位置。解压后逐SHA重验，与原读回完全相同，没有量化或有损转换。

主机检查分别验证namespace/变体选择、真实录制/owner和复制/不确定提交保活，以及Android API29完整KGSL链接。HOST memory backend明确不执行shader；公开caller的HOST测试必须在QueueSubmit得到ENOTSUP并报告失败，不能冒充GPU成功。旧core夹具曾直接写过时内部flags，未设置真实属性/raster值，导致回归失败；改用公开typed setter后通过，生产校验没有放宽。失败证明与修复后的证明一起保存。

随后使用正常重建的66源文件Activity共享host（含36个原host/HLE源文件）及原始3个NSO运行真实D2R，已越过上一轮VertexStreamStateSetStride，新的首个错误是SamplerBuilderSetBorderColor ENOTSUP；实际三次原目标纹理初始化和PresentInterval2仍通过。没有观察到真实游戏Draw/Dispatch/Present或首帧，进程严格停止。脱敏结果在上级 `a650-activity-pipeline-draw-v4-summary.json`，正常构建记录随本目录保存；原stderr含shader控制字节，仅留ignored。
