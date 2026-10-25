> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# C1 FS texture-query source identity (static evidence only)

No GPU, game, ADB, runtime/backend implementation, or frozen raw-pipeline source was used or changed. Original NSO `sdk` was read and all compressed segment hashes were verified before static Capstone disassembly. Original C1 shader blobs are retained under `original/` with unchanged SHA and extraction provenance in `metadata.json`. `c1-original-maxwell.asm` comes from the **existing** vendored shader `InstTable.GetOp` and its actual `InstTlds`/`InstTxq` properties, via reflection against the exact normal frontend DLL. No new ISA decoder/interpreter was implemented.

Confirmed flow:

1. `Program.cs` creates graphics context at0x30; `Translation/Translator.cs:12,44–49` reads an0x50 SPH and starts executable code at0x80. `Decoder.cs:266` skips scheduling words every32 bytes. `ShaderHeader.cs` does not extract a texture-CB selection field.
2. C1 code+0x90 is nonbindless TXQ dimension, `TidB=110`; TLDS at0x150,0x158,0x168,0x170,0x498 uses110 with2D/noLOD/offset forms. TLDS at0x178,0x188,0x190,0x198,0x4a8 uses112 with **Texture1DLodZero**. The frontend shader path interprets `TidB` as a word offset, while default CB is represented by `TextureOperation.DefaultCbufSlot=-1`. `TextureHandle.UnpackSlots(-1, runtimeDefault)` deliberately requires an external default texture-CB index.
3. `InstEmitTexture.cs:475–486` queries the actual texture type for the1D TLDS case to distinguish a texture buffer. The normal fixture's `QuerySamplerType(110/112,-1)=Texture2D` is explicitly chosen test specialization, **not** observed game descriptor state. The frontend also converts supported1D images to a2D native representation, so its final reflected2D type alone cannot reveal the original NVN target.
4. Original SDK scalar bindings encode `LOAD_CONSTANT_BUFFER_OFFSET` at method0x8e3, then write low32 handle bits. `BindTexture` at0x481430 uses base0x20+8*i, SeparateTexture at0x4816a0 uses0x168+8*i, SeparateSampler at0x481b50 uses0x568+8*i. The arrays use identical domain offsets, preserve stride8, and **write both32-bit halves**: SeparateTextures0x481a68–0x481a78, SeparateSamplers0x481f18–0x481f28, Textures0x482158–0x482168. Scalar graphics does not write the high word; compute stage5 records a full64-bit deferred value. These distinctions are preserved rather than summarized as a universal high-bit truncation rule.
5. [NVIDIA's published Maxwell class methods](https://github.com/NVIDIA/open-gpu-doc/blob/master/classes/3d/clb197.h) define `SET_BINDLESS_TEXTURE` byte method0x2608 (word method0x982), whose bits4:0 choose the texture constant-buffer slot. The same header independently defines TIC and TSC pool addresses/limits and sampler-binding mode. [The published texture-header fields](https://github.com/NVIDIA/open-gpu-doc/blob/master/classes/3d/clb197tex.h) distinguish1D,2D and1D-buffer resources, normalized coordinates, and sampler state. The pinned Mesa copies and their exact hashes are in metadata. [Switchbrew's independently recorded NVN method offsets](https://switchbrew.org/wiki/GPU_Classes) agree with the original SDK disassembly.
6. The pool descriptor ID is the handle's low20 bits; sampler ID occupies the following12 bits, as independently established by the original SDK GetTextureHandle and GetSeparateSamplerHandle instructions. None of instruction word-offset110/112, API slot, descriptor ID, native Adreno binding, or GPUVA is interchangeable.

Remaining minimum source-selection gap:

**The original NVN initialization value of SET_BINDLESS_TEXTURE, or an equally direct original-SDK proof that these instruction immediates select driverCB2, has not been located.** Direct literal-header scans were negative; that does not prove no such initialization occurs (it may be included in a larger method sequence or helper). `BindlessToArray.cs` contains a frontend NVN CB2 assumption, but that is not sufficient original-SDK proof. The ShaderHeader does not supply the missing choice. Neither a previous authored T10/S0 qualification nor deko3d's distinct CB ABI is used to fill this gap.

Conditionally, **if that selected default isCB2**, `110*4=0x1b8` matches SeparateTexture API10 and `112*4=0x1c0` matches SeparateTexture API11. Combined API10 would instead address0x70; these are separate tables. Both current C1 resources have integer-fetch/query flags and semantic sampler-mask0, so a separate sampler0 must not be invented as a dependency. Actual pool identity, registered descriptor version, texture target/format and dimensions must be resolved from the recorded/submit-time NVN state. Unknown source selection or mismatched specialization remains a strict rejection, with no archive-first fallback.

Pinned SDK handle carrier details, audited independently of GPU resource support:

| API | SDK address | Legal-positive-ID carrier |
| --- | --- | --- |
| GetImageHandle |0x4660d0|ID (SXTW) |
| GetSeparateTextureHandle |0x466130|ID OR bit32 |
| GetTexelFetchHandle |0x466140|ID OR bit32 |
| GetSeparateSamplerHandle |0x466120|signextend(inputID) then64-bit shift20 |
| GetTextureHandle |0x466150|32-bit(T OR S<<20), signextend that32-bit payload, then OR bit32 |

Thus combined samplers2048..4095 have upper32=FFFFFFFF, while the separate sampler carrier's upper32 remains0 for legal IDs0..4095. Original release code also accepts and signextends negative input values; this is a static observation, **not** a request to relax native legal descriptor-ID policy. Separate pureCPU carrier fixes and typed tests are in the independent ignored `sdk-handle-carrier-candidate/` directory. They do not resolve the C1 CB source gap or add any GPU texture capability.

Reproduce with repository venv (lz4/capstone) and existing dotnet:

```sh
${D2R_REFERENCE_ROOT}/.local/venv/bin/python .local/nvn-native/parallel/c1-handle-source-research/research.py
${D2R_REFERENCE_ROOT}/.local/dotnet/dotnet build .local/nvn-native/parallel/c1-handle-source-research/decoder/Decode.csproj -c Release --artifacts-path .local/nvn-native/parallel/c1-handle-source-research/decoder/artifacts
${D2R_REFERENCE_ROOT}/.local/dotnet/dotnet .local/nvn-native/parallel/c1-handle-source-research/decoder/artifacts/bin/Decode/release/Decode.dll .local/nvn-native/maxwell-texture/artifacts/bin/Maxwell/release/Ryujinx.Graphics.Shader.dll .local/nvn-native/parallel/c1-handle-source-research/original/7c603a02d455070a-1db6c54.code.bin
${D2R_REFERENCE_ROOT}/.local/venv/bin/python .local/nvn-native/parallel/c1-handle-source-research/metadata.py
```

Only metadata and sanitized conclusions may later be committed. The original blobs/raw disassembly remain ignored.
