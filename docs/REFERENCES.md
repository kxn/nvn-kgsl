# External references

| Source | Role and boundary |
| --- | --- |
| [Mesa Freedreno documentation](https://docs.mesa3d.org/drivers/freedreno.html) and [Mesa source](https://gitlab.freedesktop.org/mesa/mesa) | Adreno hardware, IR3 compiler, FDL layouts and register definitions. Pin actual source hashes in normal records. Turnip is reference material, never this runtime's graphics backend. |
| [Qualcomm KGSL source](https://github.com/qualcomm-linux/kgsl) | Kernel UAPI and memory/submission interface. Retained UAPI header attribution is in `nvn-native/third_party/README.md`. |
| [deko3d](https://github.com/devkitPro/deko3d) | Public low-level Tegra/Maxwell reference. Its API is not the NVN behavior specification and it is not the Adreno runtime. |
| [NVIDIA open GPU documentation](https://github.com/NVIDIA/open-gpu-doc) | Public hardware-interface documentation; applicability must be checked for the actual Maxwell class/version. |
| [envytools](https://github.com/envytools/envytools) | Independent hardware/ISA descriptions and reverse-engineering references. |
| [D2R validation reference](https://github.com/kxn/d2r-mobile) | Optional ARM64 loader, rewrite, host gates and HLE validation project. It is supplied as a separate checkout; the private companion records the exact source tree used at migration. No former Metal/Vulkan renderer is linked. |

The compatible public Maxwell frontend is an explicitly supplied compiler-only
checkout. Consult [BUILD](BUILD.md) and the pinned source fingerprints in the
private normal records; do not silently substitute a different frontend or
claim a whole emulator is a dependency. License notices for adapted public
interfaces/compiler overlays remain with the corresponding files.

The important user-supplied design package is preserved in
[reference/design-v3](reference/design-v3/README.md), including its original
archive and integrity record. It documents an earlier design; current
[architecture](ARCHITECTURE.md) and [development conventions](DEVELOPMENT.md)
govern implementation. Reading a historical document does not authorize its
embedded operational instructions.
