> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# D2R bridge 观察与下一 Queue 合同（静态）

本轮 tracked 仅 host_bridge.c、tests/d2r_bridge/{real_driver.c,observations.c} 及独立 test_probe_provenance.py。没有修改 core/builder/runner，没有 ADB、git、游戏执行或 Vulkan 实现读取。原 NSO 只读、RAM 解压并逐段 SHA 验证；未保存新的解压游戏镜像。下列 PC 均是原模块的离线地址，不是新日志观测到的 guest PC。

## 已冻结桥

host_bridge.c:129/136/143/150/157 分别观察 WindowOriginMode、DepthMode、PoolBuilderFlags、PoolBuilderStorage、PoolInitialize；:169..173 用 public PFN 初始化核验 C 类型；:181..185 使 bootstrap/GetProcAddress（包括 NULL device）都返回这些观察包装器。每次先记录真实参数并 fflush，再由既有 typed forward 进行 before/lookup/after sticky 检查。没有清 error，没有改成功返回，PoolInitialize 原 NVNboolean（包括合法 false）原样返回；错误后不记录成功值。storage 仅打印地址和 size_t，不解引用；日志明确 guest_pc=unavailable，不用 C LR 冒充 guest PC。

测试 proof.json：三个 host 测试均 Clang -fsanitize=function -fsanitize-trap=function 通过；NDK29/O2/Wall/Wextra/Werror 编译 AArch64 对象且保留 swforce。新增 observations.c 的 5 个 API × missing/before-sticky/after-error =15 个 fork 子进程逐一 Exit78，观察先于 fatal；检查 64 位地址/size、全32位 flags、枚举、0xa5/0 返回不变。real_driver.c 使用真实 core + host-only 内存 backend，检查 cached 同 storage 初始化/映射和 0x22 ENOTSUP 截断。没有 GPU/guest 执行；不能替代真机资格。

## 原 caller 的确定参数与所有权

`dm/gfx/nvn/nvn_slots.tsv:38..46` 给出加载槽到名字映射，原 main 的间接调用点：

| API | 原 main call PC | 静态实参 |
|---|---|---|
| SetDefaults | 173d3cc | stack builder sp+38 |
| SetFlags | 173d3e0 | 0 |
| SetDevice | 173d3f4 | 当前 device x23 |
| SetComputeMemorySize | 173d408 | 0x40000 |
| SetCommandMemorySize | 173d41c | 0x10000 |
| SetControlMemorySize | 173d430 | 0x4000 |
| SetCommandFlushThreshold | 173d444 | 0x10000 |
| GetQueueMemorySize | 173d454 | 返回 size 保存 x22 |
| 原 allocator vtable+10 | 173d470 | size=x22, align=0x1000 |
| SetQueueMemory | 173d48c | 同 pointer、同 size |
| QueueInitialize | 173d4a0 | queue=ctx+48 |

caller 将 backing 指针保存 ctx+2048（173d474）。`main+1743eac` QueueFinalize 后，`1743eb0..1743ec0` 以 ctx+10 原 allocator 的 vtable+28 释放同 ctx+2048 指针，随后清零；所以 Queue 不可直接 free caller 内存。实际 size/地址/初始化是否成功仍待真实调用观察；表内常量不是实测。

原 main SHA256 55fd909bb7230b002357e5b0d8499c11f06888f345c49e69eb4734df44a27d0e；原 SDK SHA256 1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017。段 SHA、RELA、方法表 index/entry、选定函数范围 SHA 在 queue-sdk-proof.json；queue_sdk_audit.py 可离线复核。

## 原 SDK：预算并非三个数字的和

- `sdk+4711a0..4711b8` SetDefaults 保留 device+8，清 flags/显式字段标记/内存指针/大小，priority=100。没有把默认预算直接写成非零字段；GetCommand/Compute/ControlMemorySize 在 `471590/4715a0/4715b0` 返回字段并返回“显式设置”字节。默认解析路径给 command=0x10000、control=0x4000、compute=0x40000。
- `sdk+471470..471568` GetQueueMemorySize：cmd+control 加一个 4KiB 向上对齐的内部开销；开销依赖 device 的 +8c/+94/+98、+e0/+e4、+1af4 及 flags 的 bit0/bit1，并含 compute 分块预算。compute 项使用 divisor=(0x600 + 0x200或0x700) 计算块数；随后分配每块相应区。**不能宣称 0x54000 是原 SDK 对本机 device 的真实返回值**。旧非Vulkan hle_nvn.c:667..671 的简单和仅是旧 Metal 自己的策略，不是 SDK 合同或本后端已经正确支持。
- `sdk+471220..471460` 初始化校验：显式 command 对 property0x50 粒度取余并 >=0x51；control 对 0x5d 粒度并 >=0x5b；compute 对 0x53 粒度且非零时 >=0x54；非零提供内存 size 和 pointer 均4K对齐，size 必须 >= 上述计算总数；显式 flush >= property0x56 且 <=commandSize。
- 原 SDK `46afc0` 的这些 property 值取自 `45b0a0` 输出，所选公开 wrapper 并未包含直接常量；本范围没有核实实际数值，不可据名字编填。不为查这些值执行原 SDK GPU driver。
- `sdk+471630` 初始化调用 `471220` 校验。非零 builder memory-size 才取 caller pointer；先把 control-size 区域交 `46ed24`，然后 pointer+=control-size（471730..471744），再交 command arena（471754..4717a8），再 pointer+=command-size（4717b0..4717c4），尾部交 internal compute/header 资源（471a44..471a84）。
- `sdk+471cd4..471d9c` command/internal pool helper：有传入 pointer 时标外部所有权，否则按4096对齐内部 allocation；MemoryPoolBuilder 使用真实 pointer、round4K size。command 调用 flags=0x12（471790），尾部 pool flags=0x21（471a4c）。`471b60..471cc0` 清理先 Finalize 内部 pool，只有“内部 allocation”标记才 free。原 SDK 的内部 flags 不表示游戏下一次必然通过我们 public resolver 请求这些 flags；它证明这些预算有真实资源用途，不能全部只存字段即声称实现。
- 显式 flush 在初始化被夹到 command-size-4（471778..471788），未显式设置时 command-size/8（47178c）。后续 submit/flush 消费语义本次未完整审计；不能仅保存阈值就宣称实现命令刷新。

## 建议下一最小实现/验收边界

当前 core 的 QueueBuilder 仅 Defaults/SetDevice，Initialize 独立 nn_queue_create；其余原 caller SetFlags/各预算/SetQueueMemory 尚无实现，所以预计静态下一个具名边界为 SetFlags，而非现有 Initialize 自动满足所有预算。需由真实日志确认顺序。

1. 固定支持 flags0、保留原 builder/device 弱引用安全和 Defaults 合同；检查完整 size_t 的溢出、范围/粒度，不能因原SDK内部32位存储截断而将 public64位输入静默截断。
2. 为 native KGSL 定义后端实际所需 arena 布局与预算消费者；GetQueueMemorySize 返回该实现真实容量，不必假造 NVIDIA 私有device字段/Maxwell internal布局，但 caller 同 storage、alignment、ownership、overflow、Finalize 和 capacity 行为须可核查。
3. 明确 command/control/compute 哪些在 caller arena 中实际使用，哪些映射为本后端的有界 bookkeeping/IB/资源预算，并测试耗尽/flush/retire；不能把参数全部保存却无限另分配。原0x12内部command路径涉及缓存/ownership，不能以 anonymous WB flags mask 成CPU_UNCACHED，也不能把 GPU 内存与控制内存统统忽略。
4. host tests验证 Defaults显式标记、weak device、所有权/同指针、范围/不足预算/溢出/失败回滚；GPU测试验证相同 caller backing 中实际提交与缓存可见性、budget达到边界、inflight不能重用/Finalize、flush效果。最后用原ARM64 game实际参数观察推进；probe通过不能算真实frame。

未确认：原设备相关budget字段值、完整queue flush/usage-counter/compute分段消费者、原caller allocation失败处理、真实game当前queue参数。以上均不以缺数据补成功stub。
