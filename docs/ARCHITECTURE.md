# Architecture and boundaries

The compatibility frontend owns independently reconstructed NVN types, resolver entries, guest-object sidecars, recording and validation. Hardware operations are implemented by native resource, shader, state, submission and presentation layers. The source-level semantics and hardware representation are deliberately separate.

```text
NVN calls / independently authored qualification caller
        ↓
ABI validation + object/recording/resource semantics
        ↓
Draw-time source / recipe / resource specialization
        ↓
Adreno resources + FDL layouts + PM4 state + IR3 programs
        ↓
KGSL BO/context/submit/wait → Adreno GPU
        ↓
AHardwareBuffer + SurfaceControl + Android lifecycle
```

`src/` and `include/nvn_native/` contain the library. `shader/` supplies offline compiler tools; `shaders/` contains authored GLSL qualification and copy kernels. `tools/` orchestrates normal builds, immutable records, verification and qualification. `tests/` exercises contracts and actual command encoding. `android/` contains standalone device callers. `adapters/d2r/` is an optional external-loader integration boundary; it must not pull renderer objects into the core library. Paths may retain the historical `nvn-native/` source prefix until the standalone path migration is complete.

The offline shader path may reuse an independently audited Maxwell frontend, emit SPIR-V, import NIR, and compile/link IR3. Intermediate frontend names containing Vulkan identify SPIR-V descriptor conventions; they do not authorize linking a Vulkan runtime. IR3 alone is not a Maxwell decoder. Shader identity, compilation and runtime resource specialization are separate capabilities.

The source catalogue matches complete original code/control/C1 identities. A Program owns the selected source metadata; it does not depend on a released source pool remaining CPU-readable. The current private catalogue contains 6,903 identities (2,344 VS and 4,559 FS), while the current compiled catalogue contains only 55 explicit stage specializations and 96 successful explicit VS/FS pairs. Identity registration does not make all those shaders executable. Unknown queries, handles, stages, resource roles or recipes remain unsupported before a GPU job is accepted.

Canonical source bytes, native GPU image layouts and CPU observations require explicit conversion and visibility. Resource ownership survives accepted and unknown submission outcomes. Complete known failure before acceptance can roll back; accepted/unknown work cannot be replayed or prematurely freed. Call/Copy, descriptor consumption time, source changes, replay input changes and synchronization are qualification domains, not names of assumed behavior.

The optional D2R route keeps original ARM64 hardware execution, original offline rewriting/loading/relocation, host gates and existing HLE. CPU interpreters, JITs, binary translation and recreated guest operating systems are out of scope. The general graphics library contains no camera, scene, gameplay or game-state manipulation.
