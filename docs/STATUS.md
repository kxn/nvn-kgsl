# Verified status at migration

Baseline: integration source commit `8f052d4` on 2026-10-25. This is a scope report, not a claim of NVN conformance. Historical reports may describe earlier progress; this page supersedes those status statements without rewriting their evidence.

| Domain | Verified scope | Limits |
| --- | --- | --- |
| Resolver | 406 registered entries; 102 known missing; 15 unresolved ABI | Registration counts include metadata/state calls and partial consumers; no percentage of full semantics is justified. |
| KGSL | Ordinary Android app allocation/import, context, real PM4 submissions, completion, readback and guarded ownership; A650 chip `0x06050002` is primary | Exact device/kernel/heap behavior is qualified; do not generalize to all Adreno revisions. Initial A735 results are separate. |
| Window/lifecycle | Authored source-backed NVN Window tests: 2/3 buffers, requested intervals 0–5, two generations, 240 presentations | Authored pictures are not a game frame. Two-buffer interval 0 uses effective FIFO1; incomplete FrameTimeline capture does not prove full cadence conformance. |
| Color/clear | Public NVN 22-format clear and color-target families, respectively 536 and 132 real submissions | Not arbitrary formats, MSAA or all combinations. |
| Ordinary sampled 2D | Public 22-format source-backed SAMPLE family: 264 submissions/Finish, 4,325,376 channels, max error 1 | Explicit resources, nearest/clamp profile; not all texture dimensions, integer roles or unknown handles. |
| Gather and depth writes | Public source-backed GatherR + FragDepth: 72 submissions/Finish/Draw, 72 full readbacks, 255,204 depth pixels; D16/D24 max error 0, D32F exact | MIN/MAX reduction does not establish Gather lane order; not all offsets, compare modes or components. |
| Canonical depth | 648 real submissions, 108 draws, 180 complete readbacks | Exact supported source formats and transfer/profile boundaries; no blanket stencil/MSAA qualification. |
| Native depth sampling | 60 draws, 180 submissions, 60 complete readbacks; D16/D24X8/D32F; 188,400 channels max error 1 | Authored raw-native qualification. Public original-source depth sampling has HOST coverage but still needs its dedicated GPU qualification. |
| Native graphics SSBO | 4 resource roles × 4 VS/FS offsets × 3 ranges × 2 odd image sizes × 2 replays: 192 submissions/Finish, 576 complete BO readbacks; 4,416,768 channels error 0 and 86,016 storage words exact | Raw-native authored shaders, not public NVN SSBO pixels or a game draw. Dynamic bindings, atomics, unsupported widths and out-of-range roles remain rejected. |
| Source identities | 6,903 private original identities; deep-owned selected code/control/C1; unknown compiled variants reject before allocation | The 77,105,536-byte game-derived source blob is not public and is not a compiled-shader support count. |
| Compiled catalogue | 55 explicit stage specializations, 96 successful explicit pairs; 51 historical specializations/188 attempts restored with old successful machine-code bytes unchanged | Explicit pairs only, no invented Cartesian combinations; unsuccessful attempts remain unsuccessful. Game-derived bins are not public. |
| FrontFacing | Independent compiler/runtime implementation, HOST matrix and API29 linking | GPU qualification remains pending at this baseline. |
| Sampler mip precision | 576 raw GPU results preserved; independent unchanged Khronos CTS HQ offline reference accepts the captured domain | Original exact comparison still fails (max 3 versus tolerance 2). CTS interval/reference is not an NVN precision specification. |
| Formal host plumbing | Default-OFF depth, depth-sampled and source-catalogue gates with actual ABI1/ABI2 API29 builds and original loader/HLE source closure | Compilation/linking is separate from Android GPU execution. Formal SSBO switch integration is pending. |

## Latest real external workload: v21

The private validation run rebuilt from original raw ExeFS through the normal recorded pipeline, loaded the original ARM64 main/SDK/submodule using the existing host/HLE and observed the original module entry/nnMain path. It verified 46 external RomFS files (8,973,547,258 bytes). There were **35 successful TextureInitialize calls and seven successful ProgramSetShaders calls**, comprising four single-compute and three two-stage graphics programs. This crosses the earlier first-graphics-source identity boundary.

The run stopped explicitly at `TextureBuilderGetStorageSize`, unsupported errno 95, for a **non-packaged** target-4/format-50 resource, size 2100×2100, depth/layers 2, one level, flags `0x108`. Those numeric values identify the observed boundary; they are not sufficient evidence to assign or invent every missing semantic. The narrow packaged-layout interoperability experiment was explicit, not a general layout guarantee.

**There is still no original-game Draw, QueueSubmit, Present or first frame.** v21 is not a passing game run. The application was stopped and its absence verified. Historical v20 stopped earlier at a default-disabled packaged-layout gate, so comparing only the last API name would hide the different state/options.

The underlying original-game inputs, shader/control/C1 bytes and raw logs are private local evidence. Their public replacement is a selected fact summary with evidence hashes, not a log scrub that claims to publish a complete reproduction bundle.
