> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公开 NVN Draw 到 canonical GOB 真机资格

当前通用实现经实际公开 NVN Program/UBO0/顶点状态/Draw/Submit/Finish 完成三尺寸（64²、127×73、2880×1620）、三顶点布局、四绘制方式及同句柄重放，共 72 次提交与完成、36 个录制/释放句柄。target公开flags9、真实COMPRESSIBLE合格pool及canonical地址getter也经核验。目标存储是显式不使用NVIDIA压缩的 Generic16Bx2，不表示实现NVIDIA压缩或Adreno UBWC。

GOB 源通过 GPU 预转换、Adreno 原生动态绘制、GPU 写回；没有把GOB当LINEAR、没有CPU转换图像上传。离线独立 public-minigbm inverse-sector oracle 核验449180832通道、13437792 canonical padding/outer guard字节及重放保留区域；另有设备端守卫/只读检查。误差域沿用既有RGB≤1/alpha精确规则，实际最大误差见actual.json。真实原始读回按SHA无损去重压缩并保留逐文件清单。

正常producer、当前源码HOST两种sanitizer与NDK API29严格shared-link均通过后，正常APK在DEVICE_SERIAL A650普通应用UID运行。finally_stop及pidof确认已退出。LINEAR24历史资格保持独立，未以其替代GOB资格。本次没有执行真实游戏模块，不是D2R游戏帧；不增加runtime入口数或宣称任意shader/采样资源已实现。
