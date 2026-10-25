# Build and resume

The normal backend and authored smoke tests do not require D2R, a Nintendo SDK,
a game, or a shader dump. Run commands from the repository root.

## Public baseline

Use Python 3, CMake, Ninja and a C compiler. The migration was checked with
Android NDK `28.2.13676358`, Android API 29 and ARM64.

```sh
python3 nvn-native/tools/check_standalone.py --out .local/smoke
python3 nvn-native/tools/check_standalone.py --out .local/android-smoke \
  --ndk "$ANDROID_NDK_HOME"
python3 tools/audit_public.py --out .local/public-audit.json
```

The public baseline runs 27 synthetic HOST tests. With an NDK it also builds
the default standalone library/probe and checks its actual dependency closure.
It does not run a phone, an NVN workload, or a game. The legacy `tests/` tree
also contains CLI qualification programs and private-evidence readers; whole
directory unittest discovery is not a supported baseline command.

To build the ordinary standalone command-processor probe APK, provide a JDK,
Android SDK platform 36/build-tools 36.0.0 and the NDK explicitly:

```sh
python3 nvn-native/tools/build_android_probe.py \
  --sdk "$ANDROID_SDK_ROOT" --ndk "$ANDROID_NDK_HOME" --out .local/probe
python3 nvn-native/tools/run_android_probe.py \
  --build .local/probe --serial "$DEVICE_SERIAL" --install \
  --evidence .local/evidence/command-processor.json
```

Device commands are explicit. Check the supported chip gate first; do not
silently reuse an A650 shader/layout artifact on another GPU. Preserve signing
material locally, use one ADB controller, keep complete evidence, and verify
that the test process stopped. Running this command is a new qualification,
not a promise that this checkout has already executed it.

## Optional offline shader tools

Only the public shader frontend is used, never a CPU emulator or its runtime.
Provide the compatible public source checkout separately; it must contain
`src/Ryujinx.Graphics.Shader`, `src/Ryujinx.Common`, and `src/Spv.Generator`.
The builder records the exact four-project source closure and enforces its
instruction-overlay source hashes. The frontend project currently targets .NET 10.

```sh
python3 nvn-native/tools/build_maxwell.py \
  --shader-frontend-root "$NVN_SHADER_FRONTEND_ROOT" \
  --dotnet "$DOTNET" --out .local/maxwell
python3 nvn-native/tools/build_ir3.py \
  --mesa-archive "$MESA_ARCHIVE" --meson "$MESON" \
  --host-tools "$HOST_TOOLS_PREFIX" --out .local/ir3 --jobs 6
```

The IR3 producer requires the pinned Mesa 26.2.5 archive SHA-256
`794b3d1729075dae7ebd47c7283b7ca0cdb5dec668f7f11cc0a5d82f6138a0a5`.
`HOST_TOOLS_PREFIX` contains `usr/bin/{bison,flex,m4}` and `usr/share/bison`.
NDK shader-tools supplies glslc and SPIR-V tools for the authored shader
qualification producers. IR3/FDL are compiler/layout dependencies; no Turnip
or Vulkan renderer is linked into the runtime. Check the selected producer's
`--help` for its normal input records and never manufacture a passing record.

## Optional external validation project

Set `D2R_ROOT` to a separate D2R checkout. The private companion includes a
verified source bundle and its exact original-tree provenance. Supply original
game inputs separately; none are downloaded or included by this repository.

```sh
python3 nvn-native/tools/build_d2r_activity.py \
  --d2r-root "$D2R_ROOT" --exefs "$NVN_PRIVATE_ROOT/originals/d2r/3.3.94012/ExeFS" \
  --ndk "$ANDROID_NDK_HOME" --python "$(command -v python3)" \
  --out .local/d2r-activity --jobs 6
```

This minimal command validates the external loader/HLE integration. Full GPU
integration requires explicitly selected current normal compute, FDL, texture,
graphics and source-catalogue records; see [pending work](PENDING_CANDIDATES.md),
[next steps](NEXT.md), and the private handoff indexes. Original modules must
always be rebuilt canonically from unmodified inputs. Old path-bound records
are historical evidence and must not be edited to authorize a new deployment.
