# GPU 执行静态审查 v3

本轮不是“全代码无错”结论。检查目标是 revision2/turnip-strategy.md §descriptor、shader、epoch 以及章节03/04/06/07能否直接转为第一次提交。固定对照 Mesa `8e557130bda2adcb87535aabe23bca0d68ff77b6`。没有新backend编译或GPU测试。规范修订见同目录 `gpu-execution-contract.md`；本文列出实际发现、反例与关闭判据。

## E01 / P0：root/table 尚不是一份可编译 ABI

旧设计说“root携带pool base、buffer table与stage参数”，没有指定硬件bank、index单位、stage共用关系和compiler layout。若按VS/FS各分texture/sampler/buffer三个bank，会占6个graphicsbank，A6实际仅5个。A7最大8不是所有device共同常量；`MAX_SETS`注释也明确只是最大数组容量。

依据：tu_descriptor_set.h:16-29；a6xx.xml:4220-4231,4317-4329；tu_cmd_buffer.cc:4773-4860。修订：4-bank ABI，graphics/compute独立寄存器组，VS/FS共享bank3但各有明确区间；64B索引而非byte offset。关闭判据：schema单测所有offset/stride、A6 packetdecoder不出现bank>=5，编译shader的desc_set与emitter一致。

## E02 / P0：将“native descriptor = 64B”当总大小会把相邻资源读错

旧设计按长期slot描述64B编码，缺少storage bitwidth展开。`write_buffer_descriptor_addr`按设备生成1或2个64B records，`tu_nir_lower_ssbo_descriptor`在finalize后依实际bitwidth追加index。若slot b+1紧接slot b而b实际需要2records，32bit/8bit load会读到下一个buffer。

依据：tu_descriptor_set.cc:45-75,1129-1179；tu_shader.cc:1797-1849,3705。修订：每SSBO预留128B，填充合法native顺序，layout stride=2，保留postfinalize pass。关闭判据：覆盖has_isam_v/storage16/storage8合法caps组合的descriptorgolden+NIR索引测试，GPU按实际芯片跑8/16/32bit数据读写。

## E03 / P0：编译二进制不等于渲染管线已链接

旧描述“已有中间表示转ir3、改resource load”没有明确前后pass，易把root lowering放在优化后，也没写varying/VFD/SP配置和常量配置先后。`tu_link_shaders`在每stage `tu_shader_create`之前；`tu_lower_io`还分配driverparams（其注释有zero-instance indirect常量位置导致hang的具体workaround）。只复用机器码发地址仍可能hang或输出错。

依据：tu_shader.cc:1462-1519,3655-3705,3836-3865,3950-3965；tu_pipeline.cc:2379-2415。修订：先guest adapter生成标准resource/index/deref NIR→tu_lower_nir→link→create(含IO lowering/finalize/descriptor half修正)；单独抽program config/linkage emitter。root使用UBO而非手写constfile编号。关闭判据：canonical NIR triangle、varying梯度、baseinstance/vertex/零instance、CS driverparams分别通过；不能用clear-only当达成。

## E04 / P0：常量小索引可编码，不证明任意大 pool 已可用

`MAX_SET_SIZE`注释只说hardware至少2^16，1GiB是为CP_LOAD_STATE6 offset选的实现上限。ir3 dynamic texture/sampler用32-bit vec2，并非hvec2；因此也不能反过来宣称>65535硬件必不支持。旧文缺少capability/内存预算/最大索引验收。

修订：qualified slot count与guest容量分离；full20bit双image bank预算约128MiB；不能截断/模运算，也不能未证明的上一帧访问集合当dense remap依据。关闭判据：0/15/16/255/256/65535/目标最大slot GPU探针；同时核验非uniform索引。未通过归为本实现未资格化，不假称硬件缺失。

## E05 / P0：sysmem baseline仍可误开A7并发binning

`tu6_sysmem_render_begin:3342`在A7调用concurrentbinning路径，设置sysmem不等于关闭BV。若初版wait只在BR流执行，vertex阶段可越过依赖；event wait源代码明确按目的stage切both-thread。

依据：tu_cmd_buffer.cc:3342-3377；tu_event.cc:152-181。修订：首版单context sysmem且显式关闭concurrentbinning，packet验证；后期开启BV/BR必须补both-thread wait/patchpoint。关闭判据：decoder确认无concurrentstart，VS读先前CS/event发布数据稳定。

## E06 / P0：query与event不能作为普通tile-replayed命令

反例1：primitivegenerated即使有HWbinning仍每tile额外计数，源码显式SYSMEM|BINNING条件屏蔽tile；简单全量回放结果随tile数增大。反例2：上游Vulkan timestamp允许每tile重写保留最后值，不是NVN原执行点严格等价的证据。反例3：全部Signal搬epilogue则Signal→Wait同epoch变成等未来signal。

依据：tu_query_pool.cc:1410-1437,2100-2144；tu_cmd_buffer.cc:1400-1485。修订：首版query/event为epoch boundary在主CS执行一次；query跨段有accumulator/generation；后期每operation有ONCE/BINNING/TILE分类。关闭判据：1tile/Ntile计数一致、真实GPUticks、signalwait有序、queryavailable只在结果可见后。

## E07 / P0：alias hazard若把event poll当一般reader会自造死锁

新增反例：WaitEvent(P)尚未完成时，把SignalEvent(P)按WAR排到reader完成之后，producer永远不能写P，wait永远不完成。反之把整个event所在页排除hazard会漏掉同页普通数据。

修订：event word定义SYNC_POLL/ATOMIC_SIGNAL访问类型，专门处理comparison/update及publication；精确word span区别普通data，仍对同一backing的普通读写保留hazard。等待同一阻塞CP队列中尚未提交的未来signal不能自动靠CP_WAIT解决。需要CPU或其他可推进context的producer，或拒绝/改调度以证明可推进。关闭判据：跨contextwait先submit后signal可完成；同CP未来signal检测；同页邻近普通buffer的RAW仍触发cache依赖。对该协议的完整NVN mask/update语义由recording契约补齐，未明部分不得固定译为==1。

## E08 / P0：内容不变不代表GPU state仍有效

旧文“copy/compute污染相应组”没有穷举3D clear、resolve、querycopy、restore、prologue/epilogue，也未分gfx与cs当前bankstate。A→meta→A使用相同逻辑key时若跳过重发，GPU执行meta寄存器状态。

修订：every-emitter WriteSet+context hw-valid generation；未知writer全invalidate；首版meta后GFX/CS全重装，再逐组缩小。IB cache命中仅代表不用生成新bytes，不等于无需在GPU执行安装。关闭判据：所有emitters注册WriteSet的静态清单、A/meta/A图像与buffer一致。

## E09 / P1：稳定descriptor仍需descriptor cache与普通data cache分开

修改同一个table地址而base不变时仅重emitbindlessbase可能未处理缓存；storage→sample同backing也可能在UCHE/CCU不同域。源码包含独立SP_GFX/CS_BINDLESS_INVALIDATE、CCHE多副本invalidate及GMEMblitcache处理。coherentCPU映射不删除这些操作。

依据：tu_cmd_buffer.cc:380-405,5790-5880。修订：NativeAccess域分类+clean/executionwait/invalidate/materialize四项依赖输出，canonicalspan而非objectID。关闭判据：同VAdescriptorversion、RT→sample、SSBO→UBO、CPUpublication跨submit/同submit覆盖。

## E10 / P1：尾flush收益不能归因于Vulkan必然成本

`tu_EndCommandBuffer:5448`明确CCU尾flush并非Vulkan要求，原因是kernel只处理UCHE且不知道命令buffer是否submit最后一个。原生backend也必须保证真正submit/externalobserver前写回。删“每NVN EndRecording”对应人工边界可有收益，删真实completion前clean会返回陈旧RT数据。

修订：录制结束≠提交，nativebatch真实尾保留flush；先不优化上游尾策略。计数优化对象是冗余IB边界而不是API类别。关闭判据：最后IB画RT，fence完成后CPU读到正确数据；空/waitonly提交不会虚构timestamp推进。

## E11 / P1：原生table COW的连续地址没有免费实现

稳定slot/版本只定义逻辑关系。普通KGSLBO不能假定支持任意64B/page原子重映射；nativebank是一段连续GPUVA，“只COW一页”必须解释其余页为何仍连续以及在途表的地址变化。

修订：首版完整连续table snapshot，明确18.25KiB对齐分配和bytes指标；persistenttexture/samplerpool只在版本变化时更新整个被引用的table版本或采用经过资格化的映射策略。后续GPUcopy、紧凑ABI、remaparray均单独衡量，不能靠文档名词抹掉复制。关闭判据：inflight slot复用不污染旧draw、多queueRetireSet、bytes/peakmemory真实记录。

## 审查结论

以上机制性空白已用可实现的窄契约补上，但不是声称风险消失。开工顺序：固定schema+NIR适配/检查 → sysmem canonical draw/compute → NVN Program → descriptor缓存与meta状态恢复 → query/event真实语义 → 合格GMEM。所有初版feature gate必须进入capability manifest和unsupported回退，不得以stub/nop满足覆盖率。
