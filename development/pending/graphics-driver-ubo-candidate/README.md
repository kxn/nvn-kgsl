# VS driver parameters / dynamic UBO 整族 WIP

29 个修改/新文件的实际源码快照与薄diff已保留。全部原始baseline hash在旧提交8f052d4工作树上仍能精确匹配；没有完整冻结ready，没有GPU/Android执行，没有正式宿主feature flag。公共DrawID映射仍明确拒绝。

实现包括真实NIR/IR3 typed driver使用mask、DWORD0..3角色、perjob常量owner与 GPU indirect DST_OFF、真正残余native UBO表、原C1/公共API CB独立映射、正式生产侧recipesidecar与默认OFF runtime。CPU不读取indirect/index BO。原型实际边界见PLAN.md和source-facts.json，不允许用猜测常量补齐。

已完成但范围须区分：authored-runtime-v3真实编译检查输入/产物全一致；original-runtime-normal-v2中25个原VS固定诊断FS均生成recipe，50个native bin与旧编译一致；legacy-runtime-normal-v1的293个显式旧pair逐项重编译/元数据不变，12146输入和4122产物现场hash全部一致。原游戏shader及其衍生产物不公开。

**当前源码没有有效runtime/public HOST成功记录。** runtime-host-v2在当时通过，但6个输入之后已变；public-runtime-host-v2的3个输入之后已变。runtime-host-v3/v4日志为空、没有check.json，不能说最新代码通过。旧public HOST两san各25pair/150mock提交/50mock失败/10负例仅是历史范围。

下一条工作命令（先合并候选diff并正常重建编译器与原创shader）：

```sh
python nvn-native/tools/check_runtime_constants.py --root "$NVN_ROOT" --shader-check "$AUTHORED_RUNTIME/check.json" --fdl-root "$FDL_TOOLCHAIN_ROOT" --fdl-check "$RUNTIME_FDL/check.json" --depth-check "$DEPTH_FDL/check.json" --ndk "$ANDROID_NDK_ROOT" --out "$LOCAL_WORK/runtime-host-current"
```

随后 check_pipeline_runtime_constants.py 重新覆盖25pair公共路径；审核绑定范围/描述符rounding、UNKNOWN保活与direct/indexed/indirect/count整矩阵。再增加正式默认OFF feature和原创整族GPU资格，不能用原25诊断pair HOST代替真实游戏/原GPU。
