# Historical documents and evidence

All **156 historical Markdown documents** from the integration worktree's research directory have been retained under `history/`, with their relative subdirectories. They preserve implementation detail, counterexamples, failed trials, upstream references, qualification scope and original plans. They are historical records: [STATUS](STATUS.md) states the latest verified frontier and [NEXT](NEXT.md) states continuation work. A successful old probe does not imply current full NVN support.

[history-manifest.json](history-manifest.json) records each original document's SHA-256/length, the published SHA-256/length and its transformation classes against source commit `8f052d4`. Original files were not edited. Every published history document has an explicit historical-status banner. Local absolute paths/device addresses were replaced by placeholders, and four reports' short original-SDK instruction excerpts were replaced by original semantic explanations while retaining the findings. The self-authored probe instruction in the device bring-up explanation remains distinguishable from a proprietary SDK dump.

## Unpublished artifacts

Many historical reports refer to JSON records, raw GPU readbacks, screenshots, logs, generated programs or source snapshots that lived beside them. **A link to this section means that original artifact is not supplied by that link in the public repository.** The accompanying historical target identifies what was referenced. It must not be read as proof that the file can be obtained from this checkout or as a promise of a publicly reproducible game-derived fixture.

The old research directory held 1,564 files totaling 891,649,507 bytes, including 77 ZIP archives, 53 logs, 49 PNGs and hundreds of raw or structured results. Filename suffixes cannot establish safe publication: a normal-build JSON can embed source shader data, and a source-snapshot ZIP can contain generated game-derived headers. The migration does not bulk-upload these artifacts. Selected public evidence is added only with explicit producer/input provenance, retained hashes and a content audit.

The original game/SDK inputs, code/control/C1, generated derivatives, assets and raw logs remain private. The implementation tools and authored fixtures support further development without those files. Optional real-workload verification requires separately supplied lawful inputs; its public selected summaries preserve the actual observed frontier and limitations without publishing a game's payload.

The original user design ZIP and its complete extraction are different: all 67 members were individually inspected as design/reference/model material, and the user explicitly requested preservation. They are retained unchanged with archive/member hashes under [reference/design-v3](reference/design-v3/README.md), including historical architecture and API tables. Its old Turnip bridge proposal does not change today's direct-KGSL runtime.
