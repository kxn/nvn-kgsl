# Graphics storage 正式宿主开关候选

8 个文件，基于旧提交 8f052d4。当前基准下 git apply --check 通过。新增默认 OFF 的 --graphics-storage/CMake 开关、正常构建记录与严格依赖核验；不改 SSBO runtime/compiler 语义。已有实际 API29 ABI1/ABI2 OFF/ON 四种 NDK 链接，40 assertions /37 rejects；每种使用原36个宿主/HLE平台源码，无旧渲染器。links-v3 的8355输入和22产物 hash 现场重验全部一致。policy-v1 是早期历史记录，其中 checker 已变更，不能当当前资格。

下一步：合并8文件，然后用当前路径重新生成 feature check。这个检查需要本地私有原验证工程的 names 和正常 pipeline 包；它们不公开。

```sh
python nvn-native/tools/check_graphics_storage_feature.py --root "$NVN_ROOT" --graphics-check "$PIPELINES_WITH_STORAGE/check.json" --no-storage-check "$PIPELINES_WITHOUT_STORAGE/check.json" --fdl-check "$RUNTIME_FDL/check.json" --names "$PRIVATE_NAMES" --ndk "$ANDROID_NDK_ROOT" --out "$LOCAL_WORK/formal-storage-current"
```

之后再从未改写的原 ExeFS 经外部验证工程的统一入口重建原宿主/APK。不能重用候选旧 APK 或改写旧记录 hash。注册 NVN API 数量增量0；已有 nativeSSBO192 GPU证明是另一项证据，公共SSBO像素仍未资格化。
