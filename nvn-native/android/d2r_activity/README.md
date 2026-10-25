# Thin original-host NativeActivity controller

Build-only integration candidate. This library links Android/dl/system C/math
only, then dlopens `libdmhost_activity_kgsl.so` and resolves its six exact typed
interfaces. It never links another NVN core, copies the loader, or implements
services/CPU execution. Reuse `org.nvn.nativeprobe.PresentActivity` and the existing
probe signing key. The generated manifest selects `nvn_d2r_activity_controller`.

Configuration is exactly 35 bytes in `files/d2r-activity/run.cfg`: 32 lowercase
hex nonce, space, stage digit1/2/3, newline. The controller creates a new nonce
run directory and fresh O_EXCL stdout/stderr. Each status document is written to
a fresh O_EXCL `.pending`, flushed/fsynced/closed, then hard-linked atomically to
a previously absent final name. Existing success is never overwritten. Status
records carry actual pid/uid/SELinux context and `/proc/self/stat` start ticks.

One detached owner waits for the real resumed SurfaceView ANW, attaches the exact
original VI token, checks bootstrap, then invokes the original SDK/ctors/nnMain
entry. UI calls only close(timeout0), never joins or accesses GPU/core. Independent
UI/owner state references and ANW references survive onDestroy/owner races. The
original host library deliberately remains loaded for its threads/TLS/gates;
there is no unload contract. Surface recreation after starting is rejected. This
is not complete original-game save/suspend/resume support, DISPLAY backing, or
firstframe support.

Environment and cwd match the CLI isolated tree: DM_ROOT/ROMFS/SAVE/TEMP point
to `files/d2r-host/game/{root,romfs,save,temp}`, and DM_IMPORTS/BOOTPROF/FS are1.
ABI2 modules deploy separately under `files/d2r-activity/modules`. No resources
are fabricated. The runner requires local original RomFS plus complete existing
device inventory and per-file SHA256 verification for `--stage run`.

Build with `nvn-native/tools/build_d2r_activity_apk.py --host-record RECORD --sdk
SDK --ndk NDK --keystore EXISTING_KEY --out IGNORED`; use the same arguments with
`--verify`. It requires a fresh verified ABI2 shared-host record, calls canonical
host/module/name verification with its recorded Python explicitly, and records
controlled sources/tools/sysroot build inputs/artifacts. Old candidate APKs are
not reused after provider/core/HEAD fingerprints change.

`run_d2r_activity.py --serial SERIAL --build APK_DIR --evidence FRESH.json --stage
sdk-init|constructors|run [--install] [--romfs ORIGINAL --reuse-romfs]` validates
all local records before ADB, verifies installed APK/libraries, deploys and hashes
complete ABI2 modules, and validates original Activity pid/start identity, ordinary
UID and real untrusted_app context. It finally force-stops only this package and
checks its process disappearance. Fresh raw status and full post-stop logs remain
hashed evidence. `--validate-only` invokes no ADB and reports only local validation.

An Activity supplies no waitpid status. `process_exit_code` stays null. A terminal
native NVN diagnostic plus proven original-process death can identify an unsupported
boundary; the runner never fabricates exit78 or derives a natural boundary from
its own force-stop. Result0 means a returned entry, and resource/initialization
errors override success. Every result keeps first_frame false.

Pure-host checks (explicit mock Android/host APIs; no game/GPU/ADB):

```sh
python nvn-native/android/d2r_activity/tests/check.py
python nvn-native/android/d2r_activity/tests/check_runner.py
```

The first runs14 real-controller lifecycle/config/failure/publication scenarios
under GCC ASan/UBSan and Clang function ABI traps. The second checks13 Activity
classification cases plus4 source/tool/build-input/artifact corruptions rejected
before any subprocess/ADB. They are not mock device execution evidence. Actual
Android deployment and all three original stages remain for the root integration.
