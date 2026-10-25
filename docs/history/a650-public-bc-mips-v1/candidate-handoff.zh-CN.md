> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公共 NVN BC 多 mip GPU 资格候选

仅 ignored 候选；没有 ADB、Android 执行、GPU 执行或游戏帧证明。前置 BC mip 14 文件包不改；root已另行推广该运行时。本包只补公共caller、正常producer、CMake/APK/runner薄接线，默认OFF。

入口为 `build_android_probe.py --nvn-pipeline-bc-mips-check CHECK`，CMake `NVN_NVN_PIPELINE_BC_MIPS=ON`（必须BC=ON、独立SRGB qualifier=OFF），沿用既有普通应用UID Activity、nonce、raw文件、强制停止与完整source/tool/artifact核验。没有另建transport或probe私有encoder。

矩阵：8种 BC1 RGB/BC1 RGBA/BC2/BC3 UNORM+sRGB（0x41..0x48），source64²与127×65，各4mip，三组view，各同handle重放一次，共48handles、96实际Submit/Finish。范围依次为0/4→1/3、1/2→2/2、3/1→0/1。所有4mip填不同合法端点色，repeat改变全部mip的编码；公开GetStorageSize必须等于正常source calculator，保留完整canonical BO/extent。Runtime只做已有GPU raw-block转换与硬件BC/sRGB采样，无CPU解压或图像转换。

原shader仅正常archive中显式选择的 a767 sampled pair。producer严格核完整已声明source/control/C1身份、原NIR texture/sampler masks与actual-NIR2D sample事实；graphics资格仍bounded0x880控制域，不称gamefull0x898。新v3 header存在 `NN_GRAPHICS_PIPELINE_HAS_TEXTURE_TYPES` 时调用同生产 `nn_nvn_register_typed_pipeline_archive`；旧header仅编译时legacy分支，结果typed_archive=false。新producer必须有actual NIR types且runner要求typed_archive=true，因此新资格无法借legacy通过。ProgramSetShaders、资源/selector/query/variant选择均为实际公共NVN路径，未回填expected查询或运行时选首pair。

每handle先record Draw、poison handle数组，再Register实际texture view，CPU texture与sampler立即Finalize。每次Submit后、Finish前，再把slot257更新到另一backing及不同base/count，poisonCPUview；原canonical backing publicFinalize必须EBUSY。Finish成功后，原backing才允许释放并通过真实public pool初始化重建。该过程不修改尚未Finish的源字节，验证提交snapshot的独立生命周期；不是并发GPU时序测量，不能保证每次更新与硬件采样确实重叠。全mip private BO保活机制另由前置真实common encoder HOST fault fixture覆盖，公共probe不窥探私有BO或假报检查。

每次读回完整24576B target pool，4096前/后guard，16384B像素；其它输入pool逐字节readonly检查。只有合法CPU descriptor更新的32B slot同步更新shadow参考，其余metadata字节保持原参考。两个canonical source backing均包括全部mip和padding的readonly检查。每次成功都保存nvn-pipeline-00.raw..95.raw；缺文件/错误文本由现有严格collector拒绝，最终普通UID/nonce/退出必须验证。

C closed-form与独立NumPy oracle不读取压缩input bytes推导颜色。输入只使用特意写入的端点/透明selector，不实现BC codec。四组5:6:5端点的每mip bank不同，sRGB走标准分段transfer；与C端点方程全1572864通道逐byte交叉核对。NEAREST非mip过滤只采选定base的相对LOD0，本批采遍base0..3并更换内容；不声称mipfilter、所有LOD、array、3D、C1FS、GOB RT、完整gameframe已资格。RGB容差1、alpha严格0，保留既有边界，不为通过扩大误差。

## 成功 JSON 合同

operation=nvn_bc_mip_sampled_pipeline，typed_archive=true，submissions/finishes/completed_cases/files=96，recorded/released_handles/replays=48，channel_checks=1572864，guard_byte_checks=786432，readonly_byte_checks由正常cases精确计算（当前7667712）。texture_registrations/registrations_after_record=192，registered_cpu_object_finalizes=240，binding_array_copies=96，inflight_descriptor_updates/backing_inflight_busy_checks/backing_postfinish_recycles=96，authored_mip_uploads=384，selected_mip_changes=48，source_levels=4，all_lods_pixel_qualified=false。errno/cleanup_errno=0且resources_retained=false，否则不能通过。

metadata field typed_archive仅在注册helper实际成功后置true。normal collector有至少12个实质拒绝用例（实际45个，包括typed=false、错误base/count/levels、短图/缺图、alpha差1、RGB超容差、owner/Finish/counters/nonce/UID等），另5个不兼容builder组合拒绝；synthetic collector正例不是GPU证据。正常NDK整库CMake严格链接且检查无Vulkan/GLES/EGL依赖。宏0颜色和旧singlemip BC probe机器码.text逐byte保持。

测试始终使用稳定owner内的opaque keys。不将前包发现的builder栈地址复用side-table问题写成此次已修复；root后续独立修复/证明不属于该公共资格结论。

资源成本仍为前包明确的每submit全mip转换/上传/owner链；本包未声称性能优化。任何失败/unknown Finish由现有owner quarantine保活，不能用停止进程前的缺图或不完整计数冒充GPU通过。
