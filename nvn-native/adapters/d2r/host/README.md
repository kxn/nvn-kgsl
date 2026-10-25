# Original Android ARM64 CLI host with native KGSL

This target references the original `dm/host/android/main.c`, loader, ARM64
entry gates, HLE services and platform sources. It links the typed D2R bridge
OBJECT directly to retain `swforce`, together with the independent `nvn_kgsl`
archive. It excludes `graphics_boundary.c`, `hle_nvn.c`, old renderer targets
and APK/JNI sources. It adds no loader, CPU execution engine or service stubs.

From the repository root, build private artifacts with explicit original inputs:

```sh
/path/to/python-with-lz4-and-capstone nvn-native/tools/build_d2r_host.py \
  --python /path/to/python-with-lz4-and-capstone \
  --exefs /path/to/original/ExeFS --ndk /path/to/android-ndk --out .local/d2r-host
```

Repeat the same command with `--verify` to check cached host, module and name
artifacts, source/tool identities, effective options, original NSO identities,
and the actual compile/link closure. Verification never builds or executes the
host. A changed or incomplete artifact is rejected before any cache reuse.
Output must be an ignored directory under this checkout's `.local` directory.

The single build entry calls `scripts/dev/rewrite-native.py` with `--abi elf
--rules native --private-gates 1`, then `--verify-only`. The canonical pipeline
starts from original `main`, `sdk` and `subsdk0` NSOs; it performs read-only git
queries for pinned upstream and checkout provenance. It never adds patches to
an already rewritten binary. Original module bytes remain private build outputs.
The existing name extractor generates and verifies private NVN metadata from
the original main NSO; no game binary or generated name header is committed.

The ABI is HOST_ABI1: native ARM64 AAPCS64, original initial-exec ELF TLS and
hardware entry/SVC gates. The build rejects compiler-emulated TLS. This is a
CLI host, with `DM_HEADLESS=1` and strict original HLE; it is not an ABI2/3 APK
or a presentation host. All probe headers and modes, including
`NVN_AHB_GRAPHICS_HEADER` and `NVN_AHB_PRESENT`, are cleared explicitly.

`build-record.json` records inputs, upstream identity, rewrite manifest,
source/tool hashes, commands/options and all deployable host/module/name
artifacts, object files and link map. `link-audit.json` records the 36 original
host/platform/HLE source units, bridge and five driver source units, dependency
closure, AArch64 PIE identity, 16 KiB LOAD alignment, `swforce` presence and
Android system shared-library dependencies. No shader profile is registered
by this minimal host.

The unchanged original CLI accepts `DIR --sdk-init`, `DIR --constructors` and
`DIR --run` (and its diagnostic `--entry SYMBOL` mode). These modes respectively
load and initialize the real SDK modules, also execute module constructors, or
also call the real `nnMain`. Building and verifying does none of these things.
Device execution is a separate, coordinator-controlled step after screen and
lifecycle acceptance; this tool contains no deploy or run command.

Link success establishes no runtime service coverage. The first previously
identified real NVN startup gap is `nvnDeviceBuilderSetFlags` (0x100/0x115).
The typed bridge reports unsupported/missing calls or sticky driver errors
and exits 78 rather than allowing unwritten output parameters to propagate.
Device version properties, further pool/queue options, window presentation,
full host frontend services and real-game shader registration remain separate
work. Existing headless UI subscriptions fail explicitly; unavailable text
input returns its existing failure result. No new success responses are added.
