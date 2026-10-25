> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# CASC graphics capability batch (HOST evidence)

This is research and normal-tool orchestration, with no production patch. All original code/control/C1, extracted original resources, SPIR-V and IR3 remain in this ignored directory. No tracked source/compiler changes, ADB, GPU execution, or game execution occurred.

The original inventory covers 162 logical resources, 10,825 declared containers and 6,901 complete unique identities. All declared graphics controls are 0x880 bytes; 6,648 identities have original C1. VS/FS account for 6,899 unique identities; the two TCS/TES identities are outside current VS/FS compilation. The 21 cs_ resources do not contain containers admitted by this graphics extractor, which says nothing about compute completeness.

Selection is reproducible: all identities in 120 resources with at most eight identities (209 stages), plus min/median/max declared code+C1 size representatives from each of the other 19 VS/FS resources (57 stages). The union is 265 identities, covering all 139 VS/FS logical resource families; it is not all 6,899 variants. State is strict-no-textures-state.json: explicit authored qualification host/depth/render assumptions inherited from the existing normal profile, NO texture answers, and no claim to captured game state.

Normal small and large checks both PASS, each preserving the original inventory/source/control/C1 and 16 negative cases. Union: 66 VS +14 FS pass Maxwell and SPIR-V validation, 173 identities stop at first missing texture fact and 12 at first missing QueryHostHasFrontFacingBug. These are first failures, not a complete shader resource/opcode census. A failure before texture facts cannot be called an unsupported opcode.

Reusing existing nn_pipeline_resource_reference (not a new decoder), two sanitizers agree on 36 first demanded handle/cbuf keys. 22 keys covering 150 first demands decode; 14 covering 23 demands remain ENOTSUP. Decoding a reference proves neither the actual texture type nor normalized mode, sampler, format or native binding. Real immutable descriptor/CB2 evidence is needed; do not select a texture type from the resource filename.

Three explicitly listed resource-name structural pairs are diagnostic candidates, not observed game Program pairs. nvn-graphics-ir3 metadata links all three; nvn-pipeline-ir3 creates a strict native recipe only for simple. lut_view and ui return9 and contain unused invalid-reg FS sysvals, consistent with the explicit current native gate. Do not equate the metadata executable with the native recipe executable, or attribute coarse return9 to a unique cause.

25 unique passed VS have frontend storage resources. Existing actual common IR3 compiles all to native code: 19 have num_uavs=2, six have num_uavs=3. Strict recipe rejects all. The fixed original lut_view FS diagnostic sink itself has the unused-sysval limitation, so these return9 values have a confound; independent actual VS UAV counts and pipeline_artifact.cc's explicit num_uavs hard gate establish the missing graphics storage role. No original shader was edited or wrapped.

## Implementable capability families

| Family | Evidence | Needed implementation | Boundary |
|---|---|---|---|
| Texture query/identity |173 first failures;22 known keys/150 demands|Resolve immutable actual registration/CB2 snapshots through pipeline_resource_queries and nvn_pipeline_source; runtime types/format metadata must reflect actual bound resource|14 keys/23 demands still unknown; no inferred dimension/array/MS/depth|
| Graphics SSBO |25 VS; actual IR3 UAV2/3|Typed NIR access/binding facts; native IBO recipe and descriptor tables; original source/API resource mapping; VS/FS storage snapshots, ordered Call/Copy state, owners/preflight/retirement|Current native recipe rejects all UAV; no raw fallback|
| Vertex sampling |14 VS first texture queries|Per-stage typed resources and real vertex sampler/texture descriptors|Native vs num_samp gate remains; FS support is not sufficient|
| FrontFacing |12 FS first missing capability query|Explicit host query and front/back native systemvalue recipe/qualification|Neither default false nor silently dropped active sysval|
| Unused sysval |2/3 strict pair negatives with invalid-reg unused declaration|Actual compiler use facts distinguish dead declaration from live input; preserve existing recipe bytes|Parent has isolated compiler candidate; this batch does not apply it|
| MRT/other systemvalues |Static native recipe gates only|Separate actual-demand inventory and hardware contracts|Not proven demanded by these samples|
| TCS/TES/CS extraction |2 unsupported declared graphics stages;21 cs resources outside admitted graphics format|Existing original resource/caller evidence before extending tools|No new guessed parser, no claim of compute census|

## Reproduction and provenance

Use the already recorded argv JSON files: extract-argv.json, large-extract-argv.json, large-compile-argv.json, structural-extract-argv.json, structural-compile-argv.json. Normal tools are exact copies under normal-tools. CASC compile explicitly sets pair_selection=[] for stage census. Do not use offline_graphics_stages.py's implicit default Cartesian pairing on a bulk directory. run-ssbo-diagnostics.py invokes existing actual compilers only; it does not parse original binary or implement shader translation.

Run `python3 freeze_summary.py` to recompute summary.json and verify every input/artifact hash in the nine final normal records. Final run verified 19,797 input/artifact entries. summary.json pins every normal record; ready.json pins the report, summary and orchestration. Existing normal check passed means provenance/outcome consistency, not that every shader is native/GPU compatible.
