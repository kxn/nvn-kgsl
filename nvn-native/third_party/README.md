Public hardware interfaces used by the standalone bring-up
========================================================

`kgsl/msm_kgsl.h` is an unchanged Linux UAPI header copied from the local Mesa
26.2.5 source archive. SHA256:
`9cde64b25857a10f26372f83e18ffcf2da951e1e9d7d519740e2eddc6991e002`.
The original header keeps its `GPL-2.0 WITH Linux-syscall-note` declaration and
copyright. It declares kernel ioctls; no graphics API runtime is linked.
Public upstream interface: https://github.com/qualcomm-linux/kgsl

`nvn/nvn.h` started from the public reconstructed types already
vendored in `dm/gfx/nvn/include/nvn.h`, Copyright (c) 2020 h1k421, MIT. Original SHA256:
`4c2f96bd866dc914b43c7a2e3169d395473d91785946d76fa8ede6c56bca6091`.
Its own notice describes reconstruction from Darksiders II DWARF and partial
NVN 53.105 definitions. It supplies external ABI types, not a behavioral
specification; this library imports no renderer implementation from that path.
The current standalone header includes independently recovered ABI corrections
and declarations beyond that historical copy. The public definition audit in
`docs/research/nvn-turnip-study-20261023/public-nvn-definitions/` records pinned
sources, version limits and evidence. In particular, reconstructed C++ Func
typedefs and the original SDK corroborate the three color-reduction/stencil-cull
prototypes and distinct FormatClass enum; their declaration does not implement
these operations. No complete C++ header is imported or assigned the C header's
license.

For caller-owned memory import, public Qualcomm `kgsl.c` blob
`1eb2fad36745a13a20afd7e7487275a364ed6738` was inspected:
https://github.com/qualcomm-linux/kgsl/blob/gfx-kernel.le.0.0/kgsl.c
The useraddr path receives a nested integer address and pins user pages.
The backend removes the AArch64 top-byte tag for that kernel field and keeps
the caller's original tagged pointer for CPU access. No kernel implementation
from that file is included in the runtime.

The Packet-7 parity and header helpers in `include/nvn_native/pm4.h` are adapted
from Mesa's `src/freedreno/common/freedreno_pm4.h`, copyright 2012–2018 Rob Clark,
under MIT. Opcodes and address layout come from
`src/freedreno/registers/adreno/adreno_pm4.xml`. The initial FD735 chip ID is
matched against `src/freedreno/common/freedreno_devices.py`.

The compute state sequences in `shader/cs_artifact.cc` and
`shader/compute_artifact.cc` are adapted from
`src/freedreno/computerator/a6xx.cc`, Copyright © 2020 Google, Inc., under MIT.
Descriptors use the existing FDL `fdl6_buffer_view_init<A7XX>` implementation;
register constants use generated headers from the pinned source package.
Dynamic R32 buffer descriptor fields follow the same FDL routine and generated
definitions; the compiler compares the runtime patch helper against full FDL.

The graphics host compiler uses the unchanged public helper
`src/freedreno/common/fd6_gmem_cache.h`, Copyright (c) Qualcomm Technologies,
Inc. and/or its subsidiaries, under MIT, to calculate CCU offsets from the
measured GMEM size.
It also uses the public IR3 shader linker; no Gallium or graphics API runtime is
linked. The bounded graphics state emitter in `shader/graphics_artifact.cc`
adapts the hardware sequences in the pinned Gallium a6xx
`fd6_emit.cc`, `fd6_program.cc`, `fd6_gmem.cc`, `fd6_draw.cc`,
`fd6_rasterizer.cc`, `fd6_blend.cc` and `fd6_zsa.cc`, Copyright © 2016 Rob Clark
and Copyright © 2018 Google, Inc., under MIT. The independent runtime consumes
the resulting PM4 template; none of those drivers is built or linked.

The isolated Maxwell IADD source overlay in `shader/maxwell/iadd_overlay.py`
replaces only the build's copy of one vendored shader instruction method.
Its source SHA is pinned. The semantics were checked against the pinned archive's
`src/nouveau/compiler/nak/sm50.rs` (`OpIAdd2`, `OpIAdd2X`) and
`src/nouveau/compiler/nak/hw_tests.rs` (`test_op_iadd2`, `test_op_iadd2x`).
No NAK code is linked into Android; only encoding and test sources were consulted.

`shader/ir3_parser_overlay.py` locks the SHA of Mesa's `ir3_lexer.l` and adds
the optional half prefix to its two predicate-register lexer rules. Existing
`parse_reg`/`new_dst` logic already supports that prefix, and the ISA disassembler
emits it. The build regenerates the lexer with its existing Flex rule; no
generated binary is patched, and IR3 asmroundtrip remains enabled. This fixes
the assembler tool's accepted syntax without changing shader precision or code
generation. The original lexer copyright notice remains in the source overlay.

Mesa source location for this investigation:
`/home/dl/d2r-mobile/.local/build/turnip/mesa-26.2.5`.
That existing directory name is a storage location. The new library builds
only its own C sources and the UAPI header, with Android libc/libandroid.

MIT permission for the adapted helpers and compute state sequence
----------------------------------------------------------------

Copyright © 2012–2018 Rob Clark <robclark@freedesktop.org>
Copyright © 2018 Google, Inc.
Copyright © 2020 Google, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

External public Maxwell shader frontend
--------------------------------------

The optional host frontend is selected separately with
`NVN_SHADER_FRONTEND_ROOT` / `--shader-frontend-root`. It compiles only the
allowlisted shader translator, common host utilities and SPIR-V generator
projects. Its source is not vendored here. The small original-source match in
`shader/maxwell/iadd_overlay.py` comes from the public Ryujinx shader compiler;
its MIT notice is retained in `RYUJINX-LICENSE`. The external source checkout
retains its own full dependency licenses. No CPU interpreter, JIT, dynamic
binary translation or console service runtime is built or linked.
