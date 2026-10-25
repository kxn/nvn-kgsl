# NVN native hardware library

This directory contains the independent Android ARM64 / Adreno NVN compatibility library, offline shader tools, authored qualification callers and optional external D2R validation adapter.

Start at the [repository README](../README.md), [current verified status](../docs/STATUS.md), [architecture](../docs/ARCHITECTURE.md), [build instructions](../docs/BUILD.md) and [next work](../docs/NEXT.md). Development conventions are in [AGENTS](../AGENTS.md) and [DEVELOPMENT](../docs/DEVELOPMENT.md).

The runtime uses KGSL, PM4, IR3, FDL and native Android display directly. D2R is optional validation; neither its former Metal/Vulkan renderer nor a Vulkan driver fallback is part of this runtime. Offline SPIR-V conventions do not imply a Vulkan runtime dependency.

At migration there are 406 registered NVN entries, with partial semantics and unresolved gaps; no real D2R Draw/QueueSubmit/Present or frame has been established. See STATUS for exact qualification scopes. The old long README is retained as a clearly labeled [historical document](../docs/history/nvn-native-original-README.md).

Private inputs and all unfinished candidate work are separately preserved outside public Git; see [PRIVATE_HANDOFF](../docs/PRIVATE_HANDOFF.md).
