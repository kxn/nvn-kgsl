> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始D2R ARM64 v14：越过packaged查询，推进到RGBA32F BUFFER资源

固定提交771812a，未改写原始3.3.94012 ExeFS经同一canonical入口重新生成，原host/HLE36个源码与独立后端合计69个实际编译单元。正常host/APK及部署前verify通过，ELF仅依赖m/dl/android/c，无旧renderer或CPU模拟器。46项原始RomFS共8,973,547,258字节在复用前全部核验。Adreno650、普通APP_UID上原始模块及nnMain实际执行；finally强停并确认退出，非timeout。

v12的BC3/128²/depth1/8mip/flags0/packaged {2,0x10007}现在GetStorageSize返回24064，alignment512，Initialize成功，后续mip的CopyBufferToTexture正常录制。BNTX8B仍是明确开启、默认关闭的实验profile，并非完整NVN编码规范。观察日志全程共17次TextureInitialize成功、444次TexturePoolRegisterTexture和10次CopyBufferToTexture录制。这些是日志条数，不是新增接口数或GPU完成数。

最新首个错误为TextureBuilderSetStorage(errno95)：flags0、target10(BUFFER)、format46(RGBA32F)、width65536、pool offset18002624。当前通用BUFFER构造器仅允许RGBA8/R32F，并固定cpp4，尚未与有限GPU消费资格分离；下一批应根据独立格式定义及SDK布局事实补齐typed BUFFER存储/对齐/注册这一类元数据，GPU descriptor与采样资格另行明确。

本轮没有真实游戏QueueSubmit/Draw/Present或首帧。游戏在退出前录制了复制命令，但不能将其计为GPU已执行。另有公共CallCommands24次实际GPU资格在本轮之前通过并单独归档，也不计为游戏绘制。

v13未运行游戏：完整构建被源码闭包校验拒绝，原因是预期清单漏登记已实际编译的pipeline_views.c。b2c1d96修复Activity/CLI两个正常入口的清单与差异诊断；没有减弱门禁。本轮完整69个实际源码与预期清单匹配，正常记录生成并用于实际部署。

仅归档构建、执行和资源元数据；不归档NSO、加工binary、shader/control或ASM。所有hex=代码快照（包括64个hex字符）递归替换为长度/SHA256，packaged8B布局元数据保留。Activity无waitpid，OS退出码仍unknown。
