# 公共 depth-source SAMPLE/Gather144 资格 WIP

13 个实际变动文件与薄diff已保留；尚无最终ready但实际 inputs-v3、host-v2、正常 APK-v1 已完成。inputs-v3 7384输入/12产物，host-v2 7914输入/418产物现场hash重验全部一致；未安装/运行Android。host-v1 的target95失败保留为历史原因，不当新资格。

这项不只薄测试hook：**src/nvn_pipeline_draw_record.inc 新增一个生产修改**，仅在 NN_NVN_COLOR_TARGETS 开启时移除旧的full-target-only scissor限制；已有边界检查与Direct/Call/Copy矩形传递保留。合并前单独审核该修改；当前源码HOST两san各144实际提交IB矩形与432越界scissor拒绝通过，不代表GPU结果。

2个原shader角色SAMPLE/GatherR+FragDepth ×3源depth格式 ×2几何/选定mip ×2origin ×3Direct/Copy/Call路线 ×2samehandle重放，目标144Draw/Submit/Finish/完整读回。HOST oracle的144raw/1020816通道/255204深度像素是明确synthetic_only，不能写成实际GPU通过。SAMPLE显式R001，Gather只用R；不声称比较采样、默认G/B/A、FETCH/SIZE公共GPU或实际D2R捕获绑定。原完整control/C1及衍生shader只在私有外部输入生成，不发布。

合并当前源码后先重建正常inputs；sample-pair49仅对原完整55stage/96pair包成立，必须由实际身份验证，不能盲用另一个包的index。

```sh
python nvn-native/tools/public_depth_sampled_qualification.py --root "$NVN_ROOT" --pipeline "$PRIVATE_CURRENT_PIPELINES/check.json" --fdl "$RUNTIME_FDL/check.json" --depth "$DEPTH_FDL/check.json" --a2d "$DEPTH_A2D/check.json" --runtime "$DEPTH_RUNTIME/check.json" --block "$BLOCK_COPY/check.json" --sample-pair 49 --out "$LOCAL_WORK/public-depth-sampled-inputs-current"
python nvn-native/tools/check_public_depth_sampled_probe.py --root "$NVN_ROOT" --check "$LOCAL_WORK/public-depth-sampled-inputs-current/check.json" --ndk "$ANDROID_NDK_ROOT" --out "$LOCAL_WORK/public-depth-sampled-host-current"
```

然后正常APK构建、唯一设备负责人执行GPU144/收全raw/独立oracle/strict stop。旧完整构建记录保留本地；公开manifest只有hash和安全范围，不能把它重新标成新repo当前证据。
