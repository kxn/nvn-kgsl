# Public and private material

This repository publishes implementation source, newly written explanations, permitted upstream notices/references, and independently authored qualification data. D2R is an optional external project; purchasing or possessing a game does not authorize uploading its executable, shaders or art into this repository.

| Material | Public handling |
| --- | --- |
| Original implementation and authored shaders/test inputs | Include after source/license/provenance and secret checks. |
| Mesa/FDL/IR3, deko3d, public hardware/API reconstruction references | Pin upstream URL/revision/license; preserve notices for any actual copied code. Reference access does not make a different license apply. |
| Research conclusions and implementation plans | Re-express verified facts and limits in original prose. Do not blindly copy SDK-derived disassembly, raw tables or logs. |
| Authored full GPU readbacks | Eligible only when the complete producer/input provenance is public and authored/licensed; inspect pixels and archive members. A generic filename is insufficient. |
| Game executables, ExeFS/NSO/RomFS/CASC, screenshots, extracted resources, saves | Exclude, including ZIP/GZ/nested archives, Git history and releases. |
| Game shader code/control/C1; translated SPIR-V/NIR/IR3, generated native headers/blobs | Exclude as game-derived content. Public tool source may recreate artifacts from independently supplied lawful local inputs. |
| Original SDK disassembly, code fragments and private static-inspection dumps | Exclude. Publish only necessary newly authored findings with clear version/evidence limits. |
| Raw run/build logs and normal-record bundles | Do not bulk migrate. They may contain source bytes, shader hex, resource paths, commands, private filenames and device/network identifiers. Extract a strict allowlist of facts instead. |
| User-supplied design ZIP and extracted documents/tables/model | Preserve the inspected original archive and all 67 text/reference members at the user’s explicit request, keeping provenance and preexisting rights. It contains no discovered game payload or SDK dump; historical architecture is clearly superseded. |
| Keystores, signing keys, tokens, device private IP/serial/UID, private absolute paths | Exclude. Use configurable selectors and path placeholders in instructions. |

Historical hashes are non-content references. A hash and locally observed result are not a public reproduction bundle, nor proof that an input was licensed for redistribution. Do not alter a record's embedded source hashes to make historical evidence appear current. Keep the original private record, export an explicitly labeled selected summary, and record that transformation.

Public evidence must say whether GPU code was authored or derived from a private workload, whether it ran through raw-native or public NVN entry points, whether original game CPU modules executed, and whether the claim is HOST/link/device/presentation/game-frame scope. If any underlying private input is required, say so directly and provide a public authored route for ordinary development.

Excluded material is preserved, not discarded: the separate local-only `nvn-kgsl-private/` companion contains original inputs, the complete ignored NVN workspace, original documents, external D2R bundle and integrity indexes. See [PRIVATE_HANDOFF](PRIVATE_HANDOFF.md). Public history has selected facts and content hashes; it does not expose that companion or depend on its private paths. Relocated normal records remain historical until rebuilt and verified through the current canonical tools.
