> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公开 texture binding / TextureView 参数观察（冻结候选）

仅 `minimal-promote.patch` 三文件：既有 typed bridge、一个 authored HOST 夹具、既有 `check_program_observation.py` 测试入口。生产代码/工具没有修改，原游戏/Android/GPU/ADB没有执行。没有新 NVN API 实现、成功 stub、getter额外调用、资源替换、计时或 opaque 对象解析。

## 捕获合同

* `nvnTexturePoolRegisterTexture`：pool/id/texture/view 的原参数身份。明确打印 `view_fields=opaque_not_inspected`。
* `nvnCommandBufferSetTexturePool`：command/pool身份。
* `nvnCommandBufferBindSeparateTexture(s)`：command、明确 separate_texture domain、原stage/index或first/count、完整64位struct handle；plural还记录原array地址。
* TextureView 的 SetDefaults/Levels/Layers/Format/Target/DepthStencilMode/Swizzle：记录原公开 setter 参数，均在真实typed forward lookup前。SetDefaults仅记录该操作，不捏造/回读默认字段。

仅 public 8B handle 数组可借已有 `NN_D2R_PROGRAM_INPUT_OBSERVE→dm_self_read` 安全读取：count1..32，地址非空/不溢出，单次≤256B，全读成功才输出所有element。count0为空；超范围、无reader、无效地址、read失败或shortread均只记unknown；不改原参数，不替caller校验。没有读取pool/texture/view opaque字节。view按值复制或没有见到setter时，最终字段仍unknown，不能从pointer相同或先前日志推定完整对象。

观察保存 incoming errno，完成日志/optional reader 后恢复，再调用原typed forward；保留driver产生的新errno。已有sticky不清零，且跳过reader，仍由原require_driver截断。reader失败不会伪造NVN诊断；void/bool/zero规则未改。没有C返回地址伪装guestPC，均为guest_pc=unavailable。

这份trace只能为真实caller的SeparateTexture10/11关联pool/id/texture及其公开builder/view setter历史；C1的112究竟BUFFER/1D仍待实际capture，不能拿原创测试句柄或先前2D测试值当结论。

## 验证

`proof-final/proof.json`：既有program观察/安全reader/core-source测试全部通过；新增default-off / reader-on两组，每组Clang function trap和GCC ASan+UBSan各412检查（共1648）。覆盖11个PFN、全64位handle、负/异常scalar原样转发、count-1..33、NULL/overflow、full/partial/error reader、已有sticky/缺driver/调用后sticky、原errno保留与driver errno输出。原setter/view/pool参数均为原创HOST值，无ROM数据。

同一入口实际NDK API29编译 default-off/reader-on/production-HLE；`elf-audit.json` 核AArch64+swforce，production未绕过原HLE_FORCE。实际core-source Android链接通过。`classification-audit.json` 使用当前真实runner纯函数：普通typed trace/reader errno14不算NVN终止边界；只有真实终止marker且进程死亡才判boundary，Activity退出码仍unknown。

```sh
python3 nvn-native/tools/check_program_observation.py \
  --names VERIFIED_NORMAL_NAMES/names.inc.h \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out FRESH_IGNORED_PROOF
```

不新增CMake宏；原host/Activity已启用的PROGRAM_INPUT_OBSERVE复用原bounded reader。Standalone bridge默认仍没有dm依赖；数组值只记unknown而原forward照常。全部三个文件和原基线、补丁dry-run、proof/ELF/classifier哈希见 `freeze.json`。当前root应用dry-run全成功；应等待root真实游戏本轮结束后再提升和正常重建，不能让旧APK/host记录继续当当前source闭包。
