> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# NVN 行为公开资料增量审计

范围：仅公共作者资料/声明/硬件定义；没有读取 Metal/Vulkan renderer、CPU 模拟执行、ADB，未改生产文件。原游戏 SDK53.314 的既有静态证据只用作版本差异对照，没有在此归档 SDK 代码或机器码。以下行号以按 URL 下载的原文件为准，web 页面自动去空行会产生不同编号。完整下载身份见 files.json、supplemental-files.json；重建 SDK 文件只计算摘要，不保存副本。

## 1. 新增实质：open-ead 中存在上层 NVN wrapper 的行为重建

固定仓库 [open-ead/nnheaders@246c3b4](https://github.com/open-ead/nnheaders/tree/246c3b482d045934ed004f50c5ada37f3b10b703)。README 自述用户通过 RE/带符号或调试信息的二进制重建；不是 Nintendo 发布的 SDK 文档。根目录未查到统一许可证，不能把 nvn.h 的局部 MIT 许可外推到其他文件。

- [gfx_Texture-api.nvn.8.cpp](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/src/NintendoSDK/gfx/detail/gfx_Texture-api.nvn.8.cpp#L15)：SetupTextureBuilder 从上层资源用途生成 NVN flags，RT/depth 使用 COMPRESSIBLE，scanbuffer 使用 DISPLAY；linear RT 与普通 linear texture 查询不同 stride property。这些是可实施的调用组合/回归用例来源，不能据此证明 COMPRESSIBLE 必须压缩、DISPLAY 自动分配 AHB，或还原 raw NVN 对所有 flag 的约束。
- 同文件 CalculateMipDataAlignment/Size 委托 raw NVN getter，不能把上层 CalculateMipDataOffsets 的线性数据排列当成 GOB 资源布局。
- [gfx_DescriptorPool-api.nvn.8.cpp](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/src/NintendoSDK/gfx/detail/gfx_DescriptorPool-api.nvn.8.cpp#L19)：descriptor 大小/保留数量来自 DeviceGetInteger；SetTextureView 把保存的 texture/view 传给 RegisterTexture。上层 BufferView 则存地址与长度，其 16B 槽不是 NVN texture TIC、也不是 Adreno descriptor。可据此添加分域测试，不能混用槽格式。

这些 wrapper 是 nn::gfx ApiVariationNvn8 的重建，不等于 NVN53.314 release 的实现。没有给出每个函数的原二进制 build/hash 和逐函数验证，因此适合提出测试假设，不单独压倒本游戏 SDK 的相反事实。

[nvn.h](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nvn/nvn.h#L21) 明示 Darksiders II DWARF、53.105 子集且可能混入新版成员；可核 ABI/enum/opaque 大小，不能视为完整行为规范。现有聚合返回类型错误案例仍说明公开声明需要 callee/caller ABI 核对。

## 2. deko3d：确有行为文档与实现，但不是等价 NVN

固定 [deko3d@373bfd4](https://github.com/devkitPro/deko3d/tree/373bfd43d8b2d805fa2eb7d406dbb823032df958)，zlib 许可。作者 [README](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/README.md) 明确它不是 NVN clone；[Primer](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/Primer.md) 解释其 NVN RE 来源且文档未完成。

具体可用：Primer 的 memory cache、queue state、flush/fence、control/code 分离给出了明确的观察者与所有权关系，适合生成 CPU→GPU、GPU→CPU、跨 queue、重放与 descriptor 更新测试。Switch cache/驱动限制不能变成 Android/KGSL 必然限制；不能复制其等待实现或把 flush/invalidate 当成同一操作。

可复核的不等价反例：

1. [cmd_bind_common.cpp](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/source/cmd_bind_common.cpp) 的 dkCmdBufBindUniformBuffers 图形路径用 `2+firstId+i`；本游戏 SDK53.314 已直接核到 `3+APIindex`。native IR3 binding 又属于另一命名域。
2. [dk_cmdbuf.cpp](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/source/dk_cmdbuf.cpp#L35) 空 finishList 返回 0；不能据此否定本游戏 SDK 合法空 NVN recording 的句柄语义。
3. [dk_image.cpp](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/source/dk_image.cpp) adjustSize / calcLevelOffset / alignLayerSize 可交叉核 GOB mip/array 算术；ImageInfo::fromImageView 的 Dk 视图校验不能移植为 NVN GetViewOffset 限制。我们刚完成的原 SDK offset 审计已证明地址查询与采样视图资格需分开。

## 3. 新找到的独立作者 NVN 实证记录

[ScarletNX docs/NVN_HLE.md@fe91f37](https://github.com/scarletnx/ScarletNX/blob/fe91f375bb2088230997553a54e5508483007346/docs/NVN_HLE.md)，GPL-3.0-or-later。只读这份语义调查文档和 README/许可，没读其 CPU、Horizon 或 Vulkan 实现。它是另一项目的作者报告，不是官方规范；文档明确 Catherine/TotK/Odyssey 的观察不能推广到全部 SDK。

有用增量：记录版本间 view/builder opaque 布局变化、按值复制对象、shader code/control entry 步幅；独立记录 CB0 driver、CB1 embedded、CB2 handles、CB3+N application UBO。还指出一些游戏内联写命令/提交期追加包，单按导出 API 统计不能证明命令覆盖，提交后读取复用地址也可能看错资源。

可实施为审计/测试清单，尤其 opaque memcpy、数组步幅、driver bank 分域、record/enqueue/execute 读取时点。其原游戏输入 hash、具体 SDK release 和逐观察原包未随文档全部公开；不能直接据其 offset/宏槽建立本游戏 ABI，不能声称代码谱系独立已获证明，也没有从其 renderer 导入行为或策略。本文未复制实现。

## 4. 硬件资料的适用层

- [NVIDIA Shader Program Header](https://download.nvidia.com/open-gpu-doc/Shader-Program-Header/1/Shader-Program-Header.html)：官方明确 SPH 80B、各阶段字段/kill/store/MRT 等硬件意义。可指导源 shader metadata 验证；不包含 NVN control 容器大小/生命周期或 IR3 ABI。网页 intro 与 SphType 枚举表文字存在相反映射，遇到该处仍需具体 class/已核 shader 数据交叉核，不盲从一句文字。
- [NVIDIA B197 class](https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/clb197.h) 与同仓库 clb197tex.h 是 Maxwell methods/TIC/TSC 的第一方字段定义，MIT-style 文件许可。能核源语义，不能当 Adreno packet/UBWC/FDL 定义，不能把 TIC 的 32B 直接变为 Adreno descriptor。
- [envytools Tesla texture 文档](https://github.com/envytools/envytools/blob/f102b82381f3f11cee113d16374c87091db039d9/docs/hw/graph/tesla/cuda/texture.rst) 含 fetch/LOD/derivative 行为解释但很多 TODO，且是 Tesla，不是 Maxwell NVN 的完整纹理语义。不能给当前 C1 未确定 selector 赋类型。
- Switchbrew GPU_Classes 可作为社区 method/driverCB 域算术辅助；不新增将其提升为官方 NVN 规范的结论。Qualcomm backend 仍从已核 NVN 可观察语义产生 Adreno 原语，FDL/IR3/PM4/KGSL/cache/AHB/SC 的资格另查各自硬件与真机证据。

## 5. 未找到什么、下一步怎样用

没有找到可公开复核、覆盖 raw NVN 全族并匹配本游戏 SDK53.314 的完整官方行为规范或独立通过的通用 NVN 重实现。imgui-nvn 是原 NVN 应用/注入样例（GPL2），不是驱动；LASRecomp README 指游戏重编译与宿主后端，未审 renderer，不把其限制当规范。Wonder 直链仍 404，现有搜索缓存不能提供固定 source commit，沿用已有审计的不可复现限制。

现在能立即采用的是上述精确公开事实、反例和测试设计；未知范围由原 SDK 静态方法+真实 caller ABI/typed 参数+独立 oracle 补足。行为事实与 backend 配置政策分开：CPU metadata 完整保存不等于 GPU 能消费，源 canonical 布局不等于 FDL native 布局；不能因资料不完整就继承任何 Metal 对象结构、格式限制、shadow/cache 策略或同步方式。
