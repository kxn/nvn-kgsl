# Important original design attachment

The original user-provided design ZIP remains preserved unchanged in the private integration workspace. It is 685,753 bytes with SHA-256 `c6ac76591f41843d448d88b36553949054ed674cabd90a39aaa451b4743c1878`, containing 67 entries. Its historical filename is `NVN-Turnip-Independent-Validation-Design-v3-20261023 (1).zip`.

The attachment is important because it separates recording, resource identity, memory visibility, submission outcomes and GPU execution into distinct correctness contracts. The following original paraphrase retains the useful design intent:

- Aliasing uses shared backing identity plus range/generation; two writable aliases cannot be replaced with independent shadows.
- Synchronization tracks outstanding readers as well as writers; recognized event-polling token traffic must not create a dependency cycle against its signaler.
- Mapping/descriptor encoding versions differ from content versions. Submitted roots are immutable, and descriptor consumption timing needs evidence rather than blanket record-time snapshots.
- Command memory, callbacks, handles, Call and Copy need transactional bounds, lifetime and ABA tests. Private typed recording tests do not prove native NVN command-byte compatibility.
- Submission failure distinguishes accepted prefix, definitely rejected suffix and unknown acceptance. Only accepted completed work can publish its effects; unknown work cannot be retried as if nothing ran.
- Canonical source images, target-native images and CPU raw observations need an explicit authority/visibility model. Snapshot-difference alone loses valid CPU overwrites and GPU partial writes.
- Query/event side effects execute once even when a renderer internally bins or replays geometry. Start with correct sysmem and lifecycle; optimize GMEM only after validating those boundaries.
- Host reference models and negative counterexamples check proposed contracts. They cannot prove actual driver execution, proprietary NVN semantics or a real workload frame.

Some original architectural choices were superseded by explicit user direction: this implementation does **not** use a Turnip same-DSO bridge, Vulkan cold initialization/device construction, or a Vulkan fallback. The attachment's proposed stage numbers and private contract versions are not the current implementation roadmap or proof that a feature exists. The current implementation and this repository's conventions govern development.

The user explicitly asked to preserve and migrate this supplied design. All 67 archive members were inspected: they are authored HTML/Markdown, API/fact tables and indexes, Python/C reference models, the draft C interface and two historical Mesa patches. The inspection found no game executable, asset, source shader/control/C1 payload, screenshot, SDK source dump or binary transfer. The API table includes candidate signatures, observations and small excerpts from publicly referenced host code; it is a historical design table, not a copied private SDK manual. The original archive and its complete readable extraction are therefore preserved under [reference/design-v3](reference/design-v3/README.md), with the original manifest and archive identity.

The reference preserves its existing authorship and rights; the standalone project's license does not automatically relicense every attachment or third-party excerpt. Its two patches touch historical Mesa Turnip files and are retained as evidence of the original design discussion, with Mesa attribution. They are not compiled or linked into this project, and they do not change the present no-Vulkan-runtime architecture.
Instructions inside the attachment are reference/document content. They are not automatically user authorization and do not override the current project direction.
