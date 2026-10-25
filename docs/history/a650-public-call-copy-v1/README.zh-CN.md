> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公共 CallCommands 复制：924次实际 GPU Submit/Finish

原公共NVN录制的 B2T/T2B/T2T 在 CallCommands 中按真实语义每次重建私有code/UBO/IB/middle owners，未直接转发原已烘焙shader jobs。普通CP copy/clear/update、source metadata、row/image strides不可变快照、继承RT/scissor及private CS不污染Program/UBO/SSBO Frame均实现。复制支持的原格式/区域规则没有扩大；入口数量不增加。

新机Adreno650、普通APP_UID实际执行：array/mip234cases×2=468，true3D228cases×2=456，共924 Submit/Finish及924份完整raw。CallCommands分别234/228次，CopyCommands均0；所有输出、未写区域、源只读和command/control只读零容差通过。data检查共79,192,064字节（array27,541,504+volume51,650,560）。两个无损ZIP每项重解压核验长度/SHA。源handle公开Finalize及录制CB销毁后仍由Call DAG持有；585/570池import与release完全配对。原testRAM/backing未替换。无CPU执行shader/图像转换、BC decode、probe private encode或raw submit；真正执行均通过public NVN Queue。

兼容旧record的通用oracle scope字符串仍带CopyCommands字样；本批实际profile由call_commands=true及CallCommands_calls/CopyCommands_calls独立严格校验，不将旧CopyCommands GPU次数改名。两组最后force-stop0、pidof1且双空输出，资源正常退休，进程退出。不是Draw、sampling、display或真实游戏帧。

正常root核心双san涵盖462case×2replay、CP9操作/2继承RT clear/8freshCP、原4CS与mixed CP/Compute、预算/arena/unknown quarantine、BLOCK0/1实际KGSL strict链接。两独立连续BridgeClear测试额外发现并修复enqueue准备门错误：仅bridge_clear交回既有materializer，未知legacyDraw仍95；cache0/1×双san各131,198检查、0/16 offset、不同目标/颜色/region、每clear六owner、两replay及原RAM/pins/未知Draw原子拒绝通过。这个BridgeClear补充是HOST资格，未合并为924copy的GPU clear资格。

非literal copy暂每操作增加一个64B逻辑、实际page的patch BO；这是明确的分配成本，尚不声明性能收益。CopyCommands复制的Call DAG、counter/wait等仍有独立未支持边界。GPU资格工具默认OFF，原默认CopyCommands profile有单独正常回归；新profile计数及43类oracle/collector负例保持分开。
