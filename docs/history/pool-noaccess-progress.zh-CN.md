> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# GPU-only pool：同 backing 与真实 shader 采样

2026-10-23，新机A650。此增量继续使用独立原生KGSL驱动；当前真实D2R仍停在DeviceSetWindowOriginMode，**本节是通用NVN资源资格，不是D2R实际pool或首帧通过**。

## 已实现合同

MemoryPoolBuilderSetDefaults按已核原SDK行为恢复0x21，保留device，清storage/size。Initialize在解引用弱device前检查其仍为live对象，接受CPU_CACHED|GPU_CACHED或CPU_NO_ACCESS|GPU_CACHED，另可带SHADER_CODE/COMPRESSIBLE。其它组合仍明确ENOTSUP，尤其没有把匿名WB页伪标为CPU_UNCACHED。

driver导入原caller storage的同一物理页，不换BO、不复制payload。CPU_NO_ACCESS发布前对整个WB backing做私有TO_GPU cache交接。公开PoolMap/BufferMap正常返回NULL且无sticky错误；有效范围的公开Flush/Invalidate为NOP，非法范围仍EINVAL。底层private map保留，不能将公开可见性误传成底层没有CPU地址。原SDK方法表、flags分支和默认值的逐项证据见 [研究记录](pool-implementation-next.md)。

COMPRESSIBLE bit完整保留；当前选择**native-uncompressed**存储。此策略不表示已经实现硬件压缩、Maxwell压缩字节格式或任意纹理layout。纹理仍只接受已资格的明确格式/布局。NOACCESS shader源、descriptor backing和command arena暂明确拒绝，以免在缺少私有GPU写后同步/读回能力时读到stale数据或写入不合格arena。

Pool先import、完成私有cache交接，才发布对象及device ref。预分配的回收节点确保初始化失败时仍能保留销毁失败的BO；device持引用，Finalize不能假成功。后续Finish/Finalize可安全重试回收，不丢失owner。

## 实际验证

[正式host检查](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `pool-noaccess-host-proof.json`；未随公开仓库提供） 使用真实tracked core和现host内存backend，GCC ASan/UBSan/泄漏检查与Clang函数类型检查均通过。覆盖8种受限flags、13种拒绝值、Map/范围/NOP、弱device、同storage、初始化cache失败和销毁失败保留、三种usage早拒绝。NOACCESS每次成功初始化恰一次私有clean；公开mapped cache不调用backend。该层无GPU或游戏执行。原device flags/版本的212项回归也对新core重跑通过。

[新机GPU采样](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-noaccess-pool-texture.json`；未随公开仓库提供）：仅资格调用者的POISON池改为0xa1，创建前seed16字节texels；创建后不再CPU写、复制、比较或扫描该pool payload。两种Map合法NULL、两个GPUVA检查、12次合法mapped-cache调用和4次非法范围拒绝通过。第5次draw由原fragment shader实际读取该pool，16384个诊断通道全部匹配；输出仍在独立cached RT读取。四组原shader主用例65536通道、descriptor更新、资源寿命、5次提交及完整清理同时通过。

[纯cached回归](../HISTORICAL_EVIDENCE.md#unpublished-artifacts)（历史引用 `a650-noaccess-cached-regression.json`；未随公开仓库提供） 关闭该资格开关后同样5次提交、四主用例和pool身份诊断通过。两轮各自保留4张主readback和1张诊断readback；对应文件SHA逐项相同。NOACCESS调用者减少了该payload的CPU guards，不能把它计作CPU payload校验通过；其新证据来自GPU采样结果。没有游戏Draw、硬件压缩或完整GPU-only usage声明。

可复跑构建入口：

```sh
python3 nvn-native/tools/build_android_probe.py \
  --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/a650-noaccess-texture \
  --keystore .local/nvn-native/probe/probe-debug.keystore \
  --nvn-texture-check .local/nvn-native/a650-final-textures-check/check.json \
  --texture-device-flags 0x100 --texture-noaccess-pool
python3 nvn-native/tools/run_android_probe.py \
  --serial DEVICE_SERIAL --build .local/nvn-native/a650-noaccess-texture \
  --evidence .local/nvn-native/evidence/NEW.json --install
```

后续先完成窗口原点/深度模式的native命令和录制快照，恢复真实D2R调用链，再按实际pool参数、CPU_UNCACHED/GPU_UNCACHED策略及队列预算继续推进。所有本次probe进程均已退出。
