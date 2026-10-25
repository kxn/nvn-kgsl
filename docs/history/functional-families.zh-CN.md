> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 按完整功能族实施与验收

用户在2026-10-24再次指出：不能沿着真实游戏下一条错误逐项补一个组合。此前推进确实存在此问题：公开入口增长、单一组合可运行，与通用GPU语义完成之间有明显差距。本文件调整实施范围，不将专项测试重新命名为游戏进度。

每族先按已确认NVN声明、原SDK静态证据、实际NIR指令事实和Adreno寄存器/布局合同展开实现，再成组验证。未知行为仍作为缺口记录。不能通过删校验、返回成功、伪造source/state或CPU制造图像来扩大覆盖。真实D2R在族的组合边界验证，不作为唯一待办来源。只使用独立KGSL/PM4/IR3/FDL和原生Android窗口路线。

| 功能族 | 本次可核实状态 | 接下来整组范围 |
| --- | --- | --- |
| 清屏、颜色目标 | 公共NVN22格式清屏536次、颜色目标132次真实GPU提交已通过，证据分别独立 | 保留Copy/Call/回放、掩码、混合、scissor与canonical存储检查；与深度和采样组合验收 |
| 二维纹理采样 | 公共22格式SAMPLE整组264次真实GPU提交/Finish通过，4325376通道最大误差1；两种尺寸、4mip转换、3组连续view切换、录制后注册、提交中描述符替换、完成前拒绝释放和完成后回收均验证。正式宿主开关/原36源链接已接通 | 公共FETCH/SIZE+BUFFER、Gather/真实FragDepth和LOD过滤仍需各自组合资格；本批为原source-backed SAMPLE、原创输入纹理、nearest/clamp，不代替整个采样族。整数/BC/体积/深度保留独立角色 |
| Gather | 原创native96提交通过。公共原CASC GatherR+FragDepth整组72提交/Finish/Draw、72全pool读回通过：255204深度像素，D16/D24误差0、D32F exact；三wrap、两origin、两形状/selected mip、12 Copy及12 Call组和更新输入重放；全部真零COLOR，完整原C1保留 | 本组MIN/MAX规约不能证明Gather分量顺序；不覆盖其它分量/offset、比较采样、所有纹理维度或真实游戏Draw。Clear只验证0/1端点 |
| mip过滤、LOD | 576次真实提交/Finish与完整raw保留；原exact标准部分三线性最大误差3超出2，原判定仍失败。新增实际未改写Khronos CTS固定GLES3 HQ离线参考，576raw/9437184通道均通过，7个数学负例、24个工具负例；这是独立参考，不是NVN规范 | 公共LOD过滤组合、实际NVN黑盒精度仍需确认；CTS的rho区间明确含约+0.5上侧LOD，不把它写成仅lodBits6量化，不由实测拟合oracle。各向异性单独明确 |
| 深度 | 原432专项和canonical648全链通过；公共原CASC GatherR+FragDepth的72真GPU组合也通过。新增非比较深度采样整族34文件已合入，实际raw-native60 Draw/180 Submit/60 Finish/60完整读回通过；D16/D24X8/D32F真正深度FDL角色，188400通道误差≤1。公共原source采样12组HOST通过 | 公共普通raster-depth GPU、公共depth sampling GPU、真实游戏组合、stencil/组合平面、MSAA、shadow compare/mip过滤/array shader尚未完成。不能把raw60借作公共NVN或游戏像素证据 |
| shader和程序目录 | 完整code/control/C1匹配，实际查询在Draw时解析。旧51特化从原始NSO用当前normal工具整体重提取/重编译；188既有配对保留94成功/94失败，188份成功VS/FS机器码全字节一致。与CASC DepthMode0/1合并后为55特化/96成功配对；不新增笛卡尔配对。另有265身份/139资源族批量诊断 | 265不是全部6899变体；大族仅结构代表。旧51与CASC之外的大量身份/实际资源特化尚缺；SSBO、FrontFacing、vertex sampling及未知句柄映射仍分族实施。不得将所有登记身份计为已编译或GPU支持 |
| 原游戏组合 | 最新v20从原始ExeFS规范重建，完整55特化/96配方目录、color/sampled/depth/gather开关接入；真机原模块/nnMain、16个纹理、441次描述符登记、4个计算程序。停在未启用的packaged-layout实验边界，没有原游戏Draw/Submit/Present/首帧 | v16使用显式BNTX packaged实验曾到34个纹理/图形source缺口；两者选项不同，不能宣称本次游戏边界前移。布局语义独立核实并整族资格，不能把窄实验默认为已完成NVN |

当前并行分工为：FrontFacing整族与SSBO联合编译器、图形SSBO公共整族、完整原始shader source catalogue与compiled recipe分离；主代理合入审查、唯一ADB测试、原项目规范构建及游戏接入。设备测试串行执行，源码/构建记录冻结到该次验证终态，结束后停止测试进程。实现并行不等于多个代理同时操作GPU。

覆盖报告分别记录入口注册数、完整支持的语义范围、部分支持和缺口、HOST/NDK、专项GPU及真实游戏。406入口/102已知待补/15未知ABI的数量没有因本次96个测试用例而增加。后续不以测试数量或入口数量代替通用能力与游戏里程碑。

本次新增证据：`a650-native-gather96-v1/`。其中二维采样基础证据明确为HOST/链接，不能借用Gather或颜色目标GPU结果。

后续整族批次证据：`public-float2d-depth-host-v1/`、`a650-native-depth396-v1/`、`a650-native-sampler576-v1/`。深度原始失败和mip原始失败均完整保留；396 raw-native深度通过不等于公共NVN深度或原游戏Draw。

本批新增 `a650-public-sampled22-v1/`（264完整raw及正常输入/HOST/APK记录）、`canonical-depth-upload-host-v1/`（双向传输与整组owner，仅HOST/NDK）、`sampler-mip-cts-reference-v1/`（对原576raw的独立CTS离线结果，不新增GPU提交）。公共采样本批验证了65175552只读字节和2162688保护字节；该批归档时原游戏最新为v16，没有Draw/Present/首帧。所有历史失败及其哈希保持不变。

`a650-native-depth432-v1/` 进一步收齐276份完整raw（6193152B）。新增mode11不分配、查询或绑定COLOR，真实depth-only artifact写入深度；原mode10仍明确标记dummy COLOR，前66组及文件名保留。整批验证2593824保护字节、23001408只读字节和2580次占用检查，原容差不变；不借此宣称公共NVN或游戏已绘制。

`a650-canonical-depth648-v1/` 收齐180份全链完整raw（14524416B）。54组资源/108次更新输入的链执行，每链6次提交、末尾一次Finish；提交的完整owner和Draw资源统一保活。除了实际深度结果，还精确核对未选mip/layer、GOB padding、1858368保护字节和25654752只读字节；13392次占用检查通过。D24仅选中视图的87960个X8字节无定义，不扩大到其他字节。HOST负例证明漏上传、漏回写、错mip/layer和旧重放输入均会失败；GPU不执行CPU shader或私有图像转换。正式宿主新增depth开关和三份正常helper检查接线尚在组合验证，不能把政策检查写成已通过正式宿主启用链接。

后续 `public-depth-family-host-v1/` 已完成公共路径12组HOST检查：GCC ASan+UBSan、Clang function-trap分别8729断言/27次mock提交，真实KGSL DSO及两个fixture的API29严格链接通过。正式宿主ABI1 depthON、ABI2 depth+sampledON、ABI2 depthOFF三组合分别72/75/69个编译源，均包含原36个HOST源；41断言/31负例，实际编译宏、目录和ELF依赖检查通过，未接入旧渲染器。它们没有GPU/真实游戏执行。归档初始manifest的基线commit多写一个尾字符，其正确值与未变的源码/记录哈希见同目录 `provenance-correction.json`，历史文件未重写。

`casc-capability-batch-host-v1/` 保留265身份的正常批量诊断、原始失败及重现输入。纹理句柄仅复用已有C解码器：36个首个需求key中22可解、14仍95；可解不代表已获实际资源类型。3个结构配对仅1个严格native recipe成功，另2个仍受unused sysval等门限限制；25个SSBO VS实际common IR3均编译，但当前native消费者不支持。它们不能计为公共NVN或真实D2R绘制通过。

`a650-public-casc-depth72-v1/` 保存72份完整真机raw（4177920B）、正常full96 inputs/HOST/APK和实际验证源码；3327240 canonical保护字节、10616832只读字节全部通过，selected D24的85068个X8 carrier字节未定义。进程force-stop0且pidof1/空输出，严格确认退出。原C1为256B/首值-1，实际VS CB6→API3、SeparateTexture10/Sampler3均保留；测试是原创资源/状态调用原CASC shader，不是原D2R模块或首帧。

原首轮失败单独保留在 `a650-public-casc-depth72-call-failure-v1/`：实际4提交/Finish、4份读回，下一Call组合95/未知owner保活，退出后原证据不改。前4份D16读回独立核对误差0，整轮仍失败。原因是测试caller未显式录制Call所需渲染状态；修正caller并补双san完整72条HOST metadata/lifetime路径和与GPU共享的严格ledger后，才做新一轮完整真机验证，未放宽core校验或像素参考。

`graphics-full51-restoration-host-v1/` 保留原始NSO正常重提取、原state/profile与188配对选择、全188机器码字节回归和可复现编排。历史记录仅作为既有声明集合，当前来源重新建立，不修改任何旧哈希，也不把声明的纹理特化当作新采集的游戏资源事实。

`a650-original-game-public-depth-full96-v20/` 是新增真实游戏结果：正常HOST/APK构建及运行前校验通过，46份RomFS/8973547258B核对一致，使用原签名install-r保留数据。原ARM64 main/sdk/subsdk0加载并进入nnMain；format68、128×128、8mip的packaged bytes `02:00:00:00:07:00:01:00` 在StorageSize明确95。此次未启用历史defaultOFF BNTX writer interop实验，因此早于v16停止；不是新增GPU故障，也没有绕过未证实布局继续执行。源码/HEAD固定322f8e2直到终态，force-stop0、末次pidof1且输出/错误均空。原模块的Draw/QueueSubmit/Present仍为0，真实首帧未达成。

`a650-native-depth-sampled60-v1/` 保存本次原生深度采样60份全目标读回、normal runtime/foundation/shaders/inputs/HOST/APK及实际34源码。五种操作/filter、两奇数形状/选mip-layer、30组更新canonical后同job重放均通过：794640保护字节、9747760只读字节、2592次占用检查；进程严格退出。基础owner双san各2031508断言，公共原a767显式R001 SAMPLE及CASC GatherR+FragDepth共12 HOST case/各8831断言。所有GPU路径仅原始canonical→CS untile→A2D upload→真实深度texture unit，私有depth/LINEAR数据不CPU映射；每enqueue六个私有BO的当前实现明确是正确性版本，不声称性能优化已完成。采样/格式查询仍依实际NIR与显式swizzle，未知QueryTextureFormat或比较采样继续95；正式宿主接线见随后独立证据。

`host-depth-sampled-features-v1/` 完成正式defaultOFF `--depth-sampled` 构建/复验转发、严格boolean与依赖校验，以及四份正常NDK API29原宿主构建：ABI1普通深度72源、ABI1深度采样76源、ABI2深度采样77源、ABI2 depthOFF69源；各36原HOST源。55断言/52负例与实际编译宏/符号/库检查通过；未链接旧renderer、未加入资格测试代码。初次命令传错names路径的失败日志保留，freshv2使用已有normal names.inc.h后通过，未改任何语义校验。此批只有构建/链接，没有Android/GPU/原游戏执行，不改变v20真实游戏边界。
