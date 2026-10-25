> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# NVN pool 下一实现边界（只读研究）

本轮只读现 driver、原 host/非 Vulkan NVN 接口事实、经原 NSO SHA 与3段 SHA 校验的 caller、已有 vendor KGSL 源及已有真机证据。没有 ADB/git、源码变更、CPU 模拟、GPU 执行或新成功 stub。地址均为原 main+offset。本报告不是首次真实 pool 调用日志：资源齐全后的动态参数/所有权尚待 root 实测；静态集合不能代替执行。

## 首要结论

`0xa1 = CPU_NO_ACCESS | GPU_CACHED | COMPRESSIBLE` 不能由直接mask掉0x80或照用CPU_CACHED的public Map路径完成；但也**不要求先复刻NVIDIA压缩硬件/UBWC才能创建这个GPU buffer池**。可明确选择native-uncompressed物理backing，保存压缩资格bit，把每种Texture的可观察layout/copy/readback合同单独资格。此选择是后端设计推断，下面严格列出原SDK证明的部分与尚未找到的正式规范。先拆开 CPU 可见性、实际 VMA/cache 属性、GPU 访问策略、压缩资格和资源所有权五个合同。现有 WB import 已能保持原 caller backing 的真实物理页，适合扩展受限 CPU_NO_ACCESS 对象；`0x22` 原 backing 是 game allocator 给出的普通 CPU 指针，不能换另一块 BO，不能只把 KGSL memdesc 的 CACHEMODE 改成 WC。

## 原 caller 的 storage/owner 事实

- `0x1749728` helper：type=w3；可选 allocator=x4，为0则取得原 singleton；`0x1749764..0x174978c` 将请求 size 向 ctx+0x70 的 unit 取整，并保存 size 到 ctx+0xb8；`0x1749774` 请求 0x100000 对齐；`0x1749790..0x1749798` 调 allocator vtable+0x10，backing 返回值保存 ctx+0xc0（`0x17497a0`）。本轮未确定最终 allocator 的物理页分配机制，不把它当独立 KGSL/WC VMA。
- type3 在 `0x174979c..0x17497a4` 跳过 NVN，仍有原 allocator backing；不是 flags0 的 NVN pool。
- 非 type3 另向同 allocator 请求256B/8B对齐的 NVN opaque object（`0x17497a8..0x17497c0`）；`0x17497ec` 把 size/backing 读入 x2/x1，`:97f8` 调 SetStorage；`:980c` 查原表 `0x1d7ba60`，`:9824` SetFlags，`:9838` Initialize。Initialize 返回值在这个 helper 内未检查。
- 第二 helper `0x17493c4` 向4096取整、align=max(w6,4096)，原 allocator backing 存 ctx+0x120（`:9458`），SetStorage在`:94a0`；GPU base GetBufferAddress在`:94f4`，保存 ctx+0x110。
- `0x174953c..0x1749568` 构造24B子区记录：直接以 backing+suboffset 作为CPU指针，GPUVA+suboffset作为GPU地址一起保存。**不经过 MemoryPoolMap。** 原静态 NVN表 `dm/gfx/nvn/nvn_slots.tsv:105..109` 的 Map静态调用数为0；这只覆盖静态扫描，不证明永不调用。
- 明确析构样例 `0x173e080..0x173e0bc`：先 NVN MemoryPoolFinalize(ctx+0x4e0 指针)，后由 ctx+0x4f8 的 allocator vtable+0x28 释放 opaque object，再释放 backing(ctx+0x4f0)。对应另一 pool 在`:e0cc..e104`。所以 driver Finalize只释放 KGSL pin/映射及自身元数据，不 free caller storage；失败时不得让 game 返回后照常 free 在途 backing，typed bridge 的明确截断必须保留。

原 flags 表：type0=0xa1，1=0x22，2=0x64，3=CPU-only，4=0x21，5=0x22，6=0x14，7=0x12。启动直接调用次序为 type0/2/5/6/4/7，PC=`0x173cc3c/54/6c/84/a0/bc`，size分别从配置+0xc8/cc/d0/d4/d8/dc取得；实际数值未动态确认。公开重建枚举 `third_party/nvn/nvn.h:605..614` 只确定 bit 名称，不足以证明 COMPRESSIBLE 是可随意忽略的提示。

## 现低层真的做了什么

- `src/nvn.c:173..185` 仅支持0x24或0x64，原 storage/size交 `nn_bo_import`，保存原指针、pool size/flags。`:186..203` 的 Finalize查资源refs、底层busy，Map无条件暴露CPU地址，Flush/Invalidate无CPU权限分支。`:223..233` BufferMap同样无权限判断；若未来 BO无CPU map，这里的 NULL+offset 必须先拒绝，不能返回伪指针。
- `src/kgsl.c:65..87` 的内部分配在A650请求 KGSL WRITECOMBINE 并核返回mode；A7旧资格请求UNCACHED。WC与真正UN CACHED在该4.19源映射属性不同；已有A650 unaligned-store fault/WC修复证据，不能退回Device映射给通用C访问。内部 BO拥有/释放自身mmap；不是caller storage。
- `src/kgsl.c:88..134` 的A650匿名import用 GPUOBJ_IMPORT ADDR，priv_len=整个mapping字节，明确请求并校验WRITEBACK；保持原storage CPU地址。绝不重试可能已执行的mutation，也不fallback到legacy MAP_USER_MEM。旧735分支不构成本次A650新flag资格。
- `src/kgsl.c:135..166` dma-buf import无CPU map，cache由exporter lock/unlock控制；这是AHB外部owner合同，不是原malloc backing的替代。`:181..192` 对该类CPU cache请求返回ENOTSUP；匿名WB用id/range同步；内部WC只fence而不伪做cache-clean。
- `src/kgsl.c:168..175,225..255` 资源busy与pending提交refs保证timestamp完成前不能destroy，未知submit结果保留所有refs且禁止自动重提。新增pool类型必须沿用。
- `src/nvn.c:695..750` Shader_CODE当前作为原Maxwell shader source池使用：按code/control完整身份匹配，然后独立分配已离线编译的Adreno native code BO。这个native code初始化不等于把用户资源CPU复制成另一块GPU backing；不得将后一种shadow copy伪装为pool import。当前matches_shader会CPU读source mapping，CPU可见性/生命周期需明确审计。
- `src/nvn.c:512..520,1068,1278` 等descriptor、录制command、enqueue parent-copy内部CPU访问与 API Map是两回事；引入无CPU mapping BO必须逐个检查这些消费者。仅把Map改NULL远远不够。

现有资格：`evidence/a650-nvn-api-wb.json` / `a650-final-nvn-graphics.json` / `a650-nvn-texture.json` 证明现WB profile及原shader fixtures；它们不证明CPU_UNCACHED/GPU_UNCACHED/COMPRESSIBLE池，也不是完整game首帧。

## KGSL 内核证据及限制

本机忽略目录 `parallel/pipa-kgsl.c/.h` 等为MiCode pipa-t-oss commit `77dc8e2290f3f5f15861821564f68d30ea193449` 公布源；是设备家族参考，不宣称与运行内核逐字节相同。已有`a650-import-review.md/.json`记录来源。

- `_gpuobj_map_useraddr` (`pipa-kgsl.c:2692..2724`) 保留CACHEMODE并pin已有useraddr；不改变原CPU VMA PTE。`kgsl_setup_anon_useraddr:2529`绑定原pages。把import flags从WB改WC只会让后续cache ioctl跳过维护，不会使malloc指针变成WC。
- `kgsl_mmap:4964..4981` 只有KGSL自身mmap才按CACHEMODE设置 pgprot_noncached/writecombine/writeback。`_kgsl_gpumem_sync_cache:3224..3229` 对WC/UNCACHED不做CPU维护。**KGSL_CACHEMODE是CPU映射/CPU维护合同，不能拿它直接解码成NVN GPU_CACHED/GPU_UNCACHED。** 当前提交objlist与qualified PM4也没有从NVN pool GPU_UNCACHED生成独立GPU-cache bypass策略。
- `pipa-kgsl_sharedmem.c:888..913` 会在MMU不支持时清IOCOHERENT，对WC/UNCACHED也清该位。若以后探索WB+IOCOHERENT实现自动CPU/GPU可见性，必须验证返回bit、真实no-flush双向读写/顺序；其可能满足可见性不等于已经证明NVN CPU_UNCACHED完整合同，不可静默fallback到普通WB。

## 可分配的实施阶段

1. **先抽出真实pool policy/ownership层，保留现0x24/64。** 建议独立`src/pool.c/.h`（父后续分配），表达caller-import/driver-owned/external-dmabuf、CPU API访问权限、实际CPU cache mode、GPU policy、compression资格、original pointer/size/owner generation及bo refs。完整枚举验证：CPU三选一、GPU三选一，拒组合冲突/未知/PHYSICAL/VIRTUAL；不mask。原storage范围必须对齐、无溢出、无重叠import/不同policy别名，失败不发布对象；回滚只释放自己创建的KGSL引用。
2. **CPU_NO_ACCESS/GPU_CACHED非压缩0x21的受限实现。** 保持同一caller storage物理页，沿已资格WB import并核返回id/size/flags；不为方便新alloc+CPU memcpy。分离driver内部必要access与publicMap权限；PoolMap/BufferMap按下文原SDK证据合法返回NULL且不置sticky；不要将底层private nn_bo_map改NULL。mapped cache操作按下文原SDK补证对no-access/uncached为合法NOP（有效参数）；private WB维护与public调用分开。不更改game raw pointer或mprotect其heap。所有GPU使用进入原busy/refs，Finalize不释放caller storage。此步**不会**自动放行先到的0xa1。
3. **0xa1显式native-uncompressed选择，Texture另资格。** 保存原flags及compression eligibility请求，实际同storage GPU import、private CPU可达/public Map不可达，不mask成0x21。原SDK会为bit7建立额外映射，但未证明所有资源必须固定压缩表示；不能仅因名字/没有UBWC永久拒绝pool。native backend可有清楚声明的“不启用native GPU compression”策略，前提是其所有已支持资源的layout/alias/copy/readback语义真实成立。当前只资格两个LINEAR RGBA8 profile；compressible Texture/packaged/block-linear/resourceview若未实现仍在对应Texture API准确拒绝，不能借pool成功宣称这些已支持。正式NVN是否把该bit描述为hint/eligibility，以及全纹理压缩透明回退规范，公开重建头未提供。将“保留bit+real uncompressed backing+strict texture qualification”作为可实施设计推断，而非已找到官方无条件忽略许可；无需为此先复制Maxwell compression字节布局。
4. **CPU_UNCACHED 0x22/0x12须先解决存储来源。** 首选支持明确由KGSL WC/受控external allocator提供、且caller raw pointer就是该真实mapping的storage。现原allocator地址没有这种证明；原helper CPU raw pointer使“Map返回另一个WC别名”也不够。若需原host allocator适配，必须是有分配/页所有权登记的可回收arena、保留原指针及其生命周期、在Finalize后正确回归allocator；不能任意MAP_FIXED替换game heap页或改写malloc元数据，更不能复制shadow后宣称same backing。真正WC映射/A650返回mode与caller实际读写需独立硬件资格。WB+IOCOHERENT仅是另一待核语义方案，不默认称uncached。
5. **SHADER_CODE 0x64维持并补声明。** 明确source pool padding/原caller shader source范围及ProgramSetShaders身份校验；Device property 0x4e不能按习惯猜值。target native代码BO有独立生命周期/IR3地址/同步资格，不能把SHADER_CODE随意映射成ARM PROT_EXEC（GPU代码不是CPU执行）。是否需要GPU readonly应按实际native code用途验证，不能对可能GPU写入的通用pool强设GPUREADONLY。
6. **GPU_UNCACHED 0x14/0x12独立后端实现。** 先确定NVN可见性/排序和Adreno真正的per-resource/per-access GPU cache policy或等效有证明的同步方案；不存在现成KGSL位可与0x10直接互换。现WB Flush CPU cache不是GPU cache bypass。未实施前继续拒绝，不能把所有GPUflag归为默认缓存。

## 最小验收与下一次真实观察

先保留WB compute/graphics/texture回归；再用明确新类型做GPU读写/同handle更新、CPU guard与邻cacheline、队列fence、busy Finalize、失败回滚、caller pointer不变/不被free、no-access Map合法NULL且无sticky、非法组合/重叠import拒绝。CPU_UNCACHED必须证明**不调用显式CPU Flush/Invalidate**仍具有所承诺可见性/顺序；GPU_UNCACHED须独立实测其策略，不借CPU WC结果替代。若要证明CPU_NO_ACCESS GPU输出，用真实GPU copy/已有适合的compute读回到另一个cached readback，不能由CPU镜像制造结果。

root的真实调用诊断建议只记录API名、flags、size、untagged storage/page alignment、owner分配范围/已知allocator、device/chip、effective KGSL id/mode及errno，打印后立即明确退出未知合同；不解引用未验证指针，不新造采样器/性能计时。还需动态确认：第一pool实际size是否页整数、初始化前后是否有CPU backing写入/别名、多线程、析构时GPU是否已finish、真实COMPRESSIBLE资源序列。上述未知不会因resolver有名字或first pool对象建成而消失。


## 已冻结的原 SDK Map / BufferMap 直接证据（补充）

原SDK SHA256 `1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017`，原NSO及三段hash、RELA位置/类型/加数、函数片段hash见同名JSON `original_sdk_api_proof`。只在RAM中解压并静态解码；没有写原SDK binary/原指令到本报告，没有执行SDK或采用其渲染实现。

- `sdk+0x4a3120` bootstrap经 `+0x466110` 到suffix查找 `+0x46b9c0`。suffix offsets=`0xabe798`、string base=`0xabafd5`，523项；默认fallback表由GOT `0xc78d50`的RELA解析为`0xc63e58`。MemoryPoolMap字典index263、BufferMap index31、Initialize index261。不是按猜测的函数地址挑“像Map”的代码。
- Map slot RELATIVE→`sdk+0x46cf50`，它调用pool vtable+0x28。普通非virtual constructor `sdk+0x478858`从GOT `0xc79fd0`取vtable基址+16，即`0xc4e498`；对应Map slot `0xc4e4c0`的RELA明确指向`0x4785ec`。
- 此方法读取pool+0x10 flags；`:4785f0`检CPU_NO_ACCESS bit0，命中即`:4785fc`返回NULL，`:478600`return，没有其它调用/diagnostic；非no-access返回pool+0x70。**本版本正常非debug路径下，这是合法NULL结果，不能置ENOTSUP或让typed bridge终止。**
- BufferMap=`sdk+0x45cc00`，`:45cc1c`调用同pool Map；`:45cc20`先检NULL，NULL直接跳return；只有非NULL才`:45cc28`加buffer offset。native应完全保留这个NULL guard。
- 正常pool初始化保留内部CPU地址：`sdk+0x4789e4`取builder storage，`:478a08`存pool+0x70；`:478b00..478b1c`缺内部mapping时仍尝试map。说明“public CPU_NO_ACCESS Map=NULL”和“driver保留private CPU backing”可以共存；无须把原caller heap mprotect或在低层把全部nn_bo_map隐藏。

公开检索也有边界：[BOTW重建inline头](https://code.botw.link/uking/uking/lib/NintendoSDK/include/nvn/nvn_FuncPtrInline.h.html)与[Odyssey重建NVN头](https://smo.monsterdruide.one/docs/nvn_8h.html)只支持ABI/枚举核对，没有完整的这两项语义注释；[deko3d自己的头](https://raw.githubusercontent.com/devkitPro/deko3d/master/include/deko3d.h)是不同API，不能拿其内存语义冒充NVN。CUDA的压缩allocation hint同样不是NVN规范。Map结论来自上面的本游戏原SDK直接静态证据，不来自这些替代品。

## COMPRESSIBLE：已证明的路径与设计推断

原normal initializer `sdk+0x46cda0..46ce20`将CPU/GPU类别规范化并保留高bits，0xa1的bit7没有被抹去；普通constructor保存flags于pool+0x10。其backing init `+0x478a2c`在`:478afc`单独查bit7；命中`:478b28..478b78`增加一次地址/映射setup，结果存pool+0x58，并经`:478604`继续引用同pool backing handle（pool+0x68）。失败可使Initialize false。没有找到这一步对整个caller backing执行压缩重编码或按某个外部Texture布局解码的路径。

**已证明**：该bit有实际原driver机制，不能把原实现描述为“什么都不做”；它在pool层准备额外映射，而Texture bytes/layout/格式另有接口。**推断**：这是使某类Texture能够选择压缩映射的资格准备；native backend可以选择未压缩表示，只要公共资源语义一致。不把“额外映射”提升成“官方保证可无条件忽略hint”，也不把它错误提升成“创建pool就必须读写Maxwell压缩字节/强制UBWC”。后续native-uncompressed实现的声明与测试应明确覆盖GPU backing、GetFlags保留bit、合法Map NULL、GPU buffer/已资格Texture行为，并对未资格Texture/layout立即具名拒绝。


## 限定补核：默认 flags 与 public mapped cache

- SetDefaults 字典index252、fallback RELA→`sdk+0x46cc10`：写builder+0x20 flags为`0x21`；清storage/size和其后私有字段，**保留device+8**。native PB defaults应对齐这两点，不再用flags0；可复用现TextureBuilder SetDefaults“保留device”的模式。不要借此填入新property。
- FlushMappedRange index257→`sdk+0x46cf60`→normal vtable+0x18→`sdk+0x46d040`；Invalidate index262→`sdk+0x46cf70`→+0x20→`sdk+0x46d088`。
- 两方法分别在`:46d050`和`:46d098`只检查CPU_CACHED bit2；非cached直接return，未调用cache函数或diagnostic。因此CPU_NO_ACCESS（0x21/0xa1）与CPU_UNCACHED的有效mapped-cache调用在该原SDK正常路径是**合法NOP，无sticky**。CPU_CACHED路径才取得内部Map地址并维护范围。JSON冻结了方法表RELA与branch/bodyhash；未读取不相关SDK函数。
- 这不表示匿名WB物理页不需要cache交接。新no-access pool发布前，driver应有显式private整BO clean/ownership交接，防allocator遗留dirty line未来回写覆盖GPU结果；提交/内部descriptor写后按实际WB模式维护，GPU写后内部CPU读要先fence+invalidate。不能依赖public no-access Flush（它按NVN合同NOP），也不能在inflight GPU写期间clean旧CPU cache。

## core 中可能碰无mapping的全部入口清单

以下为本轮 `nn_bo_map` 全量调用扫描，精确行号及对应core SHA在JSON `core_cpu_mapping_consumers`（父线程修改会移动行号，以函数为准）。**对CPU_NO_ACCESS只在public Map隐藏地址，private BO map不应改NULL**；下列guard主要防真正无CPU-map的dma-buf等被误用。

| 入口/内部路径 | CPU访问对象与处理边界 |
|---|---|
| MemoryPoolMap / BufferMap | public API按CPU_NO_ACCESS返回NULL、无sticky；BufferMap NULL先判断再加offset |
| TexturePoolRegisterTexture；SamplerPoolRegisterSampler / RegisterSamplerBuilder → descriptor_write | descriptor pool backing真实CPU写、旧32B备份、回滚及WB flush；无private mapping时必须在登记前拒绝，不能NULL memcpy |
| ProgramSetShaders → matches_shader | 对source shader pool原code范围memcmp；公共Map不可见不等于private code不可读。真正unmapped source不能进入memcmp，应明确unsupported或有独立合法GPU读回资格；不能伪匹配source |
| ProgramSetShaders native code初始化 | memcpy写nn_bo_alloc创建的内部VS/FS/CS code BO；保持该alloc“返回有WC CPU map”不变量，不受source pool public权限影响 |
| CommandBufferAddCommandMemory（提前门禁） | 当前不查private mapping；应在接纳arena时拒绝真正unmapped BO，避免下列record路径之后崩溃；CPU_NO_ACCESS是否允许作为arena是独立支持合同，不能只因内部可写就放宽声明 |
| DrawArrays | textured placeholder和untextured indirect packet都CPU写command arena |
| DispatchCompute | CPU写command arena的indirect packet；encode写独立内部job BO |
| ClearBuffer | CPU写command arena中的CP packet；target本身仅取GPUVA，不需public/private CPU mapping，可用于GPU-only验证 |
| EndRecording | CPU向command arena写尾包 |
| QueueSubmitCommands → materialize_draw | CPU读texture/sampler descriptor pool bytes做与snapshot一致性核验；真正unmapped descriptor backing需提前拒绝 |
| QueueSubmitCommands parent-version路径 | memcpy读原command arena、写内部parent BO；其后CPU patch私有parent。保持old-inflight refs与samehandle immutable版本 |
| nn_cs_encode (`src/compute.c:18`) / nn_gfx_encode (`src/graphics.c:67`) | 只CPU写各自nn_bo_alloc内部BO；不把public pool access权限传播到低层private map |
| nn_kgsl_probe (`src/kgsl_probe.c:43`) | 内部分配的原探针map，不是原game/新pool支持证据 |

建议首个实现/验收：1) 严格接受0x21/0xa1的real same-storage WB import，保留flag与native-uncompressed策略、明确initial private cache交接、Map NULL/NOP语义与defaults；2) host ABI测试合法NULL/NOP不触发typed fatal、无效对象仍具名截断、GetFlags保留bit/SetDefaults保留device；3) GPU-only ClearBuffer/适合的真实compute/copy-to-cached readback验证实际同一backing，资源refs/忙Finalize/回滚/guards与现compute/graphics/texture回归；4) 真实game下一调用边界另记，未实现CPU_UNCACHED/GPU_UNCACHED/Texture/window继续明确停止。CPU读回只作为测试oracle且在wait+invalidate后观察真实GPU输出，不用CPU写结果冒充GPU结果；不拿pool创建通过当真实首帧。
