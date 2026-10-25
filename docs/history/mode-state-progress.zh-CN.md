> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原点／深度模式：原生命令与 A650 验证

工作树 `nvn-turnip-study-20261023`，分支 `codex/nvn-turnip-study-20261023`；只用新机 `DEVICE_SERIAL`，Adreno650 `0x06050002`。没有接入旧 Vulkan renderer、CPU 模拟器或新的游戏逻辑。

## 已实现的闭包

四个已有 typed PFN 已接入：DeviceSet/GetWindowOriginMode、DeviceSet/GetDepthMode。设备默认 lower-left／near-minus-W；只接受各自两个枚举值，未知值拒绝且不改变原状态。每次 Draw 记录当时的设备模式；普通绘制的即时编码和采样绘制的延迟编码都使用该快照。

编译产物 opt-in `--modes` 生成 schema3，明确声明四个 scalar patch 的位置、寄存器、原值、mask 和两个候选值：viewport Y scale、Z offset、Z scale，以及 clip half-Z。运行时逐项验证声明，拒绝重复、越界、错误 packet/header 和与 GPU 地址 relocation 的重叠，只修改新提交私有命令副本。它不搜索任意命令字，更不改 shader 机器码。

原始 VS/FS 机器码与先前已通过 A650 的产物逐字节相同。原默认 CLI 仍输出原 legacy 产物；legacy1/2 的固定 +Y／half-Z 语义不被冒充为完整 NVN 默认模式。NVN 图形注册现在要求 schema3。编译工具仍仅使用独立 Maxwell 前端和 Mesa common NIR/IR3/FDL，未链接图形 API 驱动。

## 真机证据

三轮都是普通 Android APP_UID，KGSL 实际提交、等待完成、读回和安全清理通过：

| 记录 | 实际覆盖 |
| --- | --- |
| [颜色绘制](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-modes-color.json`；未随公开仓库提供） | 4 组原 VS/FS 用例，65536 通道，走即时编码路径 |
| [CPU cached 纹理](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-modes-cached.json`；未随公开仓库提供） | 原 5 次提交 + 12 组模式用例，走延迟编码路径 |
| [CPU NO_ACCESS 纹理](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-modes-noaccess.json`；未随公开仓库提供） | 同上，另保留 GPU-only pool 的合法 NULL Map、cache NOP、范围和身份检查 |

模式用例是 origin0/1 × depth0/1 × clipZ(-0.5,+0.5,+1.5)，W=1；仅改变声明的输入矩阵常数和状态，使用同一原始 shader。两轮各完成 12 次提交、12 个独立录制／释放句柄、196608 通道检查。每次录完立即将设备设为相反模式，验证提交仍使用原快照。

两轮都得到 shaded18432、discard6144、depth-clipped24576，裁剪 case mask=`0xb2c`。runner 对实际保存的 12 份原始 RGBA 另做独立检查：两种原点严格垂直镜像；正 Z 两模式以及 near-minus-W 的负 Z 输出逐字节相同；near-zero 的负 Z 与两模式 far Z 全部是指定 clear 值。没有在 CPU 翻转或补画 GPU 输出。

原四采样用例的 65536 通道和独立 pool 身份诊断 16384 通道仍单独计数。NOACCESS 的 16 字节输入只在建池前初始化，此后不读写其 payload；GPU 经原始 FS 采样至另一 cached render target。所有 readback 随证据保存。

## 可复现性与边界

APK 构建记录现在同时覆盖源码、工具、生成输入及产物，并检查源码／工具在构建前后没有变化。平台 android.jar 和 apksigner 实现 jar 也纳入工具指纹。部署前逐项验证及比较完整源码清单；既有证据路径拒绝覆盖。六项 mock provenance 测试覆盖构建中变化、部署前七类损坏及新增源码，全部在任何 ADB 前拒绝。最终工具检查见 [记录](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `mode-state-final-tools-proof.json`；未随公开仓库提供）。

Host-only 测试还覆盖两种图形 profile 的四种命令状态、metadata 负例、同一录制中两个 Draw 的不同快照，以及 typed bridge 的 fatal/sticky 边界。正式 `check_device_modes.py` 可从原 checked artifacts 重跑 [90/116 项快照回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `mode-state-reproducible-host-proof.json`；未随公开仓库提供），不依赖硬编码 checkout 路径，不跟踪原游戏 shader 数据。它们只证明 CPU 侧合同，不计作 GPU 或游戏通过。

当前图形资格仍限于 64×64、完整 viewport/scissor、depth range0..1、无 depth attachment/cull、无 FragCoord/front-face 消费者。此次证明颜色输出位置和 clip-Z 可见性，**不证明 depth buffer 写入／比较、任意 viewport、完整光栅状态或真实游戏首帧**。

独立 AHB 出图与窗口重建的既有证据继续有效；这些模式测试本身是离屏 NVN 资格。真实 D2R 使用原 NSO 统一改写和原 ARM64 loader/HLE/gates，独立记录下一处真实边界，不与测试图案混计。

## 真实 D2R 已推进的边界

[本次原 ARM64 运行](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-modes-bootstrap.json`；未随公开仓库提供） 实际传入 origin1、depth1，两调用均通过。随后同一 caller storage 的 `0xa1` pool（536870912 字节／512 MiB）与 `0x64` shader pool（10485760 字节／10 MiB）都 Initialize 返回1；下一池 `0x22`（CPU_UNCACHED | GPU_CACHED，419430400 字节／400 MiB）在 Initialize 明确 ENOTSUP，typed bridge 退出78。见 [实际参数日志](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-d2r-modes-bootstrap-stderr.log`；未随公开仓库提供）。没有修改原预算、flags、游戏流程或版本分支。

完整本机原始 RomFS 与设备复用内容逐文件验证：46文件／8973547258字节，未重拷贝或使用虚构数据。原 SDK 初始化、constructors 和 nnMain 经原 loader/HLE/宿主门执行；本轮新增平台侧代码是独立 NVN 模式与观察接口。host 使用普通 UID 的 run-as CLI，**尚非 Activity 中的真实 D2R 出图，也没有实际游戏 Draw／首帧**。本次 host 已确认退出，未发送终止信号。

下一步是 CPU_UNCACHED 的真实映射或具有证据的等效可见性方案。现 caller pointer 是匿名 WB 存储，不能仅改 KGSL flag、返回另一别名、替换 heap 页或复制 shadow 后称为已实现。队列 caller-memory 与通用 Window 的生命周期工作可并行；其成功与本轮离屏模式资格分别验收。
