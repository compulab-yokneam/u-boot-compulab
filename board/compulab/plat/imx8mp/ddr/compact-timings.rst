.. SPDX-License-Identifier: GPL-2.0+

Compact CompuLab i.MX8MP LPDDR4 timings
=====================================

The files under ``raw/`` are the unmodified, editable DDR-tool outputs.
They are not compiled. ``pack.py`` generates the top-level timing C files
and ``lpddr4_compact_tables.[ch]``. Keep raw inputs and generated outputs in
the same change. Normal builds use the checked-in outputs and need no host
Python packages or generation step.

Storage and execution
---------------------

Identical arrays share one compact descriptor. Each table family has a
packed base, selected to minimize the combined payload of its variants.
Variants either contain packed literal entries or a copy/literal stream,
whichever is smaller. The generator uses deterministic matching with
SequenceMatcher's autojunk heuristic disabled. This is a size optimization,
not a claim of mathematically optimal compression.

Packed entries are little-endian bytes with no alignment requirement:

* DDRC: 16-bit offset from 0x3d400000, followed by a 32-bit value (6 bytes).
* PHY, firmware messages and PIE: 24-bit register, 16-bit value (5 bytes).

For an encoded stream, a 16-bit command has the following meaning:

* Bit 15 set: bits 14:0 give the number of literal packed entries following.
* Bit 15 clear: bits 14:0 give a copy count; a following 16-bit entry offset
  selects a contiguous range of the packed base table.

Counts must be nonzero and may not exceed the remaining output count.
Descriptor sizes bound input reads and base counts bound copy ranges.
Completion requires exactly the declared count and input byte length.
The decoder reads individual bytes; it never casts packed data to aligned
integer pointers. Values outside the encoding ranges fail generation;
introduce an explicitly supported wider format if a future part needs one.

Each iterator emits one ordinary register/value pair at a time. The DDRC
and PHY writers issue those pairs in exactly the original order, including
all repeated writes and APB-ownership transitions. There is no full-profile
unpacking buffer, and no post-write patch phase. Runtime descriptor
adaptation uses only the small FSP metadata array, not register tables.

The source ``struct dram_timing_info`` format is not repurposed. SPL uses
``struct dram_compact_timing`` with its own table and FSP descriptors. Once
DDR is initialized, ``ddr_compact_save()`` publishes an ordinary expanded
``struct dram_timing_info`` followed by DDRC, PHY, trained CSR and PIE arrays
at the existing saved-timing address. Frequency values and physical pointers
use the existing layout. Firmware-message fields, unused by the existing
saved-timing path, are explicitly zeroed. This preserves the resume format.

The generated per-profile trained-CSR arrays are omitted from compact
storage: this driver captures and saves through the single mutable list in
``drivers/ddr/imx/phy/ddrphy_csr.c``, which remains unchanged. Generated source
copyright/license headers are preserved; the combined pool retains both
GPL-2.0+ and BSD-3-Clause provenance.

The optional COMPULAB_LPDDR4_MRR_PROBE path also consumes compact tables.
It keeps the original FSP numbering, executes just the selected 100 MT/s 1D
message with DevInit-only SequenceCtrl and both ranks temporarily enabled,
and bounds all probe waits. Probe mode returns before AXI access, ECC
scrubbing or configuration saving, and never captures trained CSRs. The
controller/PHY is reset before normal training. See ``mrr-probe.rst`` and
run ``python3 test/compulab/test_mrr_probe.py`` for rank/failure checks.
``python3 test/compulab/test_ddr_compact_phy.py`` compares the production
expanded and compact PHY execution traces in normal and probe modes,
including the selected FSP, rank overrides, skipped CSR capture and errors.

The compact path is selected by TARGET_COMPULAB_IMX8MP through the internal
IMX8M_DDR_COMPACT option. Other platforms retain their existing timing types
and initialization entry points. DDR training firmware, stage ordering,
controller handshakes, EEPROM identification and discovery order are not
changed by compression. Falcon/SPL_OS_BOOT selects from its existing list.

Regenerating or adding timings
------------------------------

Run all commands from the U-Boot source root.

1. Update the appropriate original file under
   ``board/compulab/plat/imx8mp/ddr/raw/``. For a new part, place the DDR-tool
   output there as ``lpddr4_timing_newchip_4G.c``. Keep its exact register
   order, frequencies, firmware types and board-specific settings.

2. Give its exported ``struct dram_timing_info`` a unique symbol, for example
   ``ucm_dram_timing_newchip_4G``. The raw descriptor stays in the original
   generator format; the generated top-level descriptor will be compact.
   Existing literal arrays, ARRAY_SIZE counts and conventional FSP message
   initializers are supported. Unsupported forms are rejected for review.

3. Regenerate all profiles and the shared pool::

       python3 board/compulab/plat/imx8mp/ddr/pack.py
       python3 board/compulab/plat/imx8mp/ddr/pack.py --check

   Do not edit the generated pool or top-level profile files manually.
   ``--source-dir`` and ``--output-dir`` allow comparison against a separate
   set of raw outputs. The output directory must already exist.

4. For a new part, add its top-level object to the DDR Makefile::

       obj-$(CONFIG_SPL_BUILD) += lpddr4_timing_newchip_4G.o

   Declare its generated descriptor inside the SPL block of ``ddr.h``::

       extern const struct dram_compact_timing ucm_dram_timing_newchip_4G;

5. Add the board identity to ``lpddr4_array`` under the appropriate capacity
   guard (DRAM_D4 for a 4 GiB assembly). Set the measured MR5-MR8 ID, vendor,
   total capacity in MiB and assembly metadata, and point ``.timing`` at the
   new descriptor. Preserve existing discovery order and EEPROM ID/subind
   compatibility. Two profiles with the same ID/subind are not uniquely
   distinguishable through the existing EEPROM lookup. Falcon has its own
   list; add support there only when needed and validated.

6. Run the production-decoder checks::

       python3 test/compulab/test_ddr_compact.py

   This compiles every generated profile and the production decoder using
   the host C compiler with ASan/UBSan. It checks every ordered write against
   the raw inputs, copies actual driver CSR entries into the saved layout,
   checks saved pointers, counts and untouched trailing bytes, and rejects
   malformed streams. No board access is simulated as hardware validation.

7. Build affected SPL configurations and inspect their size::

       build_dir=/tmp/uboot-compact-check
       make O="$build_dir" CROSS_COMPILE=aarch64-linux-gnu- \
           ucm-imx8m-plus_defconfig d2d4.config
       make O="$build_dir" CROSS_COMPILE=aarch64-linux-gnu- \
           -j8 spl/u-boot-spl
       aarch64-linux-gnu-size "$build_dir/spl/u-boot-spl"

   Include D1/D8, D2/D4 and Falcon where applicable. For extra validation,
   compare the actual linked original and compact ELF tables::

       python3 test/compulab/test_ddr_compact.py \
           --reference-elf /path/to/original/spl/u-boot-spl \
           --compact-elf "$build_dir/spl/u-boot-spl"

   These optional ELF checks require pyelftools. Multiple ``--compact-elf``
   arguments are supported. The reference must use the original expanded
   descriptor type. An oversized original build may need a larger temporary
   SPL_MAX_SIZE and SPL_PAD_TO solely to obtain an ELF for comparison; never
   deploy that reference as a production image.

8. Package a full boot image through the normal BSP flow, retaining the
   matching PHY firmware and other boot components. SPL alone is not a
   flashable image. Validate normal training, memory integrity, cold/warm
   boot, discovery/EEPROM behavior and suspend/resume on supported hardware.

The initial complete 16-file pool contains 9,886 packed payload bytes,
compared with 221,240 bytes of original per-profile arrays. This excludes
descriptors, code, the retained driver CSR list and linker selection effects.
Measure the actual SPL image for each enabled capacity configuration.

For the initial all-capacity UCM build with aarch64-linux-gnu, the SPL
binary without its appended device tree decreased from 265,768 to 124,048
bytes (141,720 bytes saved). The original reference needed a larger
comparison-only linker limit; the compact build uses the normal 0x2c000
limit. D1/D8, D2/D4 and Falcon builds also pass. These are software/build
results; board boot and suspend/resume still require hardware validation.

With the imported probe changes integrated, the all-capacity probe-enabled
SPL without its appended device tree is 127,296 bytes with the validation
toolchain. D1/D8 with probing is 123,488 bytes, D2/D4 without probing is
121,312 bytes and Falcon is 150,168 bytes. These remain below the normal
0x2c000 limit. The initial compact-size comparison above predates probe
integration and is retained as a record of that separate measurement.
