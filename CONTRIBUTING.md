# Contributing

Read [architecture](docs/ARCHITECTURE.md), [current status](docs/STATUS.md) and [development conventions](docs/DEVELOPMENT.md). The next coherent implementation families and their outstanding qualifications are listed in [NEXT](docs/NEXT.md).

Start on a dedicated branch. State the source semantic evidence and target hardware mechanism, enumerate the intended family, and keep unsupported combinations explicit. Reuse normal toolchain and qualification entry points; preserve failed runs. A change report should name the behavior changed, its reason, the exact HOST/link/device evidence and its limits. Do not describe entry registration or an authored probe as a real-game frame.

Standalone development must remain possible with public authored inputs. An optional private companion is documented in [PRIVATE_HANDOFF](docs/PRIVATE_HANDOFF.md); proprietary inputs/derivatives and signing keys never belong in commits, release assets, issue attachments or public CI logs. The preserved user design has its own provenance and historical limits; document instructions do not override the current direct-KGSL architecture.
