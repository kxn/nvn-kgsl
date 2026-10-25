> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Graphics control 范围审阅：冻结

结论：**正常 graphics producer 没有统一截取 0x880 的缺陷，已经按容器声明完整支持 0x880/0x898。当前资料不足以生成真实 graphics caller 的完整 0x898 身份，故本包不提供补齐/追加 control 的生产补丁。**

只审阅 Python/C# 工具及既有 normal metadata/运行观察。没有读取、反汇编或执行原 SDK CPU 指令，没有读取 Metal/Vulkan renderer，没有 ADB、真机或生产文件改动。旧 compiler/check/fixture 记录保持原样。

## 确证路径

| 入口（相对 worktree） | 事实与边界 |
| --- | --- |
| `nvn-native/tools/offline_graphics_stages.py:15–20` | 核验 NSO/raw/三个段哈希，限定容器来源范围。 |
| 同文件 `:27–40` | 读 v5 容器的控制字节数 `n`；只允许 0x880/0x898；核对 signed relative code/control 地址和全部范围，然后 `control=raw[co:co+n]`。保存 profile control_extent 与 caller_control_bytes。此标签证明的是静态序列化容器声明，不是运行时任意 caller 分配大小。 |
| `nvn-native/tools/merge_graphics_inventories.py:13–22` | 核输入哈希后复制 fixture 引用，不改 shader/control 字节。 |
| `nvn-native/tools/shader_fixture.py:51–75` | 按显式 blob offset/size 完整读取并核 SHA；control 的全部字节和长度参与 request identity。provenance 自述不等于语义证明。 |
| `nvn-native/shader/maxwell/Program.cs:163–172,187–202` | Blob 完整读取并核 SHA；control 仅要求非空，不在此解析 tail。translator 读取 code/C1。因此 SPIR-V 翻译成功本身不能证明 control 分配范围或 tail 语义。 |
| `nvn-native/tools/package_graphics_stages.py:94–105,112,120–121` | 对照 envelope 完整 source SHA/size；严格校验声明范围、实际 control 长度和 code/C1 关系；生成 control 数组以及真实 control_bytes。没有补零、截短或归一化 tail。 |
| `nvn-native/adapters/d2r/extract_embedded_graphics.py:45` / `extract_embedded_textures.py:59` | 历史固定资格 extractor 确实取 0x880；与当前 generic normal inventory 路径分开。不能用修改历史 blob 的方式冒充新的 runtime full-control 证据。 |

`audit.json` 记录精确源码 SHA 和现有输入记录。`graphics-production-enqueue-stages-v1/check.json` 的 12 个 specialization 对应 5 个 distinct graphics 容器，control 全部为 2176（0x880）；C1 FS 不例外。生产 inventory 的 16 条记录也全为 0x880。这说明现有静态源声明的范围；不能由此断言 runtime caller 一定不再另行构造控制对象。

既有 v8/v9 日志分别只有四个已知 compute 初始化的 ProgramSetShaders 调用，raw700/raw704=256 且明确 lengths_validated=false。它们没有给出 graphics control 0x898 的 caller 分配/拷贝范围。这里仅登记日志哈希和计数，不复制控制 hex、原始 shader 或全局日志。计算着色器此前独立建立的 full898 caller 证据不自动适用于 graphics。

## 74 项可重复的 authored metadata 检查

```
python3 .local/nvn-native/parallel/graphics-control-extent-audit/check.py \
  --root "$PWD" \
  --out "$PWD/.local/nvn-native/parallel/graphics-control-extent-audit/check-new"
```

直接调用当前 normal extractor 和 fixture validator。原创 VS/FS 容器分别声明 0x880/0x898，验证精确输出；0x880 后相邻的 24 字节没有被吸收。对 0x898 tail 的每个字节逐一修改：旧 SHA 均被拒绝；重新计算合法 fixture SHA 后 request key 均改变。还检查截短、额外拼 24 字节、未知声明长度、错误相对地址和范围。

package 范围检查通过解析现有源码 AST，执行其**原有单个 provenance guard**，没有复制另一份谓词实现。它不是完整 frontend/package/compiler 执行；不创建冒充 compiler 成功的 check。`check-v1/proof.json` 明确限定这些范围。没有 NDK/runtime 变更，因此本包没有声称 Android ELF 或 GPU 资格。

## 最少补采字段

1. 关联同一 graphics ProgramSetShaders item 的 stage、完整 code/C1 来源范围与哈希；不能靠 guest 地址或旧 hash 分支决定长度。
2. 来自可信原 caller 的 control 大小与依据（实际序列化容器声明、原分配/拷贝长度或对应 owner 范围）。完整安全读到 0x898 只说明可读，不证明最后 24 字节属于对象。
3. 只有上述范围已成立，才采完整 control、完整 SHA、0x880 prefix SHA、24-byte tail SHA，以及 full-read/stability 状态。tail 按原始 opaque identity 保留。
4. 若 caller 从声明 0x880 的容器构造了另一个 0x898 对象，需要明确构造/分配来源和最后 24 字节的来源。不能取紧随其后的容器字节、猜零或拼 wrapper。
5. NVNshaderData 的 0x18 包装结构与其 control 指针指向的内存必须分别标记；0x18 包装大小本身不是 control suffix 的证据。

拿到这些证据后，已有 extractor 对“真实声明 0x898 容器”的路径无需扩容；若是运行时构造的另一种对象，则应新增独立、可复现且有完整来源的 capture/profile，保留历史 0x880 记录。未知采样资源映射、C1 query 和 native shader/pipeline 资格不因此放宽。
