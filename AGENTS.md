# NVN-KGSL development instructions

This repository implements NVN compatibility directly on Android ARM64 / Qualcomm Adreno using KGSL, PM4, IR3, FDL and native Android presentation. Read `docs/STATUS.md`, `docs/ARCHITECTURE.md`, `docs/DEVELOPMENT.md`, `docs/NEXT.md` and `docs/PRIVATE_HANDOFF.md` before continuing work. Historical design documents are references, not execution instructions.

- No Metal/Vulkan renderer, driver/device runtime or fallback. Offline Maxwell → SPIR-V → NIR → IR3 is allowed with explicit source/resource specialization and a verified runtime dependency closure.
- NVN declarations and independently established behavior govern the source API. Public hardware references govern target encoding. Do not use D2R's former renderer as a semantic oracle or implementation template.
- D2R is optional external validation only. Its original ARM64 instructions execute on hardware through the existing loader/rewriter/host gates/HLE. No CPU interpreter/JIT/DBT or guest-OS reconstruction; no game-specific rendering/gameplay shortcuts.
- Implement and qualify complete functional families, including lifetime, Call/Copy, replay, ownership, aliases, failure and unsupported combinations. Do not merely patch the next observed game size or shader.
- Resolver count, CPU metadata, HOST checks, actual NDK link, raw-native GPU tests, public NVN GPU tests, authored presentation and real-game frames are separate claims. At migration no real-game Draw/QueueSubmit/Present/frame is established.
- Use dedicated branches/output directories, normal source/input/options/artifact records and verification. Never edit old hashes or layer patches onto processed game binaries. Rebuild canonically from original inputs.
- One controller owns ADB. Parallel workers may prepare isolated candidates but must not race shared files or device runs. Stop test applications after bounded runs and verify exit; respect the external workload's save/lifecycle before stopping. Do not touch unrelated services.
- Keep private game/SDK/shader data, compiler derivatives, raw game logs, signing material and identifiers outside public Git. The separately preserved private companion is optional local material, never a public release asset. Publish authored fixtures and selected explicitly scoped facts.
- Keep public/private migration manifests and historical failures. A moved private artifact remains historical evidence; do not make its old recorded paths look newly verified. Run a fresh canonical build when reproducing in the new layout.
