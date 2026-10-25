> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公共 CallCommands：原始命令所有权、状态继承与实际 GPU 绘制

普通 APP_UID、Adreno650、Android14。公共 CallCommands 包装12个原始子命令，父命令绑定 UBO0，子命令不绑定该 UBO；子命令公开句柄在首次提交前释放。三种 VFD 布局、四种直接/索引/间接绘制方式、同父 handle 两次重放，24次实际 Submit/Finish 全部成功。393,216颜色通道、196,608 guard bytes、491,520只读字节均通过；独立 NumPy 颜色比较最大误差0。完整24个原始BO读回合589,824字节，ZIP逐文件重新解压核验长度与SHA256。runner verified，finally强停并确认进程退出。

实现以不可变有向命令图保留原 command pool/arena 身份，公开查找寿命与被调用命令所有权分开。按录制顺序保存明确的状态写入和资源绑定，在入队时投影有效状态并调用已有原生 Draw 编码、私有命令暂存、KGSL 提交及退休路径。调用批次预检和引用保留失败回滚，不忽略子命令，不使用旧渲染器对象、CPU图像转换或探针私有编码。

普通成功提交在首次 Call 前留下的状态也按顺序继承；失败提交不会发布尚未成立的状态，未知完成状态使后续 Call 拒绝继续猜测。该跨提交行为已由正常HOST用例验证，本次GPU资格直接验证父子继承、公开子句柄释放和重放。HOST每套sanitizer各116个公共核心检查及722个命令图基础检查，正常原生Draw投影检查、16个既有像素oracle负例与12个Call元数据负例均通过；Android真实KGSL与Activity严格链接通过。HOST检查不计作GPU执行。

目前资格范围为RAW原生命令片段及通用Draw的Call组合。计算、复制、计数器、等待及旧预编码job混合调用仍在提交前明确拒绝，不能把本结果称为全部CallCommands语义完成。原始真实D2R模块没有在本资格中执行，也没有游戏首帧；独立后端已有406个实现入口，其数量与完整行为资格分开记录。

正常复现入口：check_command_sequence.py、check_pipeline_call_draw.py、nvn_pipeline_qualification.py、check_nvn_pipeline_probe.py --call-commands、build_android_probe.py --nvn-pipeline-check ... --nvn-pipeline-call，以及既有 run_android_probe.py。构建记录、当前根源码检查及完整读回附在本目录。
