> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公开 NVN 行为来源复核（只读，2026-10-24）

本轮只联网读取公开原始仓库、作者说明与 NVIDIA 文档；没有读/复制 Metal 或 Vulkan 后端，没有重试原 SDK 的静态任务，没有改 tracked/部署/执行 GPU。下载 URL、HTTP 结果、commit、文件 SHA 均见 downloads.json 与各 repo/head.json。声明齐全不能替代行为验证。

## 可实际使用的来源

|来源与固定版本|可用内容|许可/适用边界|
|---|---|---|
|[open-ead/nnheaders](https://github.com/open-ead/nnheaders/tree/246c3b482d045934ed004f50c5ada37f3b10b703)，commit 246c3b482d045934ed004f50c5ada37f3b10b703|include/nvn/nvn.h: NVN enums、opaque objects、PFN ABI；C++头含明确方法签名和loader。README说是用户由非剥离符号/DWARF等重构，非官方SDK发布。|C头1..21自身MIT/h1k421；C++头无相同文件许可头、仓库根未见统一LICENSE，不能把C头许可自动覆盖所有文件。C头23..26特别说明由Darksiders II DWARF恢复，覆盖53.105使用的enums/structs，可能混有更新版本内容。不是NVIDIA驱动报告版本，也不是完整NVN规范。|
|[devkitPro/deko3d](https://github.com/devkitPro/deko3d/tree/373bfd43d8b2d805fa2eb7d406dbb823032df958)，commit373bfd43d8b2d805fa2eb7d406dbb823032df958|作者明确说明采用NVN逆向路线、公开硬件知识。独立API的image layout、TIC/TSC、cache barrier及descriptor消费源码；Primer说明对象、绑定和同步模型。|LICENSE zlib/fincs：保留归属/许可，修改需标明。它是独立Dk API，数值枚举、默认值与全部NVN语义不能直接等同；可作为有来源的硬件/独立行为参考，不能冒称原SDK逐字节事实。|
|[NVIDIA/open-gpu-doc](https://github.com/NVIDIA/open-gpu-doc)，本轮HEAD记录在nvidia/head.json|原厂clb197tex.h、clb197.h、clb06f.h字段/位定义、method格式；能命名TIC归一化、TSC覆盖及CACHE操作硬件域。|所取各头SPDX MIT。硬件字段定义不证明NVN运行时具体写值，也不能直接作为Adreno寄存器或cache等价方案。|
|[NVIDIA PTX ISA9.4 §5.3.2](https://docs.nvidia.com/cuda/parallel-thread-execution/#sampler-properties)、[texture queries](https://docs.nvidia.com/cuda/parallel-thread-execution/#texture-instructions-txq)|明确纹理normalized_coords与独立sampler.force_unnormalized_coords两个属性；未指定值由源语言运行时决定。|公开原厂文档，适用PTX抽象模型；不是NVN对象默认规范，不证明SDK53.314配置。只作字段语义/交叉核对，不移植文档或CUDA运行时。|
|[Switchbrew GPU](https://switchbrew.org/wiki/GPU)、[GPU Classes](https://switchbrew.org/wiki/GPU_Classes)|独立RE维护的TIC/TSC32B、handle低20/高12、FIFO/class/method、CB_POS/CB_DATA与cache方法索引。|社区持续编辑资料；不是官方NVN规范、没有统一SDK版本承诺。页面缓存/内存一段是早期Switch特定说明，不适合外推Android WB/IOCOHERENT。转载先按页面实际许可核对，当前只链接/归纳事实。|
|[BotW公开代码浏览器NVN头](https://code.botw.link/uking/uking/lib/NintendoSDK/include/nvn/nvn.h.html)、[inline wrappers](https://code.botw.link/uking/uking/lib/NintendoSDK/include/nvn/nvn_FuncPtrInline.h.html)|可独立核对C PFN/inline转发、参数与返回ABI；gfx wrapper展示具体公开调用例子。|去编译/重构工程发布内容，不当作SDK官方全规范。网页未固定commit；其父仓库本轮HEAD8f0334acf0d348bcf48787038d0a0ba63f740ee4、NintendoSDK子模块fed45e2614b626f62809d0e1319496f51bc09871，不能擅称网页正对应此revision。|
|[Odyssey公开Doxygen NVN C++](https://smo.monsterdruide.one/docs/nvn__Cpp_8h.html)|类/enum/方法索引、可定位原始声明；可与另一重构头交叉核ABI。|parent本轮HEAD696db2ff5ed558b053c753de420504b8682f24b0，NintendoSDK子模块097321c98c2cc174c53ec3740bb53488796785c2。不能把生成网页日期当NVN版本；不是详细行为手册。|

本轮未找到公开且可核来源、覆盖完整NVN行为的官方reference/spec。搜索返回的公开头大多为重构声明，注释远少于方法签名。nnheaders C头1757行、PFN声明可按正则独立统计，行为注释主要只有来源/版本说明；C++头还保留“verify ENUM/BITS constructor”TODO。这些是有用ABI资料，不能把“存在该函数”解释成默认值、同步时点、零区域行为或完整压缩布局规则已知。

## QueryTextureCoordNormalized 的确切层次

1. 它不是上述公开NVN导出名，而是现用Maxwell shader translator的IGpuAccessor方法。vendored IGpuAccessor.cs有归一化信息的接口契约；TexturePass.cs:150..170用它决定是否为float coords插入除纹理尺寸，IntCoords路径最终不需要归一化变换。抽象接口默认true只是translator默认实现，不能据此回答运行时。
2. 原厂clb197tex.h把TIC NORMALIZED_COORDS定义在descriptor bit191（word5 bit31），并有独立TSC FLOAT_COORD_NORMALIZATION（force unnormalized值1）。PTX解释独立sampler覆盖和纹理字段的区别，但没有给NVN2D默认值。
3. 本轮独立实现证据：
   * [deko tic_generate.cpp#L60-L95](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/source/maxwell/tic_generate.cpp#L60-L95)：Pitch与BlockLinear两分支都由实际image type产生normalized_coords，普通2D为1、Rectangle为0；BUFFER走独立header分支。
   * [deko tsc_generate.cpp#L88](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/source/maxwell/tsc_generate.cpp#L88)：显式设置float_coord_normalization=0，非从shader期望值填充。
   * 公开NVN C头778..789中普通2D和RECTANGLE为不同target（1与7）；DkType编号不同，必须按语义映射，不能复制数字。
4. 这些足以支撑**明确声明的独立RE兼容策略**：本后端仅已支持的、真实注册普通2D(depth1/mip1/non-MS) resource，其源坐标契约为normalized；Rectangle/BUFFER/其它未知target不由这个普通2D规则答复。实际sampler metadata仍须独立完整snapshot+native能力预检，不能因query为true而跳过。
5. 限定：本轮没有新证据证明SDK53.314每条TextureInitialize/TexturePoolRegisterTexture路径实际TIC/TSC值。将该策略称作“public definition + independent NVN RE based backend policy”，不能写“已从原SDK实测默认”。不允许shader expected-queries反推、按a767身份硬编码，或以QueryNormalized回答替代sampler force域验证。

## 布局/同步资料如何使用

* 布局：deko dk_image.cpp包含mip/array/block-linear计算和kind选择，原厂class头能核字段；它们适合计算器和独立oracle依据，但source NVN公开size/alignment仍需与实际SDK版本事实/资格一致，不能用Adreno FDL bytes冒充canonical extent。COMPRESSIBLE不是无条件有压缩，DISPLAY也不能当无约束标志。
* 同步：deko public Barrier enum对tiles/fragments/primitives/full有文字解释，gpu_base.cpp落实Maxwell packets；NVN public BarrierBits名称可核对应意图，但不能把deko枚举直接当NVN数值。Adreno依赖必须继续用实际common order/producer+consumer与真机验证，不复制NVIDIA packet或推断CPU cache policy。
* 采样：TIC/TSC源事实、descriptor语义和实际typed resource目标三者分开。nearest/clamp UNORM2D真机结果不能推定sRGB、linear/mip/array、3D或active border已支持；BC1 RGB alphaONE与RGBA punchthrough必须保持实际FDL差异。

## 既有旧证据的重新核验清单

* 旧texture-handle-mapping.md混有Metal交叉引用：本轮不作依据。CB2/0x20+0x168+0x568应引用新的独立SDK源事实报告或原厂字段/独立RE，并保留版本限定。
* translator accessor默认true、旧fixture归一化profile、2D旧固定native descriptor：均不能作为运行时true来源。应由实际registered target与明确策略产生。
* 早期0x880 full-control、旧getter opaque-byvalue、早期MAX_VERTEX32等已纠正历史：不能重新沿用为当前NVN语义。当前full898、指针getter及NVN16/native32边界分别使用已有独立证据。
* 旧“整个NVN声明完整”或以C++ wrapper等同运行时成功/同步完成的表述应降为ABI覆盖；unknown entry/未实现native consumer继续明确失败。

所有可执行实现建议另存新candidate，不改先前BC freeze；本报告不制造HOST/GPU通过记录。
