.. SPDX-License-Identifier: GPL-2.0+

Compact CompuLab i.MX8MP LPDDR4 timings
=====================================

The files under ``raw/`` are editable DDR-tool outputs. Normalize only local
array linkage and the exported descriptor name as described below; preserve
the generated timing data. These inputs are not compiled. ``pack.py``
generates the top-level timing C files
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

Migrating an existing expanded-timing tree
-----------------------------------------

This procedure starts with a working CompuLab i.MX8MP U-Boot tree whose
top-level ``lpddr4_timing_*.c`` files contain the complete register arrays
and exported ``struct dram_timing_info`` descriptors. It migrates the whole
DDR implementation, including existing board-specific profiles. Adding one
profile after that migration is covered in `Regenerating or adding timings`_.

Run commands from the source root in the same shell. You need Git, Python 3,
a host C compiler supporting ASan/UBSan, and your working AArch64 build
toolchain. The optional ELF comparison also needs pyelftools.

1. Identify the starting point::

       git status --short
       git log -5 --oneline
       ddr_dir=board/compulab/plat/imx8mp/ddr
       rg -n 'struct dram_(timing_info|compact_timing)' "$ddr_dir"/lpddr4_timing_*.c

   Expanded inputs declare ``struct dram_timing_info``. Compact outputs
   declare ``const struct dram_compact_timing`` and reference the shared
   pool. If the tree already has compact outputs, ``raw/``, ``pack.py`` and
   the compact driver, skip this migration and use the next section.
   The current development branch at ``609067d994d`` is already migrated.

   Commit or otherwise preserve local work before continuing. The reference
   worktree below captures committed files only. Start with a clean tree.
   If an older implementation shares arrays through another C file or
   header, first recover self-contained DDR-tool outputs for each profile;
   ``pack.py`` does not resolve includes, macros or external array references.

2. Preserve a reference before changing the timing files::

       migration_dir=$(mktemp -d /tmp/uboot-ddr-migrate.XXXXXX)
       git worktree add --detach "$migration_dir/reference" HEAD
       git switch -c migrate-compact-timings
       mkdir "$migration_dir/expanded"
       cp "$ddr_dir"/lpddr4_timing_*.c "$migration_dir/expanded/"

   Keep ``migration_dir`` and its reference worktree until validation is
   complete. This copy must contain the expanded arrays, not generated
   compact descriptors. Record the original revision, board configuration,
   compiler, PHY firmware and packaging inputs. Retain a known-working full
   boot image and its boot log.

   Where the original SPL fits, build a reference with your usual board and
   capacity configuration. For example, for SBEV D2/D4::

       make -C "$migration_dir/reference" O="$migration_dir/build-original" \
           CROSS_COMPILE=aarch64-linux-gnu- \
           ucm-imx8m-plus-sbev_defconfig d2d4.config
       make -C "$migration_dir/reference" O="$migration_dir/build-original" \
           CROSS_COMPILE=aarch64-linux-gnu- -j8 spl/u-boot-spl
       aarch64-linux-gnu-size "$migration_dir/build-original/spl/u-boot-spl"

   If the expanded build exceeds the SPL limit, the source copy still
   provides a reference. See the comparison-only ELF instructions below
   if an original linked ELF is also needed.

3. Integrate the implementation before converting additional profiles.
   The series in this repository is ordered as follows:

   * ``445b7a1e1be``: MR identification probe, equivalent to the earlier
     ``f913c214dcd``.
   * ``9c661795f4b``: rank-aware identification, equivalent to
     ``c3eeaf191da``.
   * ``bfc651a48bb``: compact pool, generator, decoder, driver integration,
     migrated profiles and tests.
   * ``609067d994d``: discovery-index and verified EEPROM persistence fixes.

   Check which changes are already present, including equivalent commits
   with different hashes. For a compatible base with none of these changes,
   and with these commit objects available locally, apply them in order::

       git cherry-pick 445b7a1e1be
       git cherry-pick 9c661795f4b
       git cherry-pick bfc651a48bb
       git cherry-pick 609067d994d

   Apply only the missing changes. A tree already containing the original
   two probe commits starts with the compact commit. Resolve each conflict
   before continuing; preserve local chip entries, discovery order and
   timing values. Inspect ``git show --stat bfc651a48bb`` for the full scope.

   Copying just ``pack.py`` and regenerating the C files is insufficient.
   The migration also installs ``include/imx8m_ddr_compact.h``, the streaming
   decoder, DDRC/PHY initialization and saved-timing support, the pool object
   in the DDR Makefile, and the platform Kconfig selection. Board descriptors
   and their pointers use ``const struct dram_compact_timing``;
   initialization calls ``ddr_init_compact()`` and optional probing calls
   ``ddr_init_mrr_compact()``. Keep the existing expanded saved-timing ABI
   used by suspend/resume. These changes are included in the compact commit.

4. Reconcile every original profile with ``raw/`` before generation.
   The series includes its own 16 inputs; it cannot include later local
   changes or additional chips from another branch::

       diff -ru "$migration_dir/expanded" "$ddr_dir/raw"

   Differences are expected where array linkage was normalized. Review
   register values, order, messages, counts, rates and exported symbols.
   For each locally changed or additional expanded profile, copy the saved
   source into ``raw/`` with the same filename. For example::

       profile=lpddr4_timing_newchip_4G.c
       cp "$migration_dir/expanded/$profile" "$ddr_dir/raw/$profile"

   Use the declaration-normalization command in step 2 of the next section,
   with ``ddr_chip`` matching that profile's intended exported symbol.
   Preserve existing symbol names for existing profiles, including cases
   where the filename and symbol suffix differ. Do not copy compact outputs
   into ``raw/``. Keep the trained-CSR input array as well.

   For additional profiles, retain/add their top-level Makefile objects,
   convert their SPL extern declarations to
   ``extern const struct dram_compact_timing``, and retain/add their identity
   entries pointing at those symbols. Convert any local timing-pointer
   declarations too. Preserve ID/subind pairs, capacity guards and Falcon
   entries. Follow steps 4 and 5 of the next section for registration.
   If deliberately removing a profile, remove its raw input, generated
   output, Makefile object, extern and references together; the generator
   does not delete stale output files.

5. Generate and validate the complete reconciled profile set::

       python3 "$ddr_dir/pack.py"
       python3 "$ddr_dir/pack.py" --check
       python3 test/compulab/test_ddr_compact.py
       python3 test/compulab/test_ddr_compact_phy.py
       python3 test/compulab/test_mrr_probe.py
       python3 test/compulab/test_ddr_discovery.py
       git diff --check
       git status --short

   Check that every intended input produced its top-level output and that
   no expanded top-level profile is still compiled. Generation checks all
   inputs, including profiles disabled by the current capacity fragment.
   Decoder tests compare against ``raw/``; the source reconciliation above
   ensures those inputs still represent the previously working timings.

6. Build into a fresh output directory, using the same compiler and board
   settings as the reference::

       build_dir="$migration_dir/build-compact-d2d4"
       make O="$build_dir" CROSS_COMPILE=aarch64-linux-gnu- \
           ucm-imx8m-plus-sbev_defconfig d2d4.config
       rg '^CONFIG_IMX8M_DDR_COMPACT=y$' "$build_dir/.config"
       make O="$build_dir" CROSS_COMPILE=aarch64-linux-gnu- \
           -j8 spl/u-boot-spl
       aarch64-linux-gnu-size "$build_dir/spl/u-boot-spl"
       wc -c "$build_dir/spl/u-boot-spl-nodtb.bin"

   ``IMX8M_DDR_COMPACT`` is selected by the CompuLab platform; no timing-format
   menu option needs enabling. Repeat with separate output directories for
   D1/D8, other supported boards, and Falcon where applicable. Preserve the
   previous MR-probe setting when comparing sizes, and test probe-enabled
   builds if that feature is deployed. Compare like-for-like ELF sizes and
   SPL binaries, including the linker limit and appended device-tree size.

   When a reference ELF is available, compare its linked tables as well::

       python3 test/compulab/test_ddr_compact.py \
           --reference-elf "$migration_dir/build-original/spl/u-boot-spl" \
           --compact-elf "$build_dir/spl/u-boot-spl"

   Only linked profiles can be compared through an ELF. The host decoder
   check covers all raw profiles. A reference and compact build using the
   same capacity selection make the linked comparison easiest to assess.

7. Package the full boot image using the existing BSP process and matching
   PHY firmware, BL31 and OP-TEE. Validate each supported assembly: selected
   timing, MR identity, capacity, memory integrity, cold/warm boot, Linux
   boot and suspend/resume. Exercise both saved-EEPROM selection and fresh
   discovery using the established EEPROM service procedure. Confirm that
   discovery advances through the list and persists the final selection.
   If used, test MR probing and its normal-discovery fallback too.

   For Yocto, ensure the recipe builds this migrated source revision, then
   rebuild through the normal boot-image recipe and verify its deployed
   image. A standalone SPL build does not update Yocto's ``flash.bin``.

8. Review and commit raw inputs, generated profiles and shared pool,
   registration changes and any integration adjustments together. Include
   original/migrated revisions, configurations, measured sizes and hardware
   results in the commit description. Keep the reference until the complete
   boot image has passed the required board checks.

Regenerating or adding timings
------------------------------

Run all commands from the U-Boot source root. The example imports a new
4 GiB profile; replace ``newchip_4G`` with a unique C-identifier suffix.

1. Set the paths and copy the DDR-tool output into ``raw/``::

       ddr_dir=board/compulab/plat/imx8mp/ddr
       ddr_chip=newchip_4G
       ddr_timing_file="${ddr_dir}/raw/lpddr4_timing_${ddr_chip}.c"
       cp /path/to/generated/lpddr4_timing.c "$ddr_timing_file"

   Keep an untouched generator output outside the source tree. When updating
   an existing chip, edit its existing raw input instead. Preserve register
   order, frequencies, firmware types and board-specific settings.

2. Make configuration arrays and the FSP message array local, and give the
   final timing descriptor a unique exported symbol. For the standard
   single-line declarations emitted by the DDR tool, run::

       sed -i -E \
           -e 's/^([[:space:]]*)(static[[:space:]]+)?struct[[:space:]]+(dram_cfg_param|dram_fsp_msg)([[:space:]]+)/\1static struct \3\4/' \
           -e "s/^([[:space:]]*)(static[[:space:]]+)?struct[[:space:]]+dram_timing_info[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*=/\1struct dram_timing_info ucm_dram_timing_${ddr_chip} =/" \
           "$ddr_timing_file"

   These replacements also accept already-static declarations, so rerunning
   them does not add another ``static``. Do not replace every occurrence of
   ``struct``: that also changes descriptor declarations and can duplicate
   qualifiers. The final raw descriptor must have this form::

       struct dram_timing_info ucm_dram_timing_newchip_4G = {
           /* Original generated fields */
       };

   The identifier comes before ``=``; do not produce
   ``struct dram_timing_info = ucm_dram_timing_newchip_4G``. Inspect the
   declarations after normalization. If a generator uses different syntax
   such as multiline or macro-based declarations, adjust those declarations
   explicitly rather than broadening the replacements over the whole file.

   The raw descriptor retains its original type. Keep the generated
   trained-CSR array in the raw input; the packer omits its unused local copy
   from compiled storage. Existing literal arrays, ARRAY_SIZE counts and
   conventional FSP message initializers are supported. Unsupported forms
   are rejected for review.

3. Regenerate all profiles and the shared pool::

       python3 "$ddr_dir/pack.py"
       python3 "$ddr_dir/pack.py" --check

   This creates ``ddr/lpddr4_timing_newchip_4G.c`` and updates the shared
   compact pool and header. Do not edit these generated files manually.
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

6. Run the production-decoder, probe and discovery checks::

       python3 test/compulab/test_ddr_compact.py
       python3 test/compulab/test_ddr_compact_phy.py
       python3 test/compulab/test_mrr_probe.py
       python3 test/compulab/test_ddr_discovery.py
       git diff --check

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

Discovery state and EEPROM verification
--------------------------------------

The SPL EEPROM write helper waits 10 ms after a successful transaction,
matching the ordinary EEPROM driver's write-cycle allowance. This covers
both discovery-state records and the final ID/subindex writes. The ID getter
reads the physical device so readback verification cannot accept a cached
value. Discovery is marked valid only after successful writes and verified
ID/subindex values. A failed commit returns an error and stops this boot;
it must not reset with a valid state and restart the same scan indefinitely.

The persisted discovery index already advances before training. A reset
while training therefore resumes at that index without incrementing again.
The following host test compiles the production discovery and EEPROM
functions against an EEPROM model with busy cycles, failed writes and
write protection::

    python3 test/compulab/test_ddr_discovery.py

It checks mismatch-to-match discovery across resets, verified persistence,
physical readback, failed commits, training resets and complete exhaustion
of the scan list. Hardware EEPROM accessibility and write protection still
need checking if a real board reports a commit failure.
