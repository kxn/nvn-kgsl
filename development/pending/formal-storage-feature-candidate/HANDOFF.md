# Formal graphics storage feature

Baseline: `8f052d4`, branch `codex/nvn-turnip-study-20261023`. Only the ignored `tree/nvn-native` snapshot was edited. No ADB or ROOT tracked edits.

The thin patch adds `--graphics-storage`, default OFF, throughout ABI1 and Activity ABI2 canonical builders, build options, checked manifests, exact verification, APK canonical verification and CLI execution verification. `NVN_D2R_GRAPHICS_STORAGE` conditionally compiles only `pipeline_storage.c`, supplies the two native/public storage macros, and registers the exact normal typed storage archive at Device initialization. Existing FDL archive ownership remains with the explicitly enabled target FDL provider; storage adds no second library. OFF compiles no storage helper and introduces no storage macros or archive registration. No compiler, shader producer, runtime semantics, or NVN entry counts change.

ON requires both `--graphics-pipeline-check` and `--pipeline-target-fdl-check`; the former must be a normal, currently verified, explicitly `publish_storage_recipes=True` typed archive. The latter must match the exact verified target FDL library. Missing dependencies reject before raw ExeFS reads. Non-bool serialized options reject in APK and CLI canonical forwarding. Host cache verification compares options and the full storage manifest/input fingerprint; the actual Ninja closure verifies ON/OFF macros, unique storage source, exact registered header and normal FDL link.

Use the existing normal package with storage publication:

- `.local/nvn-native/graphics-full55-front-storage-pipelines-v2/check.json`
- `.local/nvn-native/runtime-fdl-front-storage-integrated-v1/check.json`

No records or generated headers were altered to fit the candidate. The checker loads the old records' exact original normal producer modules before validating dependencies. This is necessary only for isolated-candidate testing; after promotion the tool paths naturally coincide.

Validation:

- `policy-v1/check.json`: 9 assertions / 26 rejects.
- `links-v1` failed on my accidentally selected old `d2r-host-cli/names/names.inc.h`: obsolete ABI declarations correctly failed bridge forwarder type checks. This failure is retained. No type check was relaxed.
- `links-v2/check.json`: all 4 actual Android API29 NDK modes PASS, 33 assertions / 34 rejects. Uses genuine v20 canonical names. ABI1 OFF/ON contains 66/67 compiled sources; Activity OFF/ON contains 67/68. Each uses all 36 original host/HLE/platform sources, independent native KGSL sources and no old renderer or qualification source.
- `links-v3/check.json` is the final checker revision: PASS: 40 assertions / 37 rejects / all 4 real links. Additionally compares every actual compiled source with the exact selected production closure, and rejects 3 direct CMake missing-dependency configurations. See ready.json for the final result.

Each ELF is AArch64, uses strict `--no-undefined` links, and has no Vulkan/EGL/GLES/C++ runtime dependency. `nn_pipeline_storage_archive_validate` and `NN_GRAPHICS_PIPELINE_STORAGE_ARCHIVE` exist only in ON ELFs. These are real NDK link qualifications, not Android execution, original module execution or public NVN GPU proofs. The prior ROOT native storage 192 GPU qualifier remains separate; this patch does not change its runtime/compiler sources.

After promotion, rerun the normal feature checker against current ROOT paths and verify a fresh canonical original host/APK. If combining storage with complete-source/depth sampling, use the same normal published storage+depth package and current source catalogue, and retain the complete source/ELF audit. Unknown reflection and unresolved public NVN query/selector roles still reject with ENOTSUP; this feature does not enable an alternate compiler or invent query answers.

No proprietary shader blobs, generated original game code, APKs or ELF binaries belong in the promoted patch. Preserve selected check JSON/hashes in the evidence archive using the existing parent workflow.
