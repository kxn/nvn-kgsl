# Turnip 独立验证线：构建与资格设计

配套审计：audit-bootstrap.md。本文所有新文件/新函数是**待实现提案**；已存在函数明确写“复用”。不以本文件存在代替实际Mesa编译。

## A. 固定实施选择

1. backend=Turnip，KMD=Android KGSL，一条真实graphics/compute queue，普通BO，offscreen，sysmem先于GMEM。
2. Mesa8e557130源码作为新native分支起点；保留现有设备/编译/WSI runtime，原应用f830不替换、不继承其UUID。AndroidAPI34/NDK版本作为可复现构建输入，不能由本文件认为已安装。
3. 第一目标：独立native client能通过同DSO接口建立设备、写入/读回BO、销毁，无NVN game参与。第二目标固定shader离屏图；第三目标NVN一小组调用驱动同一图。完整游戏与hostimport后置。
4. 使用bridge内部私有tu结构，公共C ABI只暴露固定宽度值和opaque handles；不承诺Mesa跨版本ABI。所有producer/cache key记录Mesa SHA、native ABI版本、shaderABI、GPU chip与layout profile。

## B. 独立实验树中的拟议文件边界

B0–B6是技术主题索引，不是独立的实施顺序；具体依赖与先后以experiment-stages.md的V01–V12为准。GMEM在正确sysmem/replay及实际所需WSI闭环之后，不能因为B4排在此处就提前实施。

### 技术主题 B0：构建与导出闭包，不做渲染

新增 src/freedreno/vulkan/tu_nvn_bridge.h/.cc 和 tu_nvn_android.sym、tu_nvn_android_symbols.txt。修改该目录meson.build增加默认OFF选项对应的文件和独立export/symbol-check；选项声明随当前Mesa顶层meson.options约定增加 native-nvn-bridge。不修改其他ICD公共export文件。

唯一导出C函数：tu_nvn_get_interface_v1(requested_abi, caller_struct_size, out_ops)。返回table必须含struct_size、abi_version、capability_bits。不存在feature返回明确UNSUPPORTED，不能返回空pointer后继续。调用者检查返回table长度及版本。公共ABI禁止std::vector、Vk/tu结构地址、exceptions、C++类型和跨DSO分配释放。

table首批只提供 create/destroy_device、alloc/map/release_bo、submit_write_probe、wait；详细descriptor/render接口不提前塞空壳。状态码至少OK、UNSUPPORTED、INVALID_RANGE、OOM、TIMEOUT、DEVICE_LOST。错误字符串由caller buffer承接，不能借用临时std::string。

Create输入：GPU selection约束、queue priority普通档、debug flags、allocator callbacks（可选但全对象同owner）。首版用tu_CreateInstance/既有enumeration/tu_CreateDevice冷路径构造own-device，内部保留精确Vk句柄以调用配对析构；禁止应用另外destroy内部VkDevice。NativeDevice持有创建进度和反序cleanup，create失败不返回半成品句柄。destroy要求先停接受提交，wait已接收工作，lost状态用专门quarantine/设备销毁策略，不假定成功idle。

构建保留libtu_files全闭包及生成器；该设计仅增加一文件和export，不做大规模“去Vulkan化”。先产compile_commands、linked DSO、dynamic symbols、未解析symbol检查、依赖/manifest。host不能dlopen AArch64产物；host ABI测试单独mock，不冒充Android加载。

验收：native OFF符号/行为不变；ON只有指定新导出；wrongABI/wrongsize拒绝；初始化每一步OOM均能反序释放已拥有资源。这个阶段本身不需要Android设备即可做cross-link，但不能说运行成功。

### 技术主题 B1：普通BO + 最小真实IB提交

新增 tu_nvn_submit.cc/.h，tu_nvn_memory.cc/.h（仍同libtu_files）。复用tu_bo_init_new/tu_bo_map/tu_bo_finish、tu_cs_init/begin/end及现有command packet helpers、tu_submit_create/add_entries/finish、tu_queue_submit/tu_queue_wait_fence。这些不是外部dlsym符号，在DSO内直接链接。

NativeBO记录logical_size、kernel_size、mapping_owner、IOVA、allocation_flags、device generation、refcount。map首次用placed_addr=NULL。Alloc默认ordinary non-shareable，无VBO/replay/SVM。Kernel返回mmapsize用于unmap，logical_size用于guest API范围判定。CPU map写入后依内存属性做cache publish；GPU写后等待完成并按属性invalidate再读。

submit_write_probe接收已拥有BO+offset+u32pattern；生成受控CP写内存包及其要求的完成/cache顺序，使用既有tu_write_buffer_cp/tu_flush_buffer_write_cp实现作为packet/flush源，不凭名字猜MEM_WRITE就能回读。必须在代码审查中记录最后GPU cache flush和timestamp完成边界。最初无需shader/descriptor、不会检验graphics state。

SubmissionRecord在ioctl前已分配并保活全部IB/data refs；先构造→范围验证→CPU publish→锁queue→提交→记录actual timestamp→解锁。不能在ioctl成功后再分配completion record，避免OOM丢失inflight工作。tu_submit_finish可释放host ioctl容器，但IB/data引用一直等actual completion；token64绑定context-generation/timestamp32。

非空IB路径可以无wait/signal调用tu_queue_submit，再在同queue锁内读取更新fence并记录；后续含WSI时必须使用真实vk_sync payload接口，不可继续仅读fence。每次失败不更新logical success serial；EPROTO/lost进入设备终态，不复用相关BO。支持ETIME超时保留inflight refs，不误释放。

验收硬件门：4096与实际16KiB页环境、非页倍logical allocation；CPUpattern→GPUcopy/readback、GPUwrite→CPUread；2000次创建释放计数归零；命令BO立即释放client引用仍保活至完成；超时不释放；失败注入分配/dup/map/import-info等。此门待授权硬件执行。无设备时只验证mock调用次序与refcounts。

### 技术主题 B2：抽descriptor基址emitter，不重写shader编译器

从tu_cmd_buffer.cc:4773 tu6_emit_descriptor_sets抽**packet部分**到新tu_nvn_emit.h/.cc（或共享tu_descriptor_emit.h），输入：chip、bindpoint、base_iova[]、count、reserved_dynamic_index/base、输出tu_cs。原函数仍负责tu_cmd_buffer dirty位、draw_state allocation和CP_SET_DRAW_STATE时机；新native caller自己维护等价state owner。

不能只复制A7XX分支给所有Adreno；模板实例覆盖受支持chip，A6XX双SP/HLSQ寄存器和update位不同。count、reserved index、descriptor alignment及32/64地址范围验证前置。根block不因每draw content dirty重建；只offset变化直接更新根参数的资格由shaderABI定义，不可省读后依赖。

先比较旧caller和新helper在相同snapshot输入下的packet bytes/relocations；变化仅adapter拆分。之后固定shader draw验证，才接NVN绑定。预计收益来自去除VkDescriptorSet对象分配/写入/再解析和重复资源解析，不来自删除resource hazard tracker。

### 技术主题 B3：固定shader sysmem图与native编译入口

现有tu_shader.cc包含tu_spirv_to_nir、tu_lower_io、tu_shader_create及tu_compile_shaders流程；tu_lower_io依赖pipeline layout/descriptors/key。不能把ir3_compiler_create当完整NVN编译接口。

顺序：先固定已知源shader+受控layout沿现有编译路径产生tu_shader/variant，用作bootstrap oracle；这里允许冷路径Vk pipeline编译对象。然后在tu_shader.cc中新增内部entry tu_nvn_compile_nir（名称待实现），输入NIR ownership、stage、NVN lower后的resource ABI、ir3 key、IO linkage、compile flags；复用tu_shader_create路径，不绕过lower_io默默改变descriptor含义。输入NIR规定“caller借用、callee clone后持有”或“转移”必须选一个，首版选择clone，失败释放clone与所有partial variants。

输出Artifact拥有code BO、variant元数据、const layout、scratch/pvtmem要求、stage linkage及hash。释放等所有command refs退休后。compiler device lifetime长于Artifact。若还用pipeline-layout中间表示，记录其冷期构造而不逐draw创建。新helper不导出DSO公共符号。

sysmem输出固定RGBA离屏图，与原Vulkan路径作pixel差分；首版没有GMEM tile replay，避免timestamp/query副作用掩盖bootstrap。达到这一阶段再接NVN Buffer/Program/Bind/Draw子集。

### 技术主题 B4：GMEM和真实NVN接入

复用已有tu6_render_sysmem/tu6_render_tiles、tile_config、LRZ/CCU/cache逻辑，先移入共享helper再替换caller。先一个color+depth、无query/XFB/feedback场景。sideeffect分类、pass结束条件、tile replay一遍/每tile域明确后才启用。不可独立于核心memory模型先“总是合pass”。

此实验主题读取revision3其他memory/command contracts的hazard/epoch/retirement语义；BackendOps只在native core内部作功能分发，不充当已经解决kernel或compiler依赖的证明。

### 技术主题 B5：同设备WSI桥

新增tu_nvn_wsi_bridge.cc，复用src/vulkan/runtime/vk_android.c与既有image/gralloc代码。首版same VkDevice/same real queue，swapchain/acquire/present冷对象保留。raw绘制等待真实acquire payload并signal可供present等待的sync；不能将queue idle或零wait release当正常present路径。

Acquire fd从API接管点计入owner，失败也按ANDROID协议close；ImportSemaphoreFd失败不自行假设接管。Release export成功得到owned sync fd，移交native window后不再close；若present前失败，cancel buffer且按具体API转移规则处理fence。每个external image记录generation、layout/modifier、buffer ownership、acquire/release状态，resize/surface-lost后旧image仍等其完成/producer释放再回收。capture与offscreen依旧不依赖WSI。

### 技术主题 B6：host import和qualified nativeimage（可并行开发，后验证）

参考f122a完整patch，在当前Mesa8e557树**语义移植**，不能称直接apply已通过。先移植KGSL helper/BO owner/probe与内部入口，再按native API必要性决定是否保留公开VulkanEXT facade。使用新产物身份和manifest，不称f830。

先只导入自有页及memfd双VMA小窗口。Probe必须对应实际shared映射类别，不以anonymous page probe替代memfd行为验证。GPU双向一致性通过前capability保持experimental。分批窗口import保持descriptor root稳定；超大pool不整池pin。支持4KiB/16KiB页、重叠窗口canonical backing identity、VMA撤销/地址复用拒绝、预算/回滚。

再用5c8完整combined patch的image gate/layout/bind检查作为参考，保留typed ASTC offline asset profile。它是独立于generic GOB image功能的优化，不因hostimport成功就把所有NVN图像设native。新Mesa UUID/layout变动后既有f830 opaque payload默认不复用，需新producer或独立逐条件兼容性证明。

## C. 注入与失败矩阵（开工必带测试）

|失败点|已拥有资源|必须清理/保留|禁止行为|
|---|---|---|---|
|instance/physical/device构造|按阶段的runtime对象/fd/锁|只销毁成功阶段，reverse order|手写free整个tu_device漏内部资源|
|ordinary alloc后map失败|kernelID、debug/RMV条目|bo_finish一次|重复munmap未建立mapping|
|dmabuf IMPORT成功INFO失败|kernelID，caller fd借用|FREE_ID；caller fd保持|close caller fd|
|dmabuf INFO成功dup失败|kernelID|释放ID、不发布BO|shared_fd=-1继续当ordinary|
|ADDR INFO/flags不合格|kernelID，borrowed VMA|FREE_ID、保留VMA|munmap guest alias|
|IB构造OOM，尚未提交|BO refs、submit容器|全部本地释放|返回可等待的success token|
|submit成功后CPU bookkeeping|所有inflight refs|record必须预先分配|因host OOM释放GPU正在读的IB|
|wait timeout|inflight refs|继续持有、返回TIMEOUT|提升CompletedEpoch|
|EPROTO/设备lost|可能已部分执行的工作|终态隔离，拒新submit|当TIMEOUT正常重试或当已完成|
|WSI acquire失败|接管的sync fd和buffer状态|遵APIclose/cancel|把fd=-1视作历史raw工作完成|

## D. 什么才算“可以开工”与什么仍需实测

本版已确定第一个target、导出符号、完整依赖路径、设备owner、队列owner、普通BO入口、每类fd/VMA cleanup、提交保活机制、六个技术主题及独立import gate。工程师可以先完成B0/B1，不需要等f830原始全树、PanVK/kbase或完整游戏trace。

尚不能由静态源码给出：目标手机cached/coherent真实双向结果、所有GPU芯片packet等价、f830整产物可重建、实际性能提升。它们分别有上面的限定probe，不再作为一句“需要测试”混在核心实现决策中。禁止将文档静态check、header smoke或mock packet测试报告为Mesa完整build/真机通过。
