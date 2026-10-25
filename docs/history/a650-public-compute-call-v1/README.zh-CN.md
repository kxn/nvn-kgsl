> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公共Call中的原始四个计算shader：有效绑定重建

Adreno650、普通APP_UID、Android14。原始四个实际Maxwell compute shader经既有离线前端和正常IR3编译运行：24次direct Call、8次indirect Call，另8次独立ordinary prep Submit/Finish。共40次实际GPU提交及完成等待；16个父handle两次重放，16个源handle及源CommandBuffer在提交前释放，32次in-flight ProgramFinalize拒绝。

子命令按旧Program及旧pool录制，父命令选择不同Program及另一pool的UBO/SSBO。Call在入队时读取按命令顺序继承的CPU绑定元数据，调用同一个既有nn_cs_encode/indirect编码器重建私有job；没有转发旧烘焙job。GPU真正输出通过原有独立四核闭式oracle，524,288字节当前pool、524,288字节旧binding pool、524,288字节原command arena检查通过；有效输出9,440字节，UBO/间接参数/guards均通过。GPU有效绑定的证据是不同程序/不同资源的实际结果，逐case地址是公开绑定声明，不冒称PM4捕获。

复用既有Activity、result.json、安装与采集runner。正常validator完整通过；既有generic runner终于force-stop后，root另用shell-v2 pidof明确确认returncode1且stdout/stderr均空，并核对结果UID恰为实际package UID，见post-stop-proof.json。该跟进证明绑定原始evidence SHA，没有倒写旧结果，也不把无nonce的旧generic transport称为新的nonce资格。

HOST各sanitizer132,972检查、8组原始direct/indirect、16个重建jobs；实际间接参数页PROT_NONE，禁止CPU读取。52项资格metadata准入、正常CMake/API29真实KGSL严格链接通过。通用Draw投影/ordinary→首次Call回归和empty/state-only公开调用也通过；后者各san159核心检查及722基础检查。所有HOST结果均不计为GPU执行。

源recorder内有一个从未提交的state-only seed，满足当前普通encoder有限录制前提；这不是宣称NVN规范要求seed。间接参数的Clear/Copy/UpdateUniform在独立ordinary queue先完成，所以本结果不证明Call内copy→dispatch排序。Call中的Copy/private jobs、counter、wait及未资格编译profile继续在preflight拒绝。没有CPU shader执行、运行时编译、真实游戏模块或首帧。

默认OFF的--compute-call仅用于正常资格APK选择；通用生产Call支持扩展不增接口数，仍406个ENTRY。normal根源码证明、APK记录与真实结果已归档。
