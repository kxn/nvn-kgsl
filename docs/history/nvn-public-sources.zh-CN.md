> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# NVN 声明、行为与硬件依据分层（2026-10-23实际联网核对）

本次未取得无需登录的完整官方NVN API规范；不能据此断言外部不存在。公开重建声明确实比旧渲染实现更适合恢复ABI/枚举，且有大量可直接核对的PFN；但声明本身不证明默认值、record/enqueue时点、所有权或GPU同步。以下均不以Metal作为规范、模板或oracle。

| 层级/可访问来源 | 具体可用内容 | 版本与限制/本项目用法 |
|---|---|---|
| 官方入口：[Nintendo开发平台说明](https://developer.nintendo.com/home/development-for-nintendo-platforms)、[注册页](https://developer.nintendo.com/register) | 公开说明Switch开发资料在注册后还需申请并批准 | 未拿到完整NVN规范正文，不能假称已按官方API文档核过全部行为。若已有合法可用版本，应优先匹配实际SDK。官方公开的NVAPI文档是另一套API，不能混用。 |
| 与本游戏精确匹配的原SDK静态代码（本机用户原始sdk，hash/build-id见static.json） | 523 suffix名字及RELA方法表；实际ARM64参数/返回消费、字段getter/setter、默认常量、合法分支 | **本游戏固定SDK静态报告53.314**；这是本次Begin/End和Sampler defaults的直接行为依据。只静态读码，无CPU解释/JIT、无执行或改binary。单一release实现不等于跨SDK正式规范；无验证的非法参数行为不能提升为保证。 |
| Darksiders II DWARF重建C头：[公开代码浏览器nvn.h](https://code.botw.link/uking/uking/lib/NintendoSDK/include/nvn/nvn.h.html) | enum值、opaque大小、标量/聚合宽度、callback/PFN声明；MIT/h1k421来源注明于头部 | 文件自述覆盖53.105且有来自更新版DWARF的额外项。与本游戏53.314不自动完全相同。适合大量ABI候选与enum核对；缺失API或疑似类型必须用本游戏callee/caller验证，不能据头部版本直接填Device property。 |
| 公开重建SDK C wrapper：[BOTW FuncPtrInline](https://code.botw.link/uking/uking/lib/NintendoSDK/include/nvn/nvn_FuncPtrInline.h.html) | 大量实际具名C函数签名、参数顺序和pfnc转发；可核对const/outptr、float与聚合调用 | wrapper多数只是转调函数指针，不定义Sampler defaults、句柄寿命或同步时点。此文件单凭名字不能证明实际SDK函数存在或行为相同。 |
| 公开重建SDK extern表：[Odyssey FuncPtrBase](https://smo.monsterdruide.one/code/OdysseyDecomp/lib/NintendoSDK/include/nvn/nvn_FuncPtrBase.h.html)、[Cpp枚举页](https://smo.monsterdruide.one/docs/nvn__Cpp_8h.html)、[RawStorageClass](https://smo.monsterdruide.one/docs/structnvn_1_1RawStorageClass.html) | pfnc符号与类型的广覆盖清单；C++ wrapper/enums补充RawStorageClass等类别名 | 独立公开重建，非官方规范；Base头自带“确认文件存在性”的TODO。C++ enum名/序数可与本游戏W0返回交叉确认，不能据此虚构未重建C ABI。 |
| 独立RE API：[deko3d README](https://raw.githubusercontent.com/devkitPro/deko3d/master/README.md)、[Primer](https://raw.githubusercontent.com/devkitPro/deko3d/master/Primer.md)、[公开头](https://raw.githubusercontent.com/devkitPro/deko3d/master/include/deko3d.h) | 作者明确来源于对NVN的独立逆向；Maxwell寄存器、资源kind、descriptor、队列/fence等硬件知识，以及自有API的完整声明/示例 | deko3d是自己设计的homebrew API，并非NVN clone。可作为独立硬件/算法RE依据，不能直接把Dk参数、默认值、内存约束或句柄寿命当NVN合同；须与本游戏SDK/行为证据交叉核实。Primer自述尚不完整。 |
| NVIDIA公开硬件原始头：[MAXWELL_B clb197.h](https://raw.githubusercontent.com/NVIDIA/open-gpu-doc/master/classes/3d/clb197.h) | 官方公开0xB197类method、字段位与值，MIT许可，文件标明MaxwellB | 是硬件接口，不是NVN ABI/行为规范。可解码原Maxwell资料；不能把Maxwell包直接提交Adreno或写CPU模拟runtime。 |
| 社区一手硬件RE：[Switchbrew GPU Classes](https://switchbrew.org/wiki/GPU_Classes) | NVN使用的Maxwell/compute/DMA类，寄存器用途及bindless constant表等独立发现 | 给具体设备/版本的硬件观察，不等于完整NVN API规范。与公开class头及本游戏指令/数据流交叉确认，保留未知域。 |
| 目标GPU独立实现知识：[Mesa Freedreno架构文档](https://docs.mesa3d.org/drivers/freedreno.html)，本机已固定的Mesa26.2.5 Gallium/freedreno、IR3、FDL源及KGSL UAPI/内核源码 | Adreno PM4/寄存器、CCU/UCHE、原生资源布局、IR3编译、内核timestamp/BO合同 | 用来设计**目标Adreno**实现，不能反向定义NVN源语义。本轮没有阅读或接入Turnip/Vulkan实现/runtime；文档中的其它API介绍不用于移植路线。具体chip/内核必须实机资格，不能用另一代GPU成功替代。 |
| 独立测试（原SDK黑盒若可运行、当前native HOST和Android GPU资格） | 区分原SDK可观察行为与本实现是否正确执行，覆盖类型、寿命、失败/并发及像素/写回 | HOST RAM不执行PM4，不能证明GPU值；native GPU通过只证明测试域，不能自证未知NVN规范。当前Begin/End与Sampler默认值用原SDK静态方法证明；后端102checks是实现核验，未运行原SDK或游戏。 |

本次具体成果：直接SDK核验纠正了minFilter与minLOD两项默认值，且证明空/CPU状态-only录制合法。恢复ABI采用“重建声明 → 原SDK方法表+callee → 原caller消费 → typed signature检查”，实现采用“已确认NVN行为 → Adreno原语 → 独立HOST/GPU资格”。公开声明应继续系统补齐，不以一次旧实现返回值或一个API的相似性缩窄通用能力。
