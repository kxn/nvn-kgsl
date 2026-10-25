# NVN-KGSL

An independent NVN-compatible userspace graphics implementation for Android ARM64 and Qualcomm Adreno, using KGSL, PM4, IR3 and native Android presentation directly. NVN semantics are translated into the target GPU's primitives; no Metal or Vulkan renderer, driver or fallback is part of the runtime.

The project is an implementation in progress. It has substantial resource, shader, draw, synchronization and presentation qualification on an Adreno 650, but it does **not** yet render a frame from the real D2R validation workload. The historical resolver registers **406 entries**; that is not 406 fully implemented GPU behaviors. There are **102 known unimplemented entries and 15 entries with unresolved ABI**, and partial semantics inside registered entries also remain.

D2R is an optional external validation project, not the architecture of this library. Mesa/Freedreno, deko3d, public NVIDIA hardware documents and public reverse engineering are source references with different purposes and evidence strengths. Their APIs are not substitutes for an NVN specification.

Start with [architecture](docs/ARCHITECTURE.md), [current status](docs/STATUS.md), [development conventions](docs/DEVELOPMENT.md), [next work](docs/NEXT.md), and [public-data boundary](docs/PUBLIC_DATA.md). [Design provenance](docs/DESIGN_PROVENANCE.md) explains the important original design attachment and which decisions were superseded.

The source migration baseline is the private integration worktree commit `8f052d4`. Historical paths are identifiers for local evidence, not public dependencies or promises that proprietary fixtures are included. New contributors can build and qualify the authored workloads without owning a game. Reproducing the optional original-game checks requires independently supplied lawful inputs.

The handoff also preserves a separate **local-only `nvn-kgsl-private/` companion** with original lawful local inputs, complete historical artifacts/failures and unfinished candidates. It is never uploaded to the public repository. See [private handoff](docs/PRIVATE_HANDOFF.md) and [pending candidate inventory](docs/PENDING_CANDIDATES.md). Old path-bound records remain historical; moved inputs are rebuilt canonically rather than rewriting recorded hashes.

Build from a clean checkout with [BUILD](docs/BUILD.md). The migration was verified with 27 authored HOST tests, a 29-translation-unit Android core, the separate public shader frontend, authored IR3 shaders and a fresh external canonical D2R host build; see the [verification summary](docs/evidence/migration-code-verification.json). These are build checks, not a new GPU or original-game frame claim.
