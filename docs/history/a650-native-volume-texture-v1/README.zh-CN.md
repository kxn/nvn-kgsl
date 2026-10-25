> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Adreno650真实3D纹理单元：TXF与TXS资格

普通APP_UID、Android14。6格式RGBA8/RGBA16F/BC1 RGB/BC1 RGBA/BC2/BC3，共18形状/视图案例，两次数据变化重放、TXF与TXS两种原创shader：96次实际GPU materialization加72次Draw，共168次Submit、72次Finish，144次in-flight销毁拒绝。collector整体verified，finally强停并确认进程退出。

全72原始读回共14,581,760字节：canonical/native完整比对13,893,632字节，目标guard574,368字节，113,760颜色通道。TXF最大误差1、TXS0，许可误差1。命令/代码/UBO等只读元数据7,666,656字节、固定完成字192次检查通过。ZIP逐文件重新解压核对长度及SHA256。

使用已有GPU搬运kernel、原生volume owner、实际Mesa FDL布局以及正常NIR/IR3编译的原创shader。CPU只初始化测试输入和guard；无生产CPU图片转换、BC软件解码、旧renderer或游戏shader替身。实际3D texel fetch/尺寸查询成立，不等于公共NVN已接受3D、不等于过滤3D、storage image或真实游戏出帧。

正常主机检查每套sanitizer45项（4个失败/保活案例，含未执行GPU不能通过及时间戳目标邻域损坏拒绝），34个独立oracle/transport/CLI检查；真实API29 KGSL/FDL/Activity严格链接通过。正常现有签名APK和runner入口均附记录。

保留三次失败及其修正事实：v1测试漏传必需timestamp输出，实际KGSL在任何提交前EINVAL；修正调用及HOST mock的参数契约。v2/v3真实完成一次materialization/Draw后，测试错误地把GPU时间戳字当固定packet value3比较。v3保留原始413,696字节失败读回和差异offset45056；独立部分诊断确认首个case的canonical/native及4096颜色通道严格相同，但未把该失败资格改写为成功。

原始Adreno PM4 XML明确CP_EVENT_WRITE bit30选择硬件timestamp，当前graphics producer正设置此位。最终检查仅允许经原packet地址/边界验证的4B写入目标变化，其他每个字节继续只读检查；fixed compute完成字仍精确比较。动态目标72字单独记数，不冒称固定值检查，也不采集/比较时间值作为性能数据。未修改production PM4、编译器、shader或FDL以迁就测试。
