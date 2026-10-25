> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原生 KGSL/PM4 故障定位：已有工具能力核查

结论：已实际阅读本机手册并执行离线 help，不能用现有帧捕获功能替代当前自写 PM4/IB 内部阶段定位。Snapdragon/RenderDoc/AGI 的 API 捕获入口要求被拦截的图形 API；本项目没有该 API 调用链。Perfetto 的系统事件和 GPU 日志仍可能提供辅助信息，但依赖设备真正发布的数据源/tracepoint/权限，**不能在本轮 no-ADB 下断言其一定缺失**。目前没有查到这些工具可直接从任意原生 KGSL 提交恢复 CP opcode/IB 当前 PC 的公开操作入口。

本轮未运行任何 ADB、未查询/连接设备、未安装 profiler service、未启动/重启 app/游戏、未改生产代码。help 是本机进程；PATH 前置拒绝执行的 dummy adb，且只运行 --help，不执行设备查询命令。SDP GUI 未启动，因为其启动流程可能自动枚举/部署设备服务；完整49页本机手册已解析核查，而非因不会用GUI就假定不可用。

| 现有工具 / 已核事实 | 对当前 raw KGSL/PM4 的覆盖和权限边界 |
|---|---|
| Snapdragon Profiler 本机2026.9包，`doc/Snapdragon Profiler User Guide.pdf`（49页，SHA见manuals.json） | PDF p5区分Realtime/Trace/Snapshot；p23 §5.2.2明确：不选GPU API可做系统Trace，但无GPU metrics；p36 §5.3.2要求Snapshot选择GPU API。该Snapshot是帧/API drawcall replay，与KGSL内核故障snapshot不是同一功能。支持的GPU API为GLES/OpenCL/Vulkan/DirectX，未见raw KGSL ioctl/PM4/IB-PC入口。Realtime硬件计数/系统Trace仍可能用于负载或调度，不能据“有GPU trace”推导能给故障PC。非root API追踪要求app INTERNET；Snapshot还列WRITE_EXTERNAL_STORAGE，Vulkan trace要求debuggable并从Profiler启动。没有以增加这些权限冒充获得CP寄存器读取。 |
| AGI 本机3.3.3 build909，SHA5f97b4fd…；实际`gapit trace --help`、`gapit perfetto --help` | capture API选项实际为`vulkan, angle, perfetto`。Frame capture是Vulkan或GLES经ANGLE，不能截获本项目不存在的API。Perfetto模式是系统trace/metrics入口，不是额外raw PM4解析器。[Google官方Frame说明](https://developer.android.com/agi/frame-trace/frame-profiler)、[System概述](https://developer.android.com/agi/sys-trace/system-profiler)。需要目标受支持的设备/驱动数据源，debuggable/profileable与具体收集权限按平台配置；本轮未重新做设备资格查询。 |
| Perfetto 本机`trace_processor --help`及AGI附带recorder的`--help` | 实际有`--query/--query-raw`查询服务已注册数据源、`--config`收集以及离线SQL/metrics分析。原recorder文件无可执行位；仅在ignored目录复制同hash文件加执行位后读取help，未修改安装目录或连接服务。官方列`linux.ftrace`、`gpu.counters`、`gpu.renderstages`及`gpu.log`；来源必须由内核/厂商producer实际提供。已有采样/调度trace不会自动产生PM4包PC；`gpu.log`若厂商恰好发布fault信息，则可辅助，但文档未保证包含CP PC或能绕过snapshot权限。[官方GPU数据源](https://perfetto.dev/docs/data-sources/gpu)、[服务/ftrace依赖](https://perfetto.dev/docs/getting-started/system-tracing)。 |
| Arm RenderDoc 2026.5，本机README、40页User Guide及`renderdoccmd --help` | README明确OpenGL/Vulkan graphics API debugger；手册是帧capture、draw/copy/clear资源与shader分析。CLI capture/inject/replay没有raw KGSL submit/CP fault snapshot命令。Android需developer/USB debugging，标准Android捕获要求debuggable；即使满足也不会把裸ioctl变成受支持的API调用。[上游Android说明](https://github.com/baldurk/renderdoc/blob/v1.x/docs/how/how_android_capture.rst)。其Arm计数增强不是Adreno CP错误寄存器访问。 |

## 已有设备事实与仍未核的部分

由root本次实际设备诊断提供（本子任务未复测）：dmesg权限拒绝、`/sys/class/kgsl/kgsl-3d0/snapshot` Permission denied、`/sys/kernel/debug/kgsl`不存在、普通logcat无故障PC。不能把这些事实扩大成“Perfetto服务一定无GPU日志”：那是不同访问通道。若root希望再核已有厂商通道，可在**不启动负载**时一次查询设备现有Perfetto producer/data-source及允许的KGSL/fault tracepoint；本机help已确认查询能力。不能为此改root/SELinux、安装绕权服务或把API跟踪层套在原生后端上。

## 最小补充的合理边界

当前缺的是“本次自有IB实际到达哪个已知包边界”，不是FPS、性能采样或通用trace可视化。可补充极小、只用于诊断的CP写progress word：在少数语义边界写入不同标记，沿原submit refs保活，在失败/超时后按现有安全cache/owner合同读取；不得改变未知提交的保活规则。它只能证明成功执行过的标记，**不是故障CP PC/opcode快照，不是GPU计时器，不是profiler结果**。因此不会重复实现这些工具已有的FPS/统计/帧调试功能；如果后续获取真正fault snapshot，则优先用现有工具解析，并用此标记仅补无法覆盖的阶段定位。

本机路径、PDF hash、实际命令/返回码/stdout/stderr见`manuals.json`、`help-checks.json`、`additional-help-checks.json`；提取文本按PDF页号保留在`snapdragon.txt/renderdoc.txt`。help退出1/2是程序打印usage的返回值，日志确有完整选项，不被误记成捕获成功或工具完全不可用。


## root 后续实际设备查询

同轮root在空载状态实际执行 `perfetto --query`：已注册 linux.ftrace，未注册 gpu.log/gpu.counters/gpu.renderstages；该查询不是trace，不足以否认全部驱动tracepoint。dmesg和KGSL snapshot权限拒绝；没有取得故障CP opcode/IB PC。可核查的脱敏结果与原查询SHA见 kgsl-fault-tool-device-query.json；不归档其它应用的producer/PID或track_event类别全文。未自写计时/采样/profile替代工具。
