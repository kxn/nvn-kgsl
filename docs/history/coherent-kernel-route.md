> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# WB + IOCOHERENT: stage 1 metadata candidate

Scope: ignored-only source research and compile/host fault-injection checks. No ADB, no GPU execution, no allocator/heap replacement, no tracked edits. The pinned public source is MiCode commit `77dc8e2290f3f5f15861821564f68d30ea193449`; it is NOT claimed byte-identical to the running device kernel. Actual device qualification must inspect returned flags.

## Static evidence

- `adreno-gpulist.h:1219` a650 rev0 and `:1249` a650v2 ANY_ID set ADRENO_IOCOHERENT. `../pipa-adreno.c:1556` propagates this capability into KGSL_MMU_IO_COHERENT.
- `../pipa-kgsl_sharedmem.c:888-913` clears USE_CPU_MAP if unsupported, clears IOCOHERENT if MMU feature absent, and clears IOCOHERENT for WC/UNCACHED. WB plus IOCOHERENT can survive. No proof is inferred from chip name alone.
- `../pipa-kgsl.c:2692-2724` ADDR GPUOBJ_IMPORT allowlist retains IOCOHERENT and CACHEMODE, then calls kgsl_memdesc_init. `:2677` routes anonymous storage to `kgsl_setup_anon_useraddr:2529`; `:2470` pins original get_user_pages and builds its scatterlist. It does not replace existing CPU PTEs, allocate a shadow buffer, or copy bytes into another backing.
- `../pipa-kgsl.c:2845` returns sanitized memdesc.flags in import result. GET_INFO `:3693-3704` returns flags/size/address and explicitly returns useraddr=0 because multiple CPU mappings are possible. Thus GET_INFO.useraddr cannot prove pointer identity.
- `kgsl_iommu.c:1789-1821` turns retained IOCOHERENT into IOMMU_CACHE for GPU mapping. This is a real SMMU mapping attribute path, not just userspace metadata. Exact running-kernel behavior remains unmeasured.
- Legacy MAP_USER_MEM is unsuitable: its sanitization/anonymous cachemode path previously lost WB on the actual device. Use the already-qualified GPUOBJ_IMPORT ADDR shape, priv_len=mapping bytes, with the explicit extra IOCOHERENT request.

The stage 1 candidate uses original anonymous mmap pages, retains the same submitted pointer and private CPU view, and requests WB (physical CPU mapping remains WB). It reports physical_identity_measured=false; no pagemap PFN reading or GPU identity test is performed. It must never be described as a PTE=NC allocation.

## Candidate contract

`int nn_iocoherent_import_probe(const char *directory, char *json, size_t capacity)`; directory unused, capacity >=2048. Operation `kgsl_iocoherent_import_metadata`. Exact A650 0x06050002/page4096/MMU gate, 8192 original anonymous bytes. Both import.flags and GET_INFO.flags must retain WB and IOCOHERENT; size, writable/nonsecure status, id and aligned GPUVA must match. Missing bit => ENOTSUP, no fallback, no submit. A successful return only means metadata_gate_passed=true. shader_coherency_qualified=false and nvn_uncached_qualified=false always; gpu_submissions/cache_maintenance_calls=0 always.

No mutation retry. Ambiguous import error or failed free retains fd+original mapping for process teardown and reports resources_retained=true, never a passed gate. On ordinary bit rejection the known object is freed, then original pages unmapped and fd closed. Root should deploy through a normal source-recorded build; this shared object is only a strict compile artifact, never an unrecorded binary deployment.

Reproduce strict compile (from worktree root):

```sh
${D2R_REFERENCE_ROOT}/.local/android-sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang \
  -std=c11 -O2 -Wall -Wextra -Werror -fPIC -shared -Wl,--no-undefined \
  -Invn-native/third_party/kgsl \
  .local/nvn-native/parallel/iocoherent-stage1/iocoherent_import_probe.c \
  -o .local/nvn-native/parallel/iocoherent-stage1/libiocoherent_stage1.so
```

Host fault injection: test_metadata_gate.c uses only mocked kernel calls, no GPU. Eight cases: accepted flags, import bit cleared, GET_INFO bit cleared, wrong size, GET_INFO failure, free failure, ambiguous import EINTR, wrong chip. Both GCC ASan/UBSan and Clang function trap passed; initial Clang ASan link failed because the local Clang ASan runtime libraries are absent, retained as a tool limitation in proof.json. No running-kernel success is claimed.

## Stage 2, only after real stage 1 flags pass

Root may add a distinct import_coherent API with exact request+verify and no fallback. Existing normal WB import/cache behavior must stay intact. The API must retain original map pointer and pages; no alternate allocator, MAP_FIXED, hidden copy, or host-mapping cachemode fabrication.

For visibility qualification use original already-qualified Maxwell shaders through existing native pipeline. Keep code/command storage on its already-qualified path; use explicit coherent storage for target plus vertex, uniform and sampled texture resources. Across repeated submissions overwrite unique source/UBO/texture patterns after recording, keep CPU-written lines hot, pre-read old target cachelines using volatile reads immediately before submit, and after Finish read actual target through the same CPU pointer without invalidate. No public Flush/Invalidate and no private TO_GPU/FROM_GPU on those coherent resources. Keep GPU-side UCHE/descriptor/RT synchronization required by the original qualified command template: CPU coherency does not authorize dropping GPU cache barriers.

Existing four-case closed-form pixel oracle/guards and changed inputs prevent constant/stale output from passing. In addition original qualified compute source-to-target copy tests can isolate SP load/store visibility; CP-only writes are insufficient. Use no counter/timer sampler. A CPU ordering fence is not a cache-maintenance operation; report it separately and never claim the test guarantees cache residency (ordinary userspace cannot prove residency at each GPU access). Observe actual values in both CPU-to-GPU and GPU-to-CPU directions over repeated different inputs.

Only after this may the project define an explicit host-coherent implementation policy for CPU_UNCACHED visibility. Report CPU mapping WB and hardware coherence separately; do not claim physical noncached mapping or complete NVN pool support, and do not infer unsupported GPU_UNCACHED behavior from IOCOHERENT.
