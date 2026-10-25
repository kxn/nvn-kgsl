> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 真实 D2R 进入 descriptor 初始化的新边界

2026-10-23，独立分支 codex/nvn-turnip-study-20261023，仅新机 DEVICE_SERIAL。此前标准 NVN Window 双／三缓冲各两代的真机资格已独立提交 0965a7f；本轮推进真实 ARM64 D2R。

精确 0x12（CPU_UNCACHED|GPU_UNCACHED）加入既有 CPU-only UNQUALIFIED 构造策略，只增加 constructor 条件的一行 OR。底层继续 mandatory WB+IOCOHERENT same-page 导入，import/GET_INFO 均核实所需标志，失败不 fallback；原 map 指针保留，公有 cache 操作为已声明的 NOP。0x52/0x92/0xd2 等修饰组合仍拒绝。没有放开任何 GPU 或 descriptor 消费，没有变更 Window、桥以外的运行时或游戏逻辑。

原始 main/sdk/subsdk0 经统一 native/ABI1 工具从原 NSO 重建并核验；完整原始 RomFS 46 文件/8973547258 字节再次校验。a650-d2r-exact12-descriptor-boundary.json 及 stderr 表明原 SDK、构造器、nnMain 真实执行：0xa1、0x64、0x22、0x14、710MiB 0x21 后，2MiB 0x12 初始化成功，随后额外40MiB 0x22也成功。

首次实际 descriptor 消费是原游戏 nvnTexturePoolInitialize：count=15256，offset=0，pool 与刚建立的0x12同一身份；门禁上下文记录flags=0x12、policy=UNQUALIFIED、size=488192（当前 native软件slot间距32字节）。它在此处明确 ENOTSUP/exit78，没有真实游戏Draw或首帧。不能把这个CPU元数据操作的潜在GPU门禁当作GPU已经访问该池。SamplerPoolInitialize尚未真实执行；静态type7用途证据与本次首个TexturePool调用吻合。

typed TexturePool/SamplerPoolInitialize 参数观察在lookup/旧sticky/unsupported之前保留原宽度和顺序，输出object/pool/signed offset/count；不解引用caller，不制造guestPC，false/zero合法返回不改变。两个新wrapper和旧观察/ABI测试合计10组双sanitizer运行通过，包含6个新fatal边界。真实consumer上下文已由runner结构化记录，不冒充运行通过。

主机策略检查：0x12共1089项、0x14回归1088项，各22个GPU消费者拒绝、0次submit；Clang函数签名trap、GCC ASan/UBSan/泄漏及NDK严格编译/链接通过。原coherent/noaccess测试20676/37179项、各两种sanitizer通过。这些不是额外GPU通过数量。去掉唯一OR可重建已资格Window core基线，既有Window功能没有随之重写；先前GPU证据保留自己的源指纹，不改写历史记录。

后续已实现**纯CPU软件descriptor角色**：只有严格通过 TexturePool/SamplerPoolInitialize 的对象获得该角色，底层pool仍是UNQUALIFIED。当前API表由CPU写/验证并保存slot快照，真正Adreno硬件descriptor位于每次提交的私有job BO。handle以独立CPU引用表保活元数据，不把它的BO传入KGSL资源表。slot/version/latebind/域、范围、原始slot完整性及实际纹理数据的GPU门禁均保留；相同pool用于纹理像素、VBO/UBO/SSBO、code/command/CP数据源仍拒绝。此实现只覆盖已注册的软件表，不声称支持guest直接写入NVIDIA原始TIC/TSC描述符。

正式tracked源码检查通过：0x12/0x14角色各384项，10个直接GPU策略拒绝；两个纯HOST所有权submit核对真实编码及12次私有硬件表基址，基址从IB和固定schema patch元数据读取，不做字节扫描。覆盖实际15256/488192形状、CPU refs去重/65535上限、八处realloc失败回滚、record abort/finalize、NULL解绑、同record换表、late版本和旧child内容不变、in-flight真实纹理BO保留。两种sanitizer、两种ARM64严格链接及原color90/texture116回归通过。独立消费者门禁回归改为0x12共977/0x14共976项、各19个GPU拒绝；减少的三项对应现在允许的软件表Initialize/bind，其余直接GPU门禁没有放宽。Window core282项也通过。以上全部为HOST/编码证据，GPU与真实游戏结论另记。

并行同进程Activity宿主已提交61bcb41：原VI static token getter、原main专用return宏、原loader/HLE/gates、ABI2/TLSDESC canonical modules与真正ARM64共享库已构建/核验，尚未部署执行。其接口接线不等于支持普通游戏DISPLAY backing、真实尺寸/格式或存活Window跨generation恢复。相关静态计划另存 d2r-window-integration-static-plan.zh-CN.md；该文保持当时只读调查的历史口径，本次实际descriptor边界以本报告和真机记录为准。

真实重跑更新：48893e8 的完整原始NSO/ABI1模块和APK经正常重建、核验，在 .54 再次验证46个原始RomFS文件/8973547258字节后运行原 SDK/构造器/nnMain。TexturePoolInitialize(count15256,offset0)和SamplerPoolInitialize(count1256,offset488192)均真实返回1。首个新边界为 nvnDeviceGetImageHandle，ENOSYS/exit78；没有Queue初始化、游戏Draw或首帧。证据 a650-d2r-descriptor-role-boundary-v2.json 及完整stderr。此前无v2的尝试因为提交后canonical HEAD指纹过期在部署前被拒绝，记录明确deployed=false/host_execution_attempted=false，未覆盖失败记录，也未手修缓存。

下一个源码闭包加入纯CPU GetImageHandle：复用live-device与应用texture ID域256..0xfffff校验，返回uint64索引，不需要当前pool注册，也不创建GPU资源。原main首次调用literal为0xfffff，随后separatetexture=0xfffff、separatesampler=0xfff，作为缺省sentinel保存；原纯CPU HLE dm/gfx/nvn/hle_nvn.c:233也直接返回ID。后续三个静态调用从descriptor记录取得ID，其中一处通往BindImages。后者及RegisterImage、shader image读写仍不支持，capability query不虚报支持。90项CPU/ABI/范围/生命周期检查及两种sanitizer、typed-function检查、NDK严格链接通过；动态参数由typed bridge在lookup/sticky/fatal之前记录，不制造guestPC。新实现的真实调用结果待下次原模块运行确认。

缓存语义补强：exact14为CPU_CACHED，注册slot继续执行原TO_GPU clean及失败回滚；只有exact12的既有coherent CPU映射跳过这一内部cache调用。这是CPU发布语义，不放宽GPU_UNCACHED访问。正式角色fixture同步真实sampler1256形状，并检查14的clean失败不改变slot快照或两个backing引用。新的GPU资格子模式仅将四张软件metadata表设为12/14；实际纹理、code、VBO/UBO、target仍用已资格策略，资格专用wrapper检查软件表BO未进入真实资源列表、硬件表基址位于私有child job。软件表没有GPU写者，probe不对它们套用RT的FROM_GPU回读步骤；这不替代公开cache语义。当前仅HOST/NDK证明，真机结果另记。

软件表真机资格现已通过：a650-descriptor12-modes-v2.json 与 a650-descriptor14-modes.json 各完成17次标准NVN/GPU提交、278528个颜色通道检查，共34次/557056通道。每组17次审计均确认实际KGSL资源表排除四个软件表BO，136项BO检查及68项私有硬件表基址检查通过，late绑定、pool身份、origin/depth各case保留完整34份RGBA。实际像素/code/VBO/UBO/target仍使用原已资格GPU策略；不是GPU_UNCACHED像素访问、屏幕呈现或游戏帧。

首次 a650-descriptor12-modes.json 在GPU提交前被资格器拒绝（delegated=0）：VUB和FUB共享同一UBO pool，真实资源去重后为7，旧mock期望8。修正只涉及probe和runner，改为7项真实引用加1项私有child检查，即每次8项；真实NVN collector与shared-UBO观察器负例证明旧count会被拒绝。失败记录完整保留；没有减少元数据排除或实际资源保活检查。两组完成后本package均已停止并核实退出。
