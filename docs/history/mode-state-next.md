> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Native origin/depth mode: bounded implementation and rejection plan

Read-only plan. No tracked edits/build/ADB/git or GPU tests were performed for modes. The earlier bootstrap unit task has separately passed against the promoted tracked core in `device-bootstrap-tracked-proof.json`; that unit does not qualify these modes. All hardware conclusions below use common Adreno XML and Gallium freedreno, with no Vulkan implementation used as evidence. One overly broad search emitted Vulkan-path hit snippets; those were excluded, the search was narrowed to original Metal/common/Gallium, and no such result is used here.

## The four callable ABIs are now sourced

The public primary [inline header](https://code.botw.link/uking/uking/lib/NintendoSDK/include/nvn/nvn_FuncPtrInline.h.html), source lines 71-104, gives:

* `void nvnDeviceSetWindowOriginMode(NVNdevice*, NVNwindowOriginMode)`
* `void nvnDeviceSetDepthMode(NVNdevice*, NVNdepthMode)`
* `NVNwindowOriginMode nvnDeviceGetWindowOriginMode(const NVNdevice*)`
* `NVNdepthMode nvnDeviceGetDepthMode(const NVNdevice*)`

These are ordinary enum/int ABI types, not flags-getter evidence. `DeviceBuilderGetFlags` remains unresolved. Vendored header enums at `nvn-native/third_party/nvn/nvn.h:382-394` identify origin0 LOWER_LEFT, origin1 UPPER_LEFT, depth0 NEAR_IS_MINUS_W, depth1 NEAR_IS_ZERO. The same four PFNs already exist in the pinned header at lines1136/1137/1143/1144. No ABI extension, names regeneration or typed/UNKNOWN count change is needed. An earlier search incorrectly treated them as missing; that assumption is withdrawn. The current real boundary is a known typed call with no implemented resolver entry (ENOSYS), not UNKNOWN ABI.

Original raw main actual indirect calls are +0x173cc10 origin1 and +0x173cc24 depth1. Static call arguments are known; only a real typed trace confirms execution reaches each one. Parent now reports actual flags0x100, Initialize1 and major53/minor105 passed, followed by SetWindowOriginMode ENOSYS. This is parent-provided device evidence, not a run performed by this research task. Raw/input hashes are in the adjacent JSON and the previous version report.

## Current image is a fixed raw coordinate profile, not implemented NVN mode state

`nvn-native/shader/graphics_artifact.cc:563-598` emits:

| State | Existing value | Meaning supported by source |
|---|---|---|
| VIEWPORT XOFFSET / XSCALE | +32 / +32 | 64-pixel full viewport |
| VIEWPORT YOFFSET / YSCALE | +32 / +32 | Increasing clip Y increases memory row |
| VIEWPORT ZOFFSET / ZSCALE | 0 / 1 | Depth-range mapping for 0..1 NDC |
| GRAS_CL_CNTL ZERO_GB_SCALE_Z | 1 | Half-Z clip volume 0 <= z <= w |
| GRAS_CL_CNTL near/far clip-disable bits | 0 / 0 | Near/far clipping enabled |
| GRAS_SU_CNTL cull front/back, FRONT_CW | 0 / 0 / 0 | No culling, default front orientation |
| RB depth test/write | disabled | No depth attachment qualification |

Public common `a6xx.xml:1543-1549` gives six consecutive viewport registers starting at register0x8010 (not a byte address): XOFFSET, XSCALE, YOFFSET, YSCALE, ZOFFSET, ZSCALE. `a6xx.xml:1422-1436` gives GRAS_CL_CNTL register0x8000, half-Z bit6, near/far-disable bits1/2. `a6xx.xml:1570-1591` gives GRAS_SU_CNTL register0x8090, CULL_FRONT bit0/CULL_BACK bit1/FRONT_CW bit2. Gallium `fd6_emit.cc:474-487` emits the six viewport floats; `fd6_rasterizer.cc:43-55` assigns half-Z directly from `clip_halfz` and front-CW from `!front_ccw`.

The existing CPU oracle explicitly assumes this **raw +Y** mapping (`tests/texture_cases.h:98-101`; `tests/graphics_probe.c:76-78`). Both original VS pairs use the matrix's .5*z-column contribution. For the existing identity test matrix their clip Z is **.5**, W=1; they do not distinguish [-w,w] from [0,w]. An earlier message incorrectly said the untextured identity Z was zero; it is .5, and that statement is withdrawn. Neither FS uses FragCoordZ/front-facing and there is no depth attachment, so existing four-case passes cannot certify a general NVN default depth0 implementation. Changing a stored enum alone would silently retain a raw half-Z image.

Original Metal anchors: `dm/gfx/metal/test_offscreen.c:5,11` defines top-left memory row mapping `py=(1-ndcY)*H/2`; generic Metal drawing `nvn_mtl.m:4780-4792` passes viewport/depth range and declared winding to Metal. `hle_nvn.c:241-242` only stores/logs mode values and is not evidence the full mode0 behavior was implemented. Do not copy that state-only treatment into the native Draw path.

## Small native patch candidate; no shader rewrite required for these pairs

For the currently qualified 64x64/full-scissor/near0-far1, no-cull, no-FragCoord/front-face, linear RGBA8 profile:

* Origin0 raw lower-left candidate: YOFFSET=32, YSCALE=+32. Origin1 upper-left candidate: YOFFSET=32, YSCALE=-32. This matches the relative Y inversion and original Metal top-left oracle. This mode1 mapping remains a candidate until actual native image tests pass; it is not qualified by the old +Y pass.
* Depth0: clear ZERO_GB_SCALE_Z, ZOFFSET=.5, ZSCALE=.5. Depth1: set ZERO_GB_SCALE_Z, ZOFFSET=0, ZSCALE=1. Keep near/far clipping enabled. For general near/far n,f the algebra is mode0: (zoffset,zscale)=((n+f)/2,(f-n)/2), mode1: (n,f-n), but arbitrary depth ranges remain outside this subset.
* Keep full scissors at [0,63] for the first subset; their full-frame bounds are invariant. Nonzero viewport/scissor Y and partial scissors require an explicit origin-space-to-memory-space mapping and separate tests. Existing core rejects them already at `nvn.c:1007-1025`; retain that boundary.
* Y inversion changes signed screen area. A future full implementation must establish and encode the corresponding front-face convention (possibly FRONT_CW compensation) together with cull state and FS front-facing reads. The current subset has no culling and rejects `fs->frag_face`/FragCoord (`graphics_artifact.cc:74-80`), so changing FRONT_CW cannot be observed by its existing shaders. Do not label any guessed bit flip as qualified face-winding semantics. Keep culling/front-facing consumers unsupported until a discriminating original-shader test exists.
* Existing frontend specialization has `QueryHostSupportsDepthClipControl=true` and graphics `depth_mode=true` (fixture). Translator `EmitterContext.cs:307-314` inserts a Z remap only when the host lacks depth-clip control; it therefore leaves the current original position unmodified. Retain that source/profile fact when selecting hardware clip mode, to avoid a double conversion. The translator bool is not the numeric NVN enum and must not be assigned blindly from it.

The current image ABI supports only address patches (`graphics_image.h:8-56`, `graphics.c:96-123`), each patch writes two address dwords. **Do not misuse those patches for scalar state or scan arbitrary command words at runtime.** Extend the image with an explicit, separately validated scalar-state patch schema or a distinct new schema, emitted at generation time. Record for each patch its value-word index, expected type4 target register, allowed mask, baseline bits, state kind, and exact chip qualification. The builder knows the offset before each `reg()` call (`graphics_artifact.cc:193-194`); it can declare the YSCALE, ZOFFSET, ZSCALE and CL_CNTL slots without a new PM4 parser. Existing common pm4 type4 encoders suffice; no Gallium runtime import is needed.

At native encode, validate schema/chip/word bounds/expected register and allowed masks before modifying a **private job copy**, preserving unrelated CL_CNTL bits (especially A7 required clamp handling). The original compiler image and prior inflight jobs must remain immutable. Treat A650 and A735 as distinct hardware qualification targets; XML commonality is not a new A735 pass. Existing schema1/2 raw images retain their previous bytes and raw-profile meaning; they must not be relabeled as mode-aware by adding API state alone.

Core currently materializes untextured jobs during Draw (`nvn.c:1072-1081`) but textured jobs late during enqueue (`late_draw` at 1052-1056; `materialize_draw` path). A mode-aware design must bind both to the same explicitly documented lifetime rule: preferably allow device modes before any graphics registration/recording, then reject changes while graphics objects/handles/jobs are live until runtime mutation semantics have been researched. Alternatively snapshot mode into each recorded draw/private job and prove later device mutation cannot change old recorded/inflight rendering. Do not read the latest mutable device field only in the textured enqueue path while untextured commands use the old recording state. Public setter signatures alone do not establish all temporal semantics; early-only mode setting is an honest first restriction, and matches these original early bootstrap calls.

## Cheapest honest boundary if native patch qualification is deferred

An API can represent/query supported enum state without GPU work, but that only qualifies the state API. Before accepting modes1 as a drawable configuration, either implement/qualify the above mode-aware schema or return ENOTSUP. If root elects to accept bootstrap setters/getters early, mark them **state-only with rendering guarded** and add a mandatory guard at graphics registration/Draw before allocating jobs, adding resource refs or increasing command bytes. Any requested mode not explicitly described by the selected image must fail; never silently reuse the fixed raw image. Do not claim the existing raw image is a fully qualified default(0,0) NVN image merely because its former tests passed.

For the minimal incremental real-game run, keeping origin/depth unsupported remains an acceptable explicit next boundary until this guard/schema is present. It is unnecessary to undertake general viewport/culling/depth-buffer support just to store two bootstrap scalars. If setters are accepted with a draw guard, evidence must call it state acceptance and record the next semantic boundary, not native mode rendering.

## Discriminating validation with existing original shaders

Proposed tests, not executed here:

1. Mode APIs: valid device/default enum policy, set/get roundtrip for0/1, invalid enum/null/wrong-kind/finalized device errors preserving old state, early-only mutation or snapshot behavior, and strict unknown ABI regression. The public enum defines values but does not independently document initialize defaults; declare any chosen default0/0 as project profile policy until a primary default is sourced.
2. Origin: rerun the actual original full-screen RGB interpolation case with origin1. At memory pixel(x,y), expected green=(x+.5)/128, expected blue=(63-y+.5)/128 and red=1-green-blue. This is the old original-shader closed form evaluated at row63-y, not a CPU output copy/flip. Keep the same unmodified shader, VBO, output layout and guards. For the original textured cases, evaluate the existing closed-form oracle at (x,63-y); this moves UV/color/alpha-discard geometry together and should preserve aggregate counts while changing rows. An image-comparison test accepting either orientation would invalidate the test.
3. Depth clipping: use the same original VS and matrix UBO, replacing row2 with {0,0,0,-.5} and keeping W=1. All vertices then have clip Z=-.5; depth0 must render and depth1 must leave the clear image. Positive Z=.5 renders in both; Z=1.5 clips in both. This tests clip volume without adding a shader or depth attachment. Uniform-buffer data is test input, not a game-state patch.
4. Test the scalar patch declarations offline, including expected-register/mask bounds, malformed duplicates, unknown mode/image schema, and old schema immutability. Verify wrong-mode rejection does not increase recorded bytes or resource refs.
5. Since neither current FS reads fragment Z nor any current image writes a depth attachment, these cases **do not GPU-verify Z viewport scale/offset or depth buffer comparisons**. Their algebra/packet state can be checked offline, but real depth output qualification is a later task. No new shader is proposed to fill that scope silently.

This plan is bounded and implementable for mode1 by a handful of declared scalar patches plus state lifetime/guards. Full NVN mode semantics remain larger than those patches; the report names the exact unsupported surfaces instead of implying a full implementation.
