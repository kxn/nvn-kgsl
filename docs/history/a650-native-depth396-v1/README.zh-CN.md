> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# A650原生深度396次GPU资格

DEVICE_SERIAL的Adreno650普通Android APP_UID上，D16/D24X8/D32F × 两个origin × 11种状态 × 两次同job回放，通过396次真实提交及Finish：132次Draw、132次GPU清除、132次GPU读回。11种状态包括全部8种比较、禁用测试、关闭写入、FS无DATA0。两种尺寸和非全屏scissor分别检查内外区域。

独立NumPy校验264份完整原始读回：1897968个颜色通道、474492个深度像素、2467784个guard字节、21084624个只读字节，最大颜色误差0、最大UNORM深度误差0；D32F按精确可表示值检查。完整读回5947392字节。没有CPU制造/转换深度图像，native深度BO不经CPU读写；读回来自GPU CP_BLIT的实际FDL LINEAR别名。提交时检查owner忙状态，未知完成保留完整owner。结束force-stop返回0，pidof返回1且两输出为空。

首次396次提交完成但独立像素校验失败，完整失败证据和264份raw单独保留。根因是A2D四个源坐标直接写整数，而实际XML规定坐标位于bits8..24，导致读回反复取左上角清值。修复使用原生成packer；非零子矩形、16383端点和旧错误编码负例已加入HOST检查。258个CLEAR包逐字节不变，367个READBACK包只四个源坐标字段改变。修复后使用原396矩阵及原oracle重新构建运行，未提高容差或修改shader/深度状态来过测试。

本资格是原创raw-native测试，不是公共NVN调用或原D2R执行。mode10仍绑定颜色附件，不能据此宣称真正零颜色目标已通过GPU。公共SetRenderTargets、canonical深度load/store和程序目录接入仍待完成；stencil、组合平面、MSAA、UBWC、LRZ和early-Z不在本次范围。没有原游戏首帧或性能结论。

`device-evidence.json`与`full-readbacks.zip`为通过的v2；`failed-device-evidence.json`与`failed-full-readbacks.zip`为原始失败v1。正常输入/HOST/APK记录在`normal-records.zip`。`archive-manifest.json`保存每个ZIP成员的长度及SHA256，全部逐成员解压核验。
