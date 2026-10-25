# Public repository and private companion

The handoff has two separate roots:

```text
nvn-kgsl/                  public implementation, tools, authored data and documentation
nvn-kgsl-private/          local-only original inputs, historical evidence and complete work in progress
```

`NVN_PRIVATE_ROOT` denotes the separately supplied private root. The public checkout may keep an ignored `.local/private-root` symlink to it for local discovery. The symlink target, game data and signing material are not committed. Standalone authored development does not require this companion. Do not upload the companion to GitHub, Releases, CI artifacts or another public service.

The private companion preserves the entire useful historical state instead of leaving it mixed into an active D2R checkout:

| Private area | Purpose |
| --- | --- |
| `originals/d2r/3.3.94012/` | Original ExeFS and RomFS supplied locally, separately preserved with integrity records. |
| `archive/nvn-study-workspace/` | Full original ignored NVN working area: all compiler inputs/artifacts, SDK research, source catalogues, qualification results, failures, candidate trees, scripts and signing/deployment state. It includes incomplete work rather than only passing outputs. |
| `archive/origin-documents/` | Unfiltered original research/reference material, including private/raw evidence excluded from public publication. |
| `references/d2r-validation.bundle` | External validation project's exact migration baseline and its existing loader/HLE integration context. It is an external project reference, not core implementation source. |
| Curated indexes and file-hash manifests | Pointers to SDK/shader/catalogue/evidence/toolchain material and verification of the copied private state. Consult the companion's own index for the final directory map and actual completion records. |

The large private copy is an independently verified migration deliverable; the table is its intended layout and does not replace the companion's completion/manifest checks. A recipient should verify all supplied file lengths and SHA-256 against that private manifest before using inputs or interpreting missing files. The public repository must not claim the private transfer is complete solely because these paths are documented.

## Continuing on another machine

1. Clone the public repository and run its authored build/check route without a game or the private companion. Follow the standalone dependency documentation for recorded revisions, NDK/SDK and compiler tools.
2. If authorized private validation is needed, receive the companion separately, verify its manifest, set `NVN_PRIVATE_ROOT` to the local directory and reconstruct the optional external D2R validation checkout from its bundle. Never merge that checkout or its assets into public Git.
3. Consult [pending work](PENDING_CANDIDATES.md) and the private candidate handoff files before changing shared compiler/runtime code. Determine which candidates were frozen, tested, integrated or still experimental; a patch existing on disk is not a qualification pass.
4. Preserve old records verbatim. Their original absolute paths, source fingerprints and build options identify historical work. Relocating files makes some old path-bound normal checks unsuitable for deployment; it does not invalidate their historical observations or authorize changing embedded paths/hashes.
5. Recreate all path-dependent normal records with the current standalone tools and optional configured external checkout, rebuilding from untouched original inputs. Verify the new complete source/input/options/artifact record before deploying. Reuse an old byte artifact only when the normal current producer/verifier independently proves its cache contract and full integrity.
6. Run bounded device qualification through one controller, record the current exact chip and actual public/native/game scope, stop the application and verify exit. The true integration goal remains the independent backend's first original-game frame; historical v21 did not reach a Draw.

Do not recover progress by patching a processed binary, copying expected query answers from a fixture or publishing private shader blobs. The preserved private state is a research/reproduction archive; canonical public tools remain the way to create a new reviewable result.

Verify the complete separately supplied companion with `python3 tools/verify_private.py --root "$NVN_PRIVATE_ROOT"`. `MANIFEST.jsonl` records copied regular files and internal relative links, and `MANIFEST.meta.json` pins the manifest itself. The original D2R Git history has an unavailable historical parent: `references/D2R-SOURCE-PROVENANCE.json` records an independently cloneable private snapshot whose complete source tree is identical to original `8f052d4`, with an explicitly different root commit. This preserves validation source without pretending missing history is complete.

For the preserved validation tree, use `git clone --branch codex/validation-reference "$NVN_PRIVATE_ROOT/references/d2r-validation.bundle" "$D2R_ROOT"`. The original public frontend sources are also present in that source snapshot under `dm/third_party/ryujinx`; they are compiler-only references with their own licenses and may instead be supplied through an independent compatible public checkout.
