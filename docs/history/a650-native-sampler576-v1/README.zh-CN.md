> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# A650 mip/LOD 576次GPU结果：原精确oracle未通过

实际FDL 127×73七mip纹理，六种minfilter、三种wrap、两个实际base/count视图、八种LOD/bias/clamp状态，共288个job各回放两次。DEVICE_SERIAL普通Android APP_UID完成576次真实提交及Finish，收齐576份完整raw。真实生产`sampled_color`角色、sampler helper和公共层将使用的编码器参与测试；没有修改PM4包来绕过预检。测试shader是原创隐式SAMPLE，不能作为公共NVN/Maxwell/原游戏shader的证据。

原oracle要求nearest误差0、过滤误差最多2个RGBA8单位。全量离线归并显示minfilter0/1/2/3全部符合该标准；三线性min4有27份raw/2809通道超限、min5有16份raw/799通道超限，最大误差3。失败集中在没有被descriptor LOD硬clamp的隐式fractional LOD，硬clamp的对应组通过。拟合实际两层权重只能辅助定位，不能作为独立oracle或改阈值依据。

因此`native_pipeline_verified`仍为false，原失败没有重标。后续将核实并优先使用Khronos实际GLES3 CTS离线纹理验证器的既有精度模型；GL参考工具不等于NVN行为规范，相关结论须单独记录。当前不能声称整个576资格通过，更不能用此前Gather或颜色目标结果代替公共22格式采样资格。

`device-evidence.json`、`full-readbacks.zip`和正常shader/HOST/APK记录保留原始结果。`archive-manifest.json`记录所有ZIP成员长度及SHA256，已逐成员解压核验。结束force-stop返回0、pidof返回1且两输出为空；测试机未持续空跑。无原游戏执行、首帧或性能结论。
