# Original native Activity host

This standalone shared target reuses the original ARM64 loader, HLE, entry gates,
thread/TLS adaptation and startup sequence with the typed native KGSL bridge.
It does not include an Activity/JNI controller, old renderer, CPU emulator, or
new system services. The original CLI target and its default process exit stay
unchanged.

Build from unmodified original ExeFS inputs with:

```sh
python nvn-native/tools/build_d2r_activity.py --exefs ORIGINAL_EXEFS \
  --ndk ANDROID_NDK --out .local/nvn-native/d2r-activity-host
```

Use the same arguments with `--verify` before packaging. `--python` selects the
interpreter with the canonical tool's dependencies. The tool invokes the existing
`rewrite-native.py --abi elfdesc --rules native --private-gates 1`, then verifies
the full three-module bundle and generated NVN names. It records original input
identity, upstream/tool/source fingerprints, effective options, actual link
closure, module manifest and every artifact hash. Outputs remain ignored. No
module or GPU execution occurs during build/verification.

The output is a genuine shared library `libdmhost_activity_kgsl.so`, with original
TLSDESC/HOST_ABI2 modules. Never load the ABI1 CLI PIE alias as a library or mix
module ABIs. The symbol audit requires native ELF TLS/TLSDESC and rejects actual
emulated-TLS accessors/variables. NDK crtbegin may include a harmless
`__emutls_unregister_key` startup placeholder; that name alone is not evidence of
emulated TLS. Only the six functions in `activity_host.h` are public exports.

The controller must obtain a real SurfaceView ANativeWindow and hold it through
`attach`. That call obtains the exact original VI `&g_native` identity through a
host-only getter, configures the original display profiles, and registers/attaches
that token in the same process. It never changes the token returned by the four
original VI HLE_FORCE entries. Failed attach clears the generation output.

Run `check_bootstrap` on the owner for typed NULL-device resolver lookup only;
it does not create a GPU device or run modules. Run `run(verified_modules, stage)`
on the original guest-owner worker, once per process: stage1 SDK, stage2
constructors, stage3 nnMain. Separate stage runs require separate processes.
The new `DM_NATIVE_SHARED_ENTRY` macro lets only this target's original main
return to its owner. A zero stage return is not guest initialization or rendering
success; the controller must retain/classify full logs, including resource errors.
Unsupported NVN APIs/arguments still stop through the typed bridge with Exit78.

UI `close(generation, timeout<=1500)` only requests/observes compositor stopping.
It never enters core/BO/KGSL or proves GPU retirement, and does not replace original
game saving/suspend. Live-game Surface rebinding is explicitly unsupported:
attach after host startup returns ENOTSUP. Lifecycle restoration, original
features/input/audio closure and final teardown need separate integration.

DISPLAY support remains unchanged: qualified Window resources are explicit
external 64x64 RGBA8 images with two/three slots. Ordinary game pools do not gain
AHB owners here; no GPU copy/blit or hidden backing replacement is implemented.
This target being linkable does not establish real D2R Window calls or firstframe.

Reproduce the pure-host boundary tests with:

```sh
python nvn-native/adapters/d2r/activity/tests/check.py \
  --out .local/nvn-native/d2r-activity-host-tests
```

They call the actual original VI HLE section and display code, with explicit RAM
mocks for ANW/registry/main/resolver. ASan/UBSan and function ABI checks validate
41 frontend cases, and actual-main preprocessing/syntax checks cover both exit
macros. These tests execute no original modules/GPU and never enter the production
shared target. Packaging, real Android controller and execution belong to the
parent integration step.


`notify_focus(1/2/3)` forwards only to the existing original `dm_focus_set`:
InFocus / OutOfFocus / Background state plus notification15. The game consumes
that state through its existing oe HLE. Zero means accepted notification, not
save, consumption, clock/audio/GPU suspension, or Surface/game resume. This
requires no new original source/dependency; hle_oe.c is already selected.
The controller publishes its initial resumed state before original entry and
Background on pause. Its one-shot stop/close and reattach rejection remain.
