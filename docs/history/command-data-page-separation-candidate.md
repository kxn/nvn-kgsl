> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Executable wrapper / completion-data page separation candidate

This is a correctness-contract candidate, not a proven cause or fix of the
A650 mixed compute/graphics EDEADLK. No device operation was performed here.
No Metal/Vulkan implementation, shader change, runtime compiler or CPU image
copy is involved.

## Frozen promotion

Apply only `minimal-promote.patch` (two files):

- `nvn-native/src/texture_bridge.c`: `nn_tex_shadow_chain` allocation / own
  completion-address calculation only.
- `nvn-native/tests/d2r_bridge/texture_bridge_words.c`: existing mask comparisons
  mechanically use the actual `nn_device_page(d)` rounded command end; all masks,
  packet parsing, owner and readonly checks remain. An independent page-boundary
  contract checks source counts 0/1/2, absent/present writeback and executable
  endings immediately before/on/after a page boundary.

No normal tool, CMake, core, producer, resource list, queue or shader change.
The frozen old chain emitter and its linked packet-equivalence tests stay
ignored. They are not part of the production patch or normal checker.
`ready.json` records exact source, patch, evidence and public-source hashes.

## Contract and byte comparison

Let `E` be the original final executable byte count and `P` the actual device
page size. The candidate sets `D=round_up(E,P)`, requires a valid power-of-two
page, checks arithmetic and the freshly allocated BO's GPU address alignment,
and allocates `D+4` bytes (normal) or `D+8` (diagnostic). The allocator rounds the
allocation to whole pages. Only `[0,E)` is submitted as executable commands.
The sequence word lives at `D`; diagnostic progress at `D+4`, with its observer
track offset updated accordingly. The gap is zero initialized but not executed.
The whole existing BO remains owned/referenced as before; this is logical page
separation, not GPU read-only page-table enforcement.

First-clear length representative: normal 4679 DWORD / 18716 bytes and
instrumented 4707 DWORD / 18828 bytes stay exact. Both now put data at 20480,
with allocator-rounded BO size 24576 rather than the old 20480. All original
order calls, embedded pre/post compute streams, child commands and their
absolute resource/table/sequence GPU addresses remain unchanged. In normal
mode, no NOP or progress MEM_WRITE was introduced.

The ignored migration test links the pre-change encoder and new encoder with
the same live real conversion owners and child command input. It compares every
complete packet, allowing only the wrapper's own EVENT_WRITE sequence address
and existing diagnostic MEM_WRITE progress address to differ. It covers zero,
one and two inputs, writeback, representative 4320-DWORD child length, exact
4095/4096/4097 and 8191/8192/8193 total DWORD counts, all existing diagnostic
masks, invalid page metadata, failed cache-publication + failed-free retention,
and pending-owner EBUSY. Synthetic NOP child contents exercise length/boundary,
not actual clear pixels; actual core/probe fixtures are additionally linked.

## Verification and limits

- `checks-final/proof.json`: both GCC ASan/UBSan and Clang function-sanitizer
  packet migration tests; actual core and Window HOST fixtures; actual authored
  probe no-GPU and unknown-submit owner retention in normal/diagnostic modes;
  real KGSL/platform/probe ARM64 API29 strict shared links in both modes.
- `promote-proof.json`: final minimal production fixture, with no old emitter
  dependency, normal GCC ASan/UBSan 80608 checks and diagnostic Clang function
  85233 checks. It is compatible with the unchanged normal checker source list.
- Earlier `checks-v1` intentionally preserves its harness-only Clang failure:
  linker wrap arguments were passed to `clang -c`; fixed in `checks-v2` and the
  final direct-source migration harness. No runtime failure was hidden.

No GPU pass, hang resolution, general driver-wide read-only command memory, or
IB-length/alignment hardware requirement is claimed. Root must rebuild the
normal uninstrumented APK with its ordinary source closure and perform the
single-device qualification. A failure must preserve the same submitted-owner
set until process exit. Existing data outside this wrapper (including separate
compute-owner scratch and standalone Window conversion entry) is unchanged.

## Public source facts, not a hardware diagnosis

Pinned Mesa `adreno_pm4.xml:263-275` documents IB prefetch parser / pipelined
IB dispatch terminology. It does not state that completion scratch sharing an
executable page causes a fault, and does not prescribe page-separated data.
`adreno_pm4.xml:2345-2350` describes a 20-bit DWORD IB size. Gallium
`fd6_emit.h:438-459` emits the actual command count. Freedreno
`freedreno_ringbuffer_sp.c:499-509` records the actual byte offset as command
size. Official MiCode pipa source `adreno_ringbuffer.c:1119-1129` emits the
actual `size >> 2` masked to 20 bits, with no rounding to 8/16 DWORDs.
These facts rule out inventing a documented executable-length alignment rule.
They justify preserving exact executable bytes while independently qualifying
stronger command/data page isolation. The public kernel source URL/commit and
local SHA are recorded in `ready.json`.
