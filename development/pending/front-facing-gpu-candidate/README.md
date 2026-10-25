# FrontFacing 原创 raw-native GPU 资格候选

10 个文件：7个新测试/工具文件 +3个共享正常构建/运行 hook。正式 emitter/runtime/原NVN源码都已在基准主线，本候选不再改它们。ready SHA789f660f9443aa52e79ad57d664be0d1ffe77b0efc8381d37ab62a3fa6511725；薄patch SHAa4ce5d04c4b751f3cb82c76148ffdb7918635a03264629d1b9c0a4f52bdc1543。qualification-v4 2494输入/61产物、host-v4 2760输入/10376产物重新核验一致；正常APK-v2存在，未安装。

**当前git apply --check不能通过共享 build_android_probe.py hook。** 只语义合并相应hook，保留 Source/SSBO/depth 已有实现，不把旧完整文件覆盖回去。源码before/after精确hash保留于source-files.json。

整组资格目标是3个原创 GLSL角色 ×64状态 ×2绕序重放，共384Draw、640Submit/Finish、512完整raw。HOST完整矩阵、未知完成保活/构造失败/oracle负例及API29实际KGSL链接已过，尚无设备结果。预计颜色通道5161984、D32F像素383104；不能把这些预期数当实测值。

合并后第一条正常重建命令：

```sh
python nvn-native/tools/front_facing_qualification.py --root "$NVN_ROOT" --compiler "$IR3" --spirv-tools "$ANDROID_NDK_ROOT/shader-tools/linux-x86_64" --fdl-check "$RUNTIME_FDL/check.json" --depth-check "$DEPTH_FDL/check.json" --a2d-check "$DEPTH_A2D/check.json" --runtime-check "$DEPTH_RUNTIME/check.json" --out "$LOCAL_WORK/front-facing-inputs-current"
```

然后运行 check_front_facing_probe.py、build_android_probe.py --native-front-facing-check，最后由唯一设备负责人用正常runner安装/运行/全raw验证/停止进程。原12组FrontFacing FS仍在未知texture106拒绝，SampleID未支持；本资格不能代表公共NVN或D2R。
