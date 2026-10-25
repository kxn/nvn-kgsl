> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Native sampler state candidate

Ignored only, based on current frozen tracked source; no tracked edits, Git, ADB, install or GPU execution. No Metal/Vulkan implementation, graphics API runtime, CPU image producer/copy or shader modification. No change to graphics_image.h, the IR3 producer, schema or its source/input closure.

## Implementation / merge boundary

Eight-file minimal patch: CMakeLists.txt (+src/sampler_state.c), graphics.h/c (one explicit runtime sampled-render API), src/nvn.c (semantic validation/setters and current registered sampler snapshot→encoder), new sampler_state.h/c and two HOST tests. No queue/backend/window/texture layout conversion changes.

`nn_sampler_state` is a fixed plain value, uses NVN numeric enums and retains LOD/aniso/etc metadata. `nn_sampler_state_validate` is independent of shader identity. `nn_sampler_state_encode(chip,state,layout,out[4])` emits one Adreno sampler with no allocation or partial output on error. Independent layout requires normalized2D, one mip/sample, nondepth/noninteger, extents1..16384; no2x2/game/hash definition is embedded. Current image frontend still restricts its own actual texture layout, unchanged.

`nn_gfx_encode_sampled_render(device,image,buffers,draw_state,optional_draw,render,sampler,job,bytes)` is the only added graphics entry. For sampled schema3/A650, it zeros the job-local two-slot sampler table and writes the actual encoded state into slot1. Slot0 remains a zero hole. All old raw/render encoders still take no runtime sampler and copy the original artifact's bytes exactly as before. Typed GPUVA table patches and existing CP_LOAD_STATE/invalidation protocol remain unchanged. No random command-word scanning or shader edit.

The core still uses CPU software descriptor tables and full descriptor_entry value snapshots, not guest Maxwell TIC/TSC bytes. SamplerInitialize now accepts supported default LINEAR/REPEAT values; setters validate a temporary full value and commit atomically. RegisterSampler/RegisterSamplerBuilder copy the initialized object's/current builder's full state. A subsequent builder change or SamplerFinalize does not alter registered state. At enqueue, materialize_draw checks the current32B SW publication against its metadata, translates its full immutable snapshot and writes a new private GPU table. Re-registration before a later enqueue is visible by existing semantics; already generated/inflight job descriptor bytes do not change. SPOOL/TPOOL refs remain CPU lifetime refs, not GPU objlist resources. No new owner or borrowed pointer crosses submission.

## Encoded supported range and strict boundaries

- Min filters: all six public NVN NEAREST/LINEAR and nearest/linear mip selection variants. Mag:NEAREST/LINEAR. The descriptor emits the corresponding actual image/min/mip fields. Current resource layout remains one mip; this does not claim multi-mip images or chains.
- Wrap S/T/R:REPEAT, MIRRORED_REPEAT, CLAMP_TO_EDGE independently. CLAMP, border modes and mirror-clamp modes reject; no emulation/implicit border allocator.
- Compare mode NONE only; valid inactive compare-function metadata1..8 is retained. Shadow compare sampling requires depth view and shadow shader operation not present in this profile, so it is rejected.
- Reduction AVERAGE only; no MIN/MAX reduction or LOD snap. Border must remain zero because border address modes are unsupported.
- Finite ordered LOD bounds in[-1000,1000]. Before conversion the physical ufixed12/radix8 descriptor fields saturate to[0,4095/256]; logical metadata is retained. All current resources have one physical level; resource MIPLVLS remains0. This is not an arbitrary mip-layout implementation. New default0/1000 and old explicit0/0 or-1000/1000 are representable without integer wrap.
- Signed bias only[-16,4095/256], truncation matches original Mesa generated fixed13/radix8 pack. NaN/Inf/out-of-range rejects before casts. Exact anisotropy powers1/2/4/8/16 map to known hardware enum; linear image filters use ANISO when requested, nearest remains nearest as in Gallium. Fractional/nonpower values explicitly unsupported rather than guessed rounding. Source support/encoding is not already completed GPU quality qualification.

## Primary sources

Pinned Mesa26.2.5 fd6_texture.cc37–78:repeat/edge/mirror mappings and nearest/linear/aniso image filters;253–301:actual A6/A7 fourDWORD sampler state, mip near/far, bias, LOD, chroma-linear. a6xx_descriptors.xml10–46:all exact fields, widths and fixed-point radix; a6xx_enums.xml386–412:filter/wrap/aniso/reduction values. Tests regenerate adreno_common/a6xx_enums/a6xx_descriptors headers using the existing original Mesa gen_header.py, require exact equality to checked Mesa build headers, then compare actual native output to generated field packers. No Gallium runtime is linked. Detailed primary inventories/SHA are in proof/pack-generator.json and freeze.json.

## HOST and strict NDK proof

`proof/host.json`:ordinary/ASan/UBSan each32400 generated-packer combinations/64849 assertions, plus real core sampler/mode frontend211 assertions. Includes default LINEAR/REPEAT initialization, atomic rejected setter, finalized source sampler lifetime, full registered snapshots, current-slot re-registration at enqueue, actual job-local descriptor changes, unchanged earlier job, hole0zero, original mode snapshots, metadata-only resource roles/pins and SW table corruption rejection. These tests do not execute PM4/shaders/pixels or pretend GPU filtering.

`proof/ndk-2/proof.json`:normal CMake builds the actual core/KGSL/graphics/sampler plus existing original texture API probe with exact14 CPU software tables, ARM64 NDK API29 strict full shared link --no-undefined. Original GPU test source and shaders are reused unchanged, no modified test shader. No installation or device execution. Source fingerprints checked across build. The helper build dependency is public NVN header+libc only; XML generated headers are test-only.

## Parent GPU qualification design (bounded batch, original sampled VS/FS)

Reuse existing check_textures/graphics_api_bundle and native NVN texture probe lifecycle/guards/NO_ACCESS resources. Record the original sampled shader once and update actual public sampler slots between submissions, keeping previous version BO immutable until Finish. Keep independent original shader FS post-sample formula (gamma/contrast/mix/discard) from texture_cases.h; change only the analytic sample tap stage of the reference, not shader/game code.

First batch:NEAREST/NEAREST and LINEAR/LINEAR × S/T wrap3×3 =18 distinct sampler states. UV affine input `u=-3/8+scale*(x+.5)/64`, `v=-3/8+scale*(y+.5)/64`; scale1 gives magnification, scale64 gives minification. Binary fractions avoid shader-coordinate ambiguity. Vertex positions/matrix/color remain original full-screen inputs, alpha255 texels for first batch avoid conflating filtering and discard. Distinct positive RGB2x2 texels make wrapped/linear outputs visibly different. For nearest, index=floor(u*2). For linear, p=u*2-.5, base=floor(p), weights=fraction(p), sample four independent addressed taps. Repeat:integer modulo2. Mirror-repeat:modulo4 thenfold3-index for index≥2. Edge:clamp0..1. Perform unchanged original FS formula after the sample; no CPU output producer/copy. Predeclare nearest tolerance from existing original-shader oracle; linear may require documented ≤2 UNORM channels for final quantization, never a permissive orientation/color fallback.

Add asymmetric min/mag NEAREST/LINEAR versus LINEAR/NEAREST at both scales to prove the two independent fields are consumed, not merely stored. Bias test uses scale16 (lambda=-1) andbias0/+2 to move across the min/mag regime without changing original shader. Keep single resource mip; no mip-chain qualification claim. Aniso powers have exact descriptor proof but non-isotropic filtering quality needs a separate source-backed visual/invariant qualification; do not label fields tested by this first18-state batch as full anisotropic quality support.

Every case must collect fresh exact regular64RGBA files, actual registered-state metadata, source/artifact/UID/nonce identity, target guard and original command/code/input readonly checks; rejected/unknown submit keeps all owners. Reuse existing app worker and runner force-stop discipline. Root alone implements/promotes/runs this actual GPU batch after current real D2R freeze. No sampler GPU result is asserted in this candidate.
