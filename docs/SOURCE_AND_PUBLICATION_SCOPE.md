# Source and publication scope

| Source/category | Handling | Reason and retained value |
| --- | --- | --- |
| Independent NVN/KGSL implementation and authored qualification sources | Public source, with inherited dependency notices | Reusable work, without requiring the validation game's binaries. |
| 156 historical research Markdown documents | Retained under `history/`, original/published hashes in `history-manifest.json` | Preserve all fact reports and next-work context. Historical banner, local-path placeholders, fixed public links; four SDK instruction examples paraphrased without dropping their semantic conclusions. |
| User design ZIP and all 67 extracted members | Retained unchanged under `reference/design-v3/` | Explicit user-preservation request and per-file inspection found design/API tables/models/public patch references, no discovered game payload or private SDK source dump. Original hashes retained; third-party rights are not relicensed. |
| Design's two Mesa Turnip host-import/image patches | Historical reference only, not linked | Public-source patches document the old proposal; current runtime does not use a Vulkan device or Turnip driver. |
| Mesa compiler/layout and public hardware reference sources | Pinned external references and permitted tools/code with actual notices | Reusing NIR/IR3/FDL knowledge does not entail reusing a Vulkan runtime; target hardware facts do not define source NVN semantics. |
| deko3d | External independent reverse-engineering reference | Its API and defaults are its own, not an NVN specification or drop-in implementation. |
| Public reconstructed NVN declarations | Preserve their actual source/version/license and known corrections | Declaration/ABI evidence is separate from behavior, lifetime and synchronization evidence. |
| Private game/SDK executables, assets, saves, shader/control/C1 and derivative compiler artifacts | Excluded from public tree/history/releases | Copyrighted content is not necessary for the independent authored development route. |
| Raw original-game logs and game-derived captures/source-snapshot archives | Excluded; publish selected original fact summaries and hashes | Logs can carry shader hex/data, private paths and game contents; generic archive filenames do not prove safety. |
| Signing material, tokens and developer/device identifiers | Excluded or replaced by placeholders in instructions | Configure externally; they are not required project provenance. |

The historical Markdown reports containing proprietary-SDK instruction examples were `abi-recovery-sdk-research.zh-CN.md`, `raw-storage-sdk-research.zh-CN.md`, `sdk-defaults-recording-research.zh-CN.md` and `texture-view-sdk-research.zh-CN.md`. Those examples are now semantic descriptions. Offsets, names, hashes, default numeric values and qualified behavioral conclusions remain as factual research metadata. No full SDK disassembly or original bytes are introduced by these reports.

Any public result derived from a private shader must say that the original input is omitted and reproducibility requires that optional external input. It cannot be advertised as the public authored regression suite. No game frame has been established at migration.
