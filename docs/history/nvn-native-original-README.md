> 历史库README / Historical library README, preserved at migration. 早期入口数、游戏边界及命令路径已过时；当前状态见 [STATUS](../STATUS.md)，接续见 [NEXT](../NEXT.md)。本地路径与设备标识已替换。

Independent NVN native hardware bring-up
======================================

This directory starts the independent NVN-compatible userspace GPU driver.
The implementation qualifies Android ARM64 / Adreno 735 and 650 / KGSL using a
standalone NativeActivity APK. D2R supplies test inputs and original-host integration;
the library has no dependency on D2R rendering types or runtime. Public
reconstructed NVN ABI types are kept in its own third-party directory.

Latest A650 qualification: standard NVN Window source presentation now passes
2/3 buffers × requested intervals0..5, with two window generations per case:
240 actual presentations, 24 screenshots and 4,478,976,000 exact active-channel
checks. Lifecycle, canonical storage, guards and final process stop pass.
Two-buffer interval0 uses effective FIFO1 in this backend. The existing Perfetto
FrameTimeline capture has dropped/parser-error records; it does not establish
complete cadence conformance. This authored display workload is not a game frame.

The ordinary fixed64x64 sampled NVN consumer passes three fresh process runs
after separating its completion word from executable command pages. A distinct
raw-native dynamic pipeline passes72 submissions across three sizes, three vertex
layouts and four draw modes, with same-handle replay. These qualifications have
separate scopes; they do not qualify the new source-resolved public NVN Draw path.
The latter now has typed runtime specialization, API UBO projection, vertex state,
per-submit resource ownership and real KGSL strict-link checks. Its public NVN
LINEAR64 qualification now passes24 actual submissions and393216 exact color
channels across three vertex layouts, four draw modes and same-handle replay.
Its separate public NVN flags9 canonical-GOB qualification also passes72 actual
submissions across64x64,127x73 and2880x1620, three vertex layouts and four draw
modes with replay. The independent offline inverse-sector oracle verifies449180832
channels and13437792 canonical padding/outer-guard bytes. GPU conversion and
retile preserve canonical storage; this does not qualify arbitrary sampled
resources, NVIDIA compression or a real D2R frame.

The latest genuine D2R run executes the original rewritten ARM64 modules through
the original host/HLE, reaches nnMain, initializes three original2880x1620 source
textures and its Window with the game's actual PresentInterval2. Its observed
first error is TextureGetViewOffset ENOTSUP after successful registration of
the actual64x64 RGBA_DXT1 resource and its default view. CPU sampler metadata and
canonical texture registration are independent of native consumption capability.
Four original texture initializations succeed, including that BC1 resource.
The earlier typed vertex state also crossed VertexStreamStateSetStride. No actual game
Draw, Dispatch, Present or game frame has been established. Declarations, runtime
entries, HOST checks, GPU qualifiers and real-game execution remain separate.
Independent public NVN declarations and original SDK static evidence are indexed
in docs/research/nvn-turnip-study-20261023/public-nvn-definitions/.

The inactive-border sampler fix now passes38 actual GPU submissions and622592
channel checks, with76 public Builder/Sampler border-color snapshot checks.
Repeat, mirrored-repeat and clamp-to-edge retain arbitrary border-color bits
without using a border table. Border-consuming modes remain unsupported. The
original SDK static setter/getter evidence, HOST pack checks and both earlier
qualifier setup failures are preserved separately from the successful GPU run.
Seven independently typed sampler border aliases/BuilderGetDevice entries now
bring the registered entry count to344; they add CPU metadata/getter behavior,
not new GPU border modes. Whole-family canonical texture registration passes
37 existing layout formats, complete mip/layer views and snapshot lifetimes;
unsupported GPU formats and actual subresource views retain their consumer gates.

Initial A735 hardware result: an ordinary application UID opened KGSL, queried chip
ID `0x43030b00`, allocated/mapped one 4096-byte uncached BO, created a context,
submitted Packet-7 `CP_MEM_WRITE`, waited for the returned context timestamp,
and read back `0x4e564e31`. Cleanup completed without errors. The BO contains
both the command and destination; the destination is initially zero. The APK
is stopped after each qualification run.

That default test exercises the GPU command processor. The optional compiled-CS
mode now also executes the shader core: both controlled variants wrote their
respective expected values through SSBO 0. Neither mode executes an NVN API
entry point or real game module. A separate NVN API mode is described below.
The backend now provides reusable allocation/import, contexts, asynchronous
submission and explicit completion. Unknown submission results retain resource
ownership and prevent further submission; unresolved completion blocks
destruction. Qualification failures leave such objects intact until the test
process exits, rather than recycle potentially live memory.

Build and run
-------------

From the repository root, choose dedicated output and explicit SDK/NDK paths:

```sh
python3 nvn-native/tools/build_android_probe.py \
  --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/probe
python3 nvn-native/tools/run_android_probe.py \
  --serial DEVICE_SERIAL --build .local/nvn-native/probe \
  --evidence .local/nvn-native/evidence/run.json --install
```

Build requirements: CMake, Ninja, JDK/keytool, NDK r28c, Android SDK platform 36
and build-tools 36.0.0. The native library targets API 29. Local debug signing
material and all outputs stay in the selected output directory. OEM installation
confirmation may appear on the phone; the runner does not change security settings.

`build-record.json` records source/tool/artifact hashes and executed commands.
The runner verifies artifact integrity and compares the installed APK hash,
removes stale results, runs one bounded test, saves the result and build record,
and stops only the probe package. Its polling only waits for the result file;
it is not a GPU timer or performance sampler.

The exact chip ID is gated. Supporting other revisions or cached/coherent BOs
requires separate qualification. The initial context flag combination rejected
by this kernel is preserved in local evidence; the working combination follows
the existing Freedreno KGSL replay tool.

Shader input contract
---------------------

`tools/shader_fixture.py FILE.json` verifies a versioned fixture envelope:

- Top-level fields: `schema=1`, `stage`, nonempty `profile`, `compiler`,
  `provenance`, `blobs`, and `state`.
- `blobs` explicitly contains `code`, `control`, `c1`; each specifies `path`,
  `offset`, `size`, `sha256`. C1 may explicitly have zero bytes. Inputs are
  bounded and hashes cover exactly the selected ranges.
- `state` explicitly contains `resources`, `graphics`, `compute`, `host`,
  `precision`. Compute specifies `local_size`, `shared_bytes`, `local_bytes`;
  graphics and compute state are mutually exclusive.

Its request digest includes input bytes, profile/compiler versions and explicit
state; file paths and provenance labels do not specialize code. This validates
the input envelope, not the full meaning of resource/host/precision dictionaries.
The strict compiler accessor and a bounded Maxwell-to-NIR buffer bridge are
now implemented below. No PC-derived facts are implicitly loaded. The input
contract still does not describe all NVN control versions or shader stages.

```sh
python3 -m unittest discover -s nvn-native/tests -v
```

The tests use synthetic temporary data and exercise input mutation, bounds,
missing compute metadata, and specialization-key invalidation. No game bytes
or generated game shaders belong in this directory.

The design, preserved ZIP, and task T01–T08 acceptance criteria are in
`docs/research/nvn-turnip-study-20261023/implementation.zh-CN.md`.
Sections below retain the earlier bring-up results. Current NVN graphics calls
require the explicit schema3 mode artifacts described at the end of this file;
legacy raw graphics/AHB qualifiers retain their earlier artifact contracts.

Standalone IR3 compiler
-----------------------

`shader/ir3_compile.c` builds a controlled NIR compute shader and compiles it
for FD735 using Mesa IR3. It writes a caller-selected uint32 to SSBO 0 at offset
zero. Outputs include pre-optimization NIR, actual machine code, captured IR3
disassembly and hardware program metadata. This is compiler bring-up input;
This controlled input remains separate from the original Maxwell path below.

```sh
python3 nvn-native/tools/build_ir3.py \
  --mesa-archive ${D2R_REFERENCE_ROOT}/.local/tools/adreno-native-research/mesa-26.2.5.tar.xz \
  --meson ${D2R_REFERENCE_ROOT}/.local/tools/adreno-native-research/venv/bin/meson \
  --host-tools ${D2R_REFERENCE_ROOT}/.local/tools/adreno-native-research/host-tools \
  --out .local/nvn-native/ir3-pinned
python3 nvn-native/tools/check_ir3.py \
  --build .local/nvn-native/ir3-pinned --out .local/nvn-native/ir3-pinned/check
```

The builder verifies the Mesa archive SHA256, extracts into a dedicated output
directory and checks source bytes against the archive on subsequent builds.
Its overlay adds the compiler executable and enables full NIR for standalone
tools; upstream otherwise builds `nir_stub.c` with all drivers disabled. Both
Gallium and Vulkan drivers remain disabled. The mutable source copy, generated
headers and all compiler outputs stay outside tracked source directories.
The builder records the source manifest, overlay/tool hashes, options, commands
and compiler artifact hash. Host dependencies include a C/C++ compiler, Ninja,
Meson and its Python generator modules, Bison/Flex/m4, zlib/zstd and libdrm headers.
The executable's actual dynamic dependencies are checked separately.

The check uses Mesa's existing `IR3_SHADER_DEBUG=asmroundtrip` facility with two
different stored values, verifies machine code and metadata outputs, and checks
unsigned input rejection. These checks verify compilation and encoding, not
the GPU's execution of the shader.

Original Maxwell CS compilation
-------------------------------

`shader/maxwell/` references only the vendored shader translator, SPIR-V emitter
and common host utility project. It does not instantiate a renderer or execute
CPU guest instructions. Its accessor overrides every `IGpuAccessor` method;
reflection rejects inherited default interface implementations after an upstream
change. Queries require explicit fixture state, code/C1 reads have hard bounds,
and unsupported resources or translator diagnostics reject compilation.
The accessor also accepts bounded original VS/FS containers, checks their SPH
stage and requires every graphics specialization field explicitly. No implicit
graphics state or PC reflection is imported.

`shader/spirv_ir3.c` imports SPIR-V into NIR, lowers scalar set-zero buffer
bindings to separate bindful UBO/SSBO tables, and compiles with IR3 for FD735.
Binding numbers are table indices (holes remain), not inferred game addresses.
The upstream names `TargetApi.Vulkan` and `NIR_SPIRV_VULKAN` select intermediate
SPIR-V descriptor conventions only; no graphics API runtime/driver is involved.
The standalone build continues to disable all graphics API drivers.

The D2R adapter extracts four original embedded CS from one pinned, unmodified
94012 main NSO. It verifies the full input hash, decoded segment hashes and known
control identity; code and C1 ranges plus compute dimensions come from this
specific container profile. Local memory zero and host translation choices are
explicit profile facts, not a general NVN control decoder or device capability
claim. A new query is rejected until that profile explicitly supplies it.

```sh
python3 nvn-native/tools/build_maxwell.py \
  --dotnet ${D2R_REFERENCE_ROOT}/.local/dotnet/dotnet \
  --out .local/nvn-native/maxwell
${D2R_REFERENCE_ROOT}/.local/venv/bin/python \
  nvn-native/adapters/d2r/extract_embedded_cs.py \
  --nso ${D2R_REFERENCE_ROOT}/work/data/switch/3.3.94012/Update/ExeFS/main \
  --out .local/nvn-native/real-shaders
python3 nvn-native/tools/check_maxwell.py \
  --dotnet ${D2R_REFERENCE_ROOT}/.local/dotnet/dotnet \
  --frontend-build .local/nvn-native/maxwell \
  --ir3-build .local/nvn-native/ir3-pinned \
  --spirv-val ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358/shader-tools/linux-x86_64/spirv-val \
  --out .local/nvn-native/maxwell/check \
  .local/nvn-native/real-shaders/*.fixture.json
```

Build IR3 first with the preceding command. The frontend needs .NET 10; extraction
reuses the existing NSO tool and requires its Python `lz4` dependency. The check
uses the existing `spirv-val`, `spirv-dis`, `spirv-as` tools from one SDK directory
and IR3's `asmroundtrip`; no replacement validator/disassembler is implemented.
Build records cover the four-project dependency closure, sources, restored package
manifests, runtime files, tool hashes and compiler output identities.

All four original CS passed: two produce 256-byte IR3 programs and two 384-byte
programs, each with local size 32×1×1. Outputs include original imported NIR,
lowered NIR, SPIR-V, actual target binary, IR3 disassembly and program metadata.
Metadata preserves system input registers, constant allocations, driver parameters
and UBO push ranges. Missing queries, mismatched hashes, bounded/truncated code,
unqualified precision and nonzero descriptor sets were rejected.
Evidence is in `docs/research/nvn-turnip-study-20261023/maxwell-ir3-check.json`.

These checks alone qualify host compilation and encoding. The native program
path below now executes these four shaders; full Maxwell semantics and original
NVN behavior still need much broader evidence. The earlier controlled hardware
fixture remains separate. Game bytes, decoded modules and generated shader code stay
in ignored `.local/` outputs; only hashes and metadata are checked in.

Compiled CS hardware qualification
----------------------------------

`shader/cs_artifact.cc` uses the generated register definitions, device data and
FDL buffer descriptors to encode a bounded A7xx compute fixture. The state
sequence follows the existing Computerator implementation, including program
setup, UAV load, dispatch and cache completion. It emits an artifact containing
the shader, commands, descriptor and BO-relative address relocations. Android
copies it into a 16 KiB BO and patches addresses with the actual KGSL GPUVA.
The CPU initializes the output to zero; the command processor only writes a
separate completion location. The SSBO result is written by shader instructions.

```sh
python3 nvn-native/tools/build_android_probe.py \
  --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/cs-probe \
  --keystore .local/nvn-native/probe/probe-debug.keystore \
  --cs-check .local/nvn-native/ir3-pinned/check/check.json --cs-case 0
python3 nvn-native/tools/run_android_probe.py \
  --serial DEVICE_SERIAL --build .local/nvn-native/cs-probe \
  --evidence .local/nvn-native/evidence/cs.json --install
```

Use case 1 for the second stored value. Reuse the original local debug signing
identity for package updates. The builder checks the generated header hash and
embeds the compiler check/build record; the runner also checks that the result
matches the selected case. Both cases passed on the ordinary UID of the target
FD735: `0x4e564e32` and `0x12345678` respectively. Records are in
`docs/research/nvn-turnip-study-20261023/kgsl-cs-check.json`.

The emitter currently rejects shader requirements outside this fixture: extra
resources, samplers, constants, private/shared memory or larger workgroups.
It is a hardware execution qualification, not a general NVN shader runtime or
GPU replay tool. Further NVN work must provide program/resource state and input
semantics through reusable backend interfaces rather than hardcode this fixture.

Reusable KGSL and first NVN API path
------------------------------------

`include/nvn_native/kgsl.h` and `src/kgsl.c` provide separate device, BO and queue
objects. BOs may be uncached allocations or imports of caller-owned pages;
imported CPU pointers retain their identity. Cache range operations cover the
cached imports. Submission retains every referenced BO until explicit queue
completion, even if the higher-level command handle has already been released.
An uncertain submit retains its resources and poisons further queue submission.
BO/context/device destruction returns EBUSY rather than free unresolved work.
Fault recovery and finer-grained timestamp retirement remain future work.

The old one-shot test now uses this same backend. Its compiled CS regression
still writes `0x4e564e32` correctly after the refactor.

`src/nvn.c` exposes `nvnBootstrapLoader` and `nvnDeviceGetProcAddress`. The initial
entry table covers builders, device/queue, memory pools, buffer views, command
and control memory, ClearBuffer recording, queue submission/finish and command
handle finalization. Every entry is compile-checked against the public
reconstructed ABI type. Internal object state lives outside caller opaque
objects, with references retained independently of caller command handles.

```sh
python3 nvn-native/tools/build_android_probe.py \
  --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/nvn-api --nvn-api \
  --keystore .local/nvn-native/probe/probe-debug.keystore
python3 nvn-native/tools/run_android_probe.py \
  --serial DEVICE_SERIAL --build .local/nvn-native/nvn-api \
  --evidence .local/nvn-native/evidence/nvn-api.json --install
```

The real-device API test resolves the function pointers, imports two caller-owned
pools, creates overlapping Buffer views, records two command handles and submits
both without waiting between them. It releases recording objects/handles, verifies
in-flight data and command BOs cannot be freed, waits, invalidates the CPU range,
and checks all data bytes including untouched guards. It also checks opaque
object guards, command/control usage and rejection of insufficient control memory.
This path passed on ordinary UID 10357. The first import attempt returned EFAULT;
removing the AArch64 top-byte tag from the nested kernel address fixed it, while
the CPU-visible pointer remains the caller's original value.

Current qualification is deliberately explicit: calls must be serialized by the
owner; the initial pool profile is page-aligned CPU_CACHED | GPU_CACHED storage;
recording uses one command arena and one control arena; ClearBuffer currently
accepts at most 1024 aligned words per call. There is no memory callback, arena
chain, physical-alias canonicalization across pools, concurrent entry-point
qualification, deferred public-object destruction, Draw, Sync,
Event or D2R integration yet. Builder metadata reclamation also needs refinement.
Unsupported names return NULL and unsupported configurations report a sticky
qualification diagnostic through `nn_nvn_error`, rather than claim success.

This is a working NVN-shaped resource/command subset against self-authored tests,
not original-NVN conformance evidence. The ABI header supplies types only.
Existing-model black-box comparison and real D2R module execution still belong
to the full objective. Evidence is recorded in
`docs/research/nvn-turnip-study-20261023/nvn-resource-check.json`.

Native NVN Program / Dispatch
------------------------------

The resolver now also provides ProgramInitialize/SetShaders/Finalize,
CommandBufferBindProgram/BindUniformBuffer(s)/BindStorageBuffer(s)/DispatchCompute. SetShaders matches
the complete original code/control against explicitly registered offline compiler
cache entries. A miss returns ENOTSUP; this is not runtime shader compilation.
The registration API is internal glue with immutable storage lifetime documented
in `include/nvn_native/nvn.h`. Guest code is never treated as Adreno machine code.

`shader/compute_artifact.cc` emits program-state templates and typed patch records
from real IR3 metadata. `src/compute.c` fills actual code/resource GPU addresses,
workgroup counts and global dimensions. FDL produces the fixed R32 descriptor;
dynamic address/size field patching uses generated field definitions and is checked
against full FDL output for 20 address/size combinations per shader. Each dispatch
has an immutable 8 KiB command/descriptor/sequence BO, separate from the code BO.
Command handles retain all resources. Released dispatch BOs retire after queue
completion, including handles released while in flight. Repeated handles reset
their completion location on the GPU before each execution.

IR3 driver parameters are uploaded to their allocated constants. UBO push ranges
use indirect GPU constant loads, so updates after recording and before submission
are consumed at execution. NVN Uniform slot 0 maps to physical Maxwell c3, through
c16 for NVN slot 13. Compute globals reflected into c0[0x310 + 16*r] map directly
to NVN Storage slot r; callers do not bind a raw c0 bank or construct its driver
table. Bulk bindings validate the whole range before committing any change;
address/size zero unbinds a slot. Other raw driver banks and indirect global
descriptor schemes remain unsupported.
Shader-code-marked cached caller pools are now accepted as original input storage.

```sh
python3 nvn-native/tools/build_android_probe.py \
  --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/maxwell-probe \
  --keystore .local/nvn-native/probe/probe-debug.keystore \
  --maxwell-check .local/nvn-native/maxwell/check/check.json
python3 nvn-native/tools/run_android_probe.py \
  --serial DEVICE_SERIAL --build .local/nvn-native/maxwell-probe \
  --evidence .local/nvn-native/evidence/maxwell-compute.json --install
```

The D2R-only adapter bundles verified original inputs, compiled artifacts and
resource reflection into the qualification caller; identities and kernel-specific
oracles never enter the driver. Four original shaders execute through NVN pointers,
with counts 1/33/65, multiple workgroups, buffer addresses four bytes past a
64-byte boundary, whole-allocation guards and u64 arithmetic edge inputs.
All 12 cases passed on ordinary UID 10357. Four further submissions reuse handles
after recording-object destruction, change constants and source contents, clear
destinations and revalidate the entire allocation. Program destruction is rejected
while referenced/in flight; all BOs are reclaimed after completion. The earlier
ClearBuffer/alias/lifetime qualification was rebuilt and passed as a regression.

The initial compute caller in commit c3ea24c bound raw Maxwell c0/c3 as if they
were NVN Uniform slots. That run proved GPU execution/lifetime, but did not prove
NVN buffer-slot semantics. It is superseded by the actual Uniform 0 / Storage 0,1
caller and evidence in `docs/research/nvn-turnip-study-20261023/nvn-buffer-slots-check.json`.
All 12 cases and four handle reuses passed again. Three negative checks cover an
out-of-range uniform slot, a partially invalid bulk binding (old bindings stay
intact), and dispatch after storage unbind (no command bytes emitted).

The first GPU run exposed a real frontend IADD carry bug in the difference kernels.
`shader/maxwell/iadd_overlay.py` applies a pinned-source fix only to this isolated
compiler build via MSBuild source replacement; shared vendored sources stay intact.
It follows the pinned NAK Maxwell IAdd2/IAdd2X encoding and hardware-test contract:
ordinary subtraction contributes the initial +1, extended negated sources use
one's complement with carry-in. The profile rejects IADD.PO. A compiler marker and
source hashes make the overlay mandatory and visible. The original failing run
and final success are preserved in
`docs/research/nvn-turnip-study-20261023/nvn-compute-check.json`.

This is real original shader execution through a bounded NVN-compatible API path,
with a self-authored caller. It does not execute a real ARM64 game module or prove
original-NVN conformance. Qualified images have nonzero R32 SSBO tables, fully
pushed UBO ranges, no samplers/private/shared/extra constant data and 64-thread
hardware wave mode. Runtime compilation misses, broader GPU resource semantics,
NVN graphics commands, textures, synchronization, window presentation and actual
D2R execution remain work. Native backend Draw qualification follows below.

Original graphics pair and VPC preparation
-------------------------------------------

`adapters/d2r/extract_embedded_graphics.py` reuses the verified NSO extraction and
selects original controls `8ac93c54cfb3cbe3` / `0d721a0acc36c0bf`: an untextured
embedded VS/FS pair. Original NVN prefixes and SPH remain present in the inputs;
the frontend starts decoding at the SPH after the 0x30-byte container prefix.
The graphics profile explicitly configures float attributes/outputs, triangle
topology, depth/viewport/alpha state and native format swapping. These are compiler
profile choices; BGRA rendering is not hardware-qualified by this host check.

```sh
${D2R_REFERENCE_ROOT}/.local/venv/bin/python \
  nvn-native/adapters/d2r/extract_embedded_graphics.py \
  --nso ${D2R_REFERENCE_ROOT}/work/data/switch/3.3.94012/Update/ExeFS/main \
  --out .local/nvn-native/graphics-shaders
python3 nvn-native/tools/check_graphics.py \
  --dotnet ${D2R_REFERENCE_ROOT}/.local/dotnet/dotnet \
  --frontend-build .local/nvn-native/maxwell \
  --ir3-build .local/nvn-native/ir3-pinned \
  --spirv-val ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358/shader-tools/linux-x86_64/spirv-val \
  --vs .local/nvn-native/graphics-shaders/8ac93c54cfb3cbe3.fixture.json \
  --fs .local/nvn-native/graphics-shaders/0d721a0acc36c0bf.fixture.json \
  --out .local/nvn-native/graphics-check
```

Rebuild both host tools first. `shader/nir_bridge.h` shares the actual VTN and
buffer lowering path with CS. All four CS binaries and native templates remain
byte-identical after that refactor. `nvn-graphics-ir3` splits per-member SPIR-V
interface structs, lowers stage IO and emits actual 384-byte VS / 128-byte FS
programs plus disassembly, constant and IO metadata. It checks the FS varying's
producer/type width before invoking IR3 linkage; position and point size are then
appended to the VPC map using the same public helpers as the hardware driver.
Missing specialization state, SPH stage mismatch and a valid SPIR-V FS requesting
an absent varying are rejected. SPIR-V validation and IR3 asmroundtrip pass.

The native device exposes GMEM bytes from the existing KGSL device-info ioctl.
The ordinary-UID test reported 1,572,864 bytes. Optional
`--gmem-evidence .local/nvn-native/evidence/graphics-device.json` feeds that measured
profile to the host tool, which reuses `fd6_calc_gmem_cache_offsets`: three CCUs,
sysmem depth offset 0 and color offset 786,432. Insufficient GMEM is rejected
before the layout helper can underflow. Evidence is in
`docs/research/nvn-turnip-study-20261023/graphics-compiler-check.json`.

The original host-only result prepares shader programs, VPC exports, constants
and cache layout. Its subsequent native GPU qualification is described next.

Original VS/FS native offscreen draw
-----------------------------------

With `--gmem-evidence`, the graphics compiler also emits `.graphics.h/.json`:
a strict A735 64x64 RGBA8 linear rendering profile, FDL target layout, actual
VS/FS code, 698 PM4 words and eight typed GPUVA patches. State comes from the
public hardware helpers and the compiler's real IO/linkage metadata. The
independent `src/graphics.c` uploads a caller's bound BO addresses into a fresh
immutable job; matrix constants use GPU indirect loads. The generated profile
rejects unknown layouts, resources and shader properties.

```sh
python3 nvn-native/tools/check_graphics.py \
  --dotnet ${D2R_REFERENCE_ROOT}/.local/dotnet/dotnet \
  --frontend-build .local/nvn-native/maxwell-texture \
  --ir3-build .local/nvn-native/ir3-pinned \
  --spirv-val ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358/shader-tools/linux-x86_64/spirv-val \
  --vs .local/nvn-native/graphics-shaders/8ac93c54cfb3cbe3.fixture.json \
  --fs .local/nvn-native/graphics-shaders/0d721a0acc36c0bf.fixture.json \
  --gmem-evidence .local/nvn-native/evidence/graphics-device.json \
  --out .local/nvn-native/graphics-draw-check
python3 nvn-native/tools/build_android_probe.py \
  --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --keystore .local/nvn-native/probe/probe-debug.keystore \
  --graphics-check .local/nvn-native/graphics-draw-check/check.json \
  --out .local/nvn-native/graphics-draw
python3 nvn-native/tools/run_android_probe.py \
  --serial DEVICE_SERIAL --build .local/nvn-native/graphics-draw \
  --evidence .local/nvn-native/evidence/graphics-draw.json --install
```

The ordinary-UID run passed four submissions using the same IB: solid red,
matrix translation giving a green right half, translation fully outside the
viewport, and RGB barycentric interpolation. All 65,536 channel checks and
98,304 guard-byte checks passed; integer-color cases are exact and the gradient
allows one UNORM quantization unit. The runner saves the actual RGBA readbacks,
compares installed APK identity, removes stale files and stops the process.
See `docs/research/nvn-turnip-study-20261023/kgsl-graphics-check.json` and previews.

This is original GPU shader execution with a self-authored native caller, not
NVN graphics API coverage or a real ARM64 game frame. Texture/depth tests,
graphics preemption robustness, general state, window/presentation and game
execution remain unqualified. T05 is therefore only partially complete.

Original sampled pair, offline only
-----------------------------------

`extract_embedded_textures.py` selects the verified original VS `96757bb36a024b91`
and FS `a7672f1d3bfeb7d0`. Supply a test specialization JSON explicitly, for example
at `.local/nvn-native/texture-profile.json`:

```json
{"textures":[{"handle":274104430,"cbuf_slot":196610,"sampler_type":"Texture2D","coord_normalized":true}],"host":{"QueryHostGatherBiasPrecision":0}}
```

These values configure the compiler test. They are not a captured game sampler
state, native image format, pool index or hardware gather-precision claim.
The gather query is requested by the frontend; this FS has no gather operation.
Texture and sampler handle-source words are decoded separately in reflection.

```sh
${D2R_REFERENCE_ROOT}/.local/venv/bin/python \
  nvn-native/adapters/d2r/extract_embedded_textures.py \
  --nso ${D2R_REFERENCE_ROOT}/work/data/switch/3.3.94012/Update/ExeFS/main \
  --texture-profile .local/nvn-native/texture-profile.json \
  --out .local/nvn-native/texture-inputs
python3 nvn-native/tools/check_textures.py \
  --dotnet ${D2R_REFERENCE_ROOT}/.local/dotnet/dotnet \
  --frontend-build .local/nvn-native/maxwell-texture \
  --ir3-build .local/nvn-native/ir3-pinned \
  --spirv-val ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358/shader-tools/linux-x86_64/spirv-val \
  --extraction .local/nvn-native/texture-inputs/extraction.json \
  --out .local/nvn-native/texture-check
```

Both original stages compile, pass SPIR-V validation and IR3 asmroundtrip, and
link UV.xy/color.xyzw outputs. The tool rejects missing queries, nonzero sets,
out-of-range texture bindings and descriptor arrays. It also verifies that
binding15 needs 16 hardware slots, preserving holes. The NIR bridge uses existing
sampler/cast lowering and explicitly rebuilds sparse resource-use bitsets.
`build_ir3.py` applies the SHA-locked half-predicate lexer correction while
keeping asmroundtrip enabled. This profile does not emit a native texture Draw
artifact; actual hardware sampling is still pending.

Original-host bootstrap bridge, compile only
--------------------------------------------

`adapters/d2r/host_bridge.c` registers the original `HLE_FORCE` bootstrap hook
and keeps subsequent device lookups on the independent resolver. Known missing
calls name themselves and terminate with exit78; they never report success.
Unknown/null lookups terminate immediately. The generated header provides typed
failure boundaries for the 477 names with public prototypes; 46 undeclared names
remain explicit unknown-ABI termination boundaries. The driver resolver itself
continues to return NULL for unsupported names.

```sh
python3 nvn-native/tools/build_d2r_bridge.py \
  --nso ${D2R_REFERENCE_ROOT}/work/data/switch/3.3.94012/Update/ExeFS/main \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/d2r-bridge
python3 nvn-native/tools/build_d2r_bridge.py \
  --nso ${D2R_REFERENCE_ROOT}/work/data/switch/3.3.94012/Update/ExeFS/main \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/d2r-bridge --verify
```

The tool reuses original NSO/name extraction, verifies input/segment hashes,
names and typed declarations, builds only the ARM64 bridge OBJECT and driver
archive, and records source/tool/options/artifact identities. Verification
checks cached bytes without repairing them. ELF inspection confirms AArch64
and the original 24-byte retained `swforce` section.

A future original host can link CMake's optional `nvn_d2r_host_bridge` OBJECT
target directly while selecting the new backend; its old `graphics_boundary.c`
and `hle_nvn.c` must then be excluded. This tool does not make that selection,
build/deploy a game or change its binary. Actual game execution remains pending;
the first observed unsupported startup call is `DeviceBuilderSetFlags`.

Replacement A650 device and NVN graphics caller
---------------------------------------------

The current test device is `DEVICE_SERIAL`, Xiaomi `23043RP34C` / pipa, Android 14
with kernel 4.19.157. Its ordinary-UID KGSL query reports exact chip `0x06050002`
and GMEM 1179648 bytes. Wireless ADB ports may change; discover the current port
using `adb mdns services`. The earlier A735 phone is no longer used.

`--device-inspect` builds a read-only property query with no GPU submission.
A650 driver-owned BOs use WC (Normal-NC) mappings; old-kernel UNCACHED mappings
faulted on a compiler-generated unaligned CPU store. Anonymous cached pools use
`GPUOBJ_IMPORT` with ADDR and `priv_len=buffer_bytes`, with strict returned
cache-mode/ID/address/size checks. The legacy import interface drops cache-mode
flags in the public pipa kernel and led to a failed NVN resource submission.
Explicit kernel cache maintenance remains required for imported pools.

A6 shader/compiler artifacts use the measured exact chip and GMEM, A6 CCU
layout, generation-specific registers and timestamp-bearing cache events. Their
4-byte sequence storage is internal to the same immutable graphics job BO and
shares its submission lifetime. A735 command/code regressions are checked on
the host; they do not establish A650 hardware results.

The initial untextured graphics contract is deliberately narrow:
the registered original untextured pair, RG32F position/RGBA32F color in a
24-byte vertex stream, VS Uniform0, one 64x64 linear RGBA8 target, full viewport
and scissor, depth range 0..1, and one 3-vertex triangle. Unsupported states are
rejected. Public reconstructed ABI declarations do not establish full NVN
semantic conformance.

`--nvn-graphics-check PATH` packages verified original code/control identities
and drives Program/Pool/Buffer/Texture/CommandBuffer/DrawArrays/Queue through
the public NVN resolver. A single command handle is reused after host matrix,
vertex and target changes. Four cases check 65536 channels and 167712 guard
bytes, with exact solid colors and one-code-unit gradient tolerance. This is
an ARM64 qualification caller executing translated original GPU shaders; no
original ARM64 game module has executed through this backend.

Full replacement-device findings and evidence are in
`docs/research/nvn-turnip-study-20261023/a650-device.zh-CN.md`.

Sampled NVN graphics and descriptor consumption
-----------------------------------------------

The resolver now has 154 entries, including checked sampler builders, samplers,
texture/sampler pools and three independent handle-binding domains. API pool
slots remain 32 bytes with 256 reserved entries. Registration writes checked
software descriptors and snapshots texture layout/backing or sampler values;
Adreno's 64-byte texture descriptors are generated separately. Arbitrary guest
TIC/TSC bytes, views, additional layouts and filters are not qualified.

The original sampled pair uses separate fragment texture slot 10 and sampler
slot 0. Its CB2 source words 110/346 are recorded explicitly in compiler-cache
metadata. Neither word is a pool ID or a native binding: the test uses pool
IDs 257/259 and native indices 1/1. Combined handles occupy a different API
domain. The current implementation supports 32 selectors per stage as an
explicit subset, not a statement about every original NVN binding limit.

Draw records pool identities, selectors and fixed state. Each enqueue resolves
only referenced descriptor entries and creates private parent/child command
BOs. Submitted versions retain their backing BOs until completion; later
registration cannot overwrite an earlier submission's hardware descriptors.
The caller command arena stays unchanged. CPU writes to texture/UBO data still
require publication and ordering against GPU use. This does not implement
unsynchronized live-table changes during GPU execution.

Build with `--texture-check PATH` for the direct KGSL sampled caller, or
`--nvn-texture-check PATH` for the NVN pool/binding caller, using a verified
`check_textures.py` output. Both execute the original shaders and compare four
images to a manually derived Maxwell closed form. The NVN test additionally
checks wrong domains/slots, copied binding arrays, post-record registration,
same-handle backing replacement, object lifetime and same IDs in different
pools. A fifth independently counted draw selects the second pool. Full
qualification and remaining work are recorded in
`docs/research/nvn-turnip-study-20261023/texture-progress.zh-CN.md`.

`--ahb-inspect` only allocates/maps an Android HardwareBuffer and inspects its
borrowed fd metadata. It uses a published platform extension absent from the
public NDK header; successful device qualification is not a portable API promise.
It submits no GPU commands and presents no frame. The replacement device reports
64x64 RGBA8, stride 64 pixels, two DMA-BUFs and correct cross-lock CPU sentinels.
Matching the shared CPU VMA's device/inode to owned fds identifies the pixel
backing and offset without reading vendor integers. The shared `android/ahb.c`
owner balances CPU locks and retains allocations after uncertain unlocks;
the caller separately tracks GPU and display ownership.

`--ahb-graphics-check PATH` consumes the same verified original untextured
`check_graphics.py` record. On the replacement A650 it imports the identified
pixel DMA-BUF directly into KGSL, first checks CP writes against a complete
16 KiB independent shadow, then checks four original-shader draws and 65,536
color channels through AHB read locks. The imported BO has no CPU map and
rejects `nn_bo_cache`; the allocator's lock/unlock protocol owns CPU cache
operations. The CPU initializes backgrounds but never copies rendered or
expected images into this target. Busy targets cannot be destroyed.

```sh
python3 nvn-native/tools/build_android_probe.py \
  --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/a650-ahb-direct \
  --keystore .local/nvn-native/probe/probe-debug.keystore \
  --ahb-graphics-check .local/nvn-native/a650-final-graphics-check/check.json
python3 nvn-native/tools/run_android_probe.py \
  --serial DEVICE_SERIAL --build .local/nvn-native/a650-ahb-direct \
  --evidence .local/nvn-native/evidence/a650-ahb-direct.json --install
```

All four device readbacks match the prior NVN offscreen outputs byte for byte.
This qualifies only this chip and 64x64 RGBA8/usage819/linear-pitch256 profile;
it does not establish arbitrary gralloc/UBWC layouts. There is no CPU check of
storage outside the AHB payload. This offscreen mode submits no frame to a
compositor. The subsequent presentation mode below qualifies SurfaceControl
and window lifecycle on the replacement A650. Neither qualification caller
executes an original ARM64 game module.

The D2R adapter now forwards declared APIs through their actual PFN types and
checks sticky diagnostics before and immediately after each call. Errors exit
78 with API/errno/phase; ordinary false, zero, float and aggregate results remain
unchanged. Its special resolver keeps both NULL-device bootstrap and subsequent
lookups in the adapter. Undeclared ABI entries remain fatal boundaries. This
does not implement missing DeviceFlags/version/memory/window semantics.
Original-host integration and the observed boundary are described below.

`tests/d2r_bridge/` preserves the exact host regression sources whose hashes and
compiler commands are in
`docs/research/nvn-turnip-study-20261023/d2r-typed-bridge-check.json`. For a clean
checkout, replace the commands' ignored test-source paths with these tracked
copies; obtain `names.inc.h` with `build_d2r_bridge.py` as documented above.
`real_driver.c` uses the actual NVN core with a host-only memory provider that
never executes GPU submissions. `abi_carriers.c` checks synthetic public C ABI
values only. Neither test executes a game module or qualifies GPU behavior.


`--ahb-present-check PATH` builds a minimal SurfaceView-backed NativeActivity.
The ordinary-UID lifecycle runner renders an original RGB frame directly into
an AHB, presents it with SurfaceControl, captures the real screen, backgrounds
the Activity, then recreates and retires a second window. It compares interior
screen samples at the SurfaceView's actual screen position, preserves transient
animation mismatches, and requires all four raw GPU readbacks and balanced
window/buffer ownership. No CPU image transport is used. Use the local Python
with Pillow for the screenshot checker:

```sh
python3 nvn-native/tools/build_android_probe.py \
  --sdk ${D2R_REFERENCE_ROOT}/.local/android-sdk \
  --ndk ${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358 \
  --out .local/nvn-native/a650-present \
  --keystore .local/nvn-native/probe/probe-debug.keystore \
  --ahb-present-check .local/nvn-native/a650-final-graphics-check/check.json
${D2R_REFERENCE_ROOT}/.local/venv/bin/python nvn-native/tools/run_android_probe.py \
  --serial DEVICE_SERIAL --build .local/nvn-native/a650-present \
  --evidence .local/nvn-native/evidence/a650-present.json --install
```

The independent original CLI host is selected by
`adapters/d2r/host/CMakeLists.txt`. It references the original loader/HLE/native
entry sources and replaces the graphics boundary with the typed native NVN
bridge. `tools/build_d2r_host.py --exefs ORIGINAL --ndk NDK --out PRIVATE_OUT`
uses the canonical original-NSO rewrite entry with ELF/HOST_ABI1/native rules;
`--verify` checks the complete cache and provenance. It never deploys or runs
anything. A changed checkout requires a normal rebuild through this entry
before cached verification; do not patch generated modules or records.

`build_android_probe.py --d2r-host-record PRIVATE_OUT/build-record.json` verifies
and packages that unchanged PIE as `libnvn_d2r_host.so` for installation.
`tools/run_d2r_host.py --serial SERIAL --build APK_BUILD --evidence NEW.json
--stage sdk-init|constructors|run` executes the installed original host using
`run-as` under the package UID, verifies every deployed module, records native
logs, and checks only its own process during cleanup. This is a command-line
`runas_app` context, distinct from Activity execution. SDK initialization,
constructors, nnMain entry/return, a named unsupported API, and a real game frame
are separate observations. A zero host exit after guest initialization failure
is not successful game startup. All diagnostic saves/resources stay in this
probe package's private `files/d2r-host` namespace.

With original RomFS, the replacement device now completes SDK initialization
and constructors and enters original nnMain through the native loader/HLE.
After resource initialization it passes DeviceBuilderSetFlags(0x100), initializes
the independent KGSL device and queries the declared partial ABI version. It now
passes origin1/depth1 and three initial pools, then exits78 at MemoryPoolInitialize
for CPU_CACHED|GPU_UNCACHED (0x14). No real game frame has been produced. See
`docs/research/nvn-turnip-study-20261023/pool-coherent-progress.zh-CN.md`
for immutable logs, build provenance and the next bounded implementation work.

For the run stage, pass `--romfs ORIGINAL_ROMFS`. When the exact resources are
already on the same device, `--device-romfs-source PACKAGE:RELATIVE_PATH`
reads them into the probe package's private namespace and verifies each target
file against that local original directory. It does not reuse source binaries
or saves. Run the orchestration-only regressions with
`python3 nvn-native/tests/test_d2r_host_runner.py`; they execute no ADB/game.

Subsequent run stages can use `--romfs ORIGINAL_ROMFS --reuse-romfs`: the runner
checks the probe's exact private file inventory, rejects symlinks/special files,
and rehashes every file against the local original before executing anything.
It does not recopy or alter the reused resource tree. This option excludes
`--device-romfs-source` and cannot skip integrity verification.

Device flags accept exactly 0 and 0x100; unsupported debug/deferred flags retain
the previous builder state and report ENOTSUP. Initialize snapshots the flags.
`--texture-device-flags 0x100` selects an explicit-flags GPU qualification with
`--nvn-texture-check`; omitting it exercises SetDefaults. Both paths now pass
on A650. The implementation declares `nn-nvn-partial-abi53.105-v1` and returns
53/105 for API identity, based on the pinned reconstructed ABI dialect. This is
not a measured NVIDIA runtime version or full conformance. Unsupported APIs and
properties still fail independently. The contract and source hashes are in the
build metadata; runtime bootstrap logs declare its partial identity explicitly.
See `docs/research/nvn-turnip-study-20261023/device-bootstrap-progress.zh-CN.md`.

`--texture-noaccess-pool` with `--nvn-texture-check` qualifies original shader
sampling from a same-storage 0xa1 pool. CPU_NO_ACCESS public maps return NULL;
valid public mapped-cache operations are NOPs, while initialization performs
the private WB cache handoff before publishing the pool. This uses a native
uncompressed policy and preserves COMPRESSIBLE eligibility; it does not claim
hardware compression. NOACCESS shader sources, descriptor backing and command
arenas remain unsupported. A650 GPU sampling and the cached regression pass;
see `docs/research/nvn-turnip-study-20261023/pool-noaccess-progress.zh-CN.md`.

Explicit NVN origin/depth modes
------------------------------

Generate fresh color/texture checks with `check_graphics.py --modes` or
`check_textures.py --modes`, an independently recorded `--gmem-evidence` and a
separate `build_ir3.py` output containing the current emitter. These schema3
images declare four bounded scalar state relocations. NVN Draw snapshots the
device modes at recording; its delayed descriptor materializer keeps the same
snapshot. Raw legacy image encoding remains available to historical qualifiers.

Pass the schema3 check to `build_android_probe.py --nvn-texture-check CHECK
--texture-modes`, optionally with `--texture-noaccess-pool`. Twelve A650 cases
exercise origin0/1, near-minus-W/near-zero and negative/positive/far clip Z,
including recorded-state mutation. The runner preserves all twelve raw GPU
readbacks and checks exact mirror, visibility and clear images independently.
Current scope is full 64x64 viewport/scissor, depth range0..1, no depth attachment,
cull or FragCoord/front-face consumer; depth storage/testing is not qualified.

The CPU regression uses real core and encoder with a host memory backend:

```sh
python3 nvn-native/tools/check_device_modes.py --root "$PWD" \
  --color-check .local/nvn-native/a650-modes-graphics-check/check.json \
  --texture-check .local/nvn-native/a650-modes-textures-check/check.json \
  --out .local/nvn-native/NEW-modes-host-check
```

It verifies the original checked inputs before generating ignored headers, runs
function-type and address/undefined sanitizers, and checks two different Draw
snapshots in one recording. It executes no GPU commands or original game module.

CPU_UNCACHED visibility through hardware IO coherency
---------------------------------------------------

`nn_bo_import_coherent` imports the same caller pages and retains the WB CPU
mapping. Both import and GET_INFO must preserve requested WB+IOCOHERENT flags;
the exact A650 gate rejects missing bits with no noncoherent fallback. This is
an explicit host-coherent implementation policy, not a claim of an NC CPU PTE.
Public maps keep the caller pointer and mapped-cache operations are legal NOPs.
GPU_UNCACHED remains unsupported independently of the CPU mapping policy.

`build_android_probe.py --iocoherent-inspect` only qualifies import metadata,
with no GPU execution. Add `--texture-uncached-pools` to the schema3 NVN texture
qualifier for actual original-shader CPU-to-GPU reads and GPU-to-CPU writes,
without public Flush/Invalidate. It can combine with `--texture-modes` and
`--texture-noaccess-pool`. Both 17-submit coherent runs and a separate cached
regression pass on A650. Preserve GPU-side cache barriers; hardware CPU
coherency does not replace them. Evidence, scope and the real D2R next boundary:
`docs/research/nvn-turnip-study-20261023/pool-coherent-progress.zh-CN.md`.
