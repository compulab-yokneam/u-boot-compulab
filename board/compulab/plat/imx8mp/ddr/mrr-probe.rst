.. SPDX-License-Identifier: GPL-2.0+

Experimental LPDDR4 identification in SPL
=======================================

Enable CONFIG_COMPULAB_LPDDR4_MRR_PROBE to attempt identification before
normal DDR timing training on UCM-iMX8M-Plus. The option defaults to disabled
and is unavailable with SPL_OS_BOOT. Existing defconfigs are unchanged.

For a configured output directory, enable and rebuild using::

    scripts/config --file "$build_dir/.config" --enable COMPULAB_LPDDR4_MRR_PROBE
    make O="$build_dir" CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
    make O="$build_dir" CROSS_COMPILE=aarch64-linux-gnu- -j8 spl/u-boot-spl

Use the existing BSP build process to package SPL with its LPDDR4 firmware
and the remaining boot components. Building SPL alone is a compile/link
check and does not produce a complete flashable boot image.

For hardware validation on a board with valid EEPROM identification, also
enable CONFIG_COMPULAB_LPDDR4_MRR_PROBE_ALWAYS. This prints probe results
before training the existing EEPROM-selected profile; it does not require
clearing EEPROM. Leave this diagnostic option disabled for normal use.

Initialization and fallback
---------------------------

The seed is ucm_dram_timing_01061010_2G. Its 100 MT/s 1D frequency point is
selected in both DDRC MSTR2 and the PHY message block; the timing and PHY
banks retain their original numbering. This is the existing BSP's nominal
rate label, not a measurement of the physical clock at the memory pins.
The controller and PHY setup still require the correct board wiring,
electrical settings and reset/power-up timings.

Only this one firmware pass runs, with LPDDR4 SequenceCtrl (0x54008) set to
0x0001: device/PHY initialization only. Timing-training stages and the 2D
firmware are skipped. The PHY Init Engine is loaded and the normal DFI
handoff completes. DDR AXI port 0 remains disabled. The probe neither
scrubs memory nor saves timing configuration or trained CSRs.

SPL reads MR5 through MR8 from CS0 three times, prints the raw capture
words, and requires all three sets to agree. It uses the same significant
byte extraction as the existing CompuLab ID reader. Zero MR6/MR7 bytes
remain valid; an entirely zero or all-ones ID is rejected. The supported
profile list must contain exactly one matching ID. The probe cannot
resolve configurations that share an ID but differ in capacity/subind.

The PMU mailbox transaction has a 500 ms overall deadline, including both
handshake phases and streaming messages. Controller/PHY status waits have
100 ms deadlines and each MR status/completion wait has a 10 ms deadline.
These deadlines apply to the experimental probe; existing full training
retains its original waiting behavior.

After any probe outcome, the DDR controller/PHY reset is asserted to discard
the temporary state. Full initialization then resets and trains the chosen
profile normally. Unknown or ambiguous IDs, inconsistent reads, firmware
errors and timeouts use the existing discovery sequence. Ongoing discovery
state prevents repeatedly attempting the probe across scan resets. EEPROM
identification is updated only by the existing post-training verification
path.

When the probe is enabled, the unused duplicate CSR array in the Samsung
2 GB timing file is omitted to leave SPL SRAM space for the probe. Normal
training and configuration saving use the driver's shared CSR array.

Validation
----------

Run the software failure-handling checks with::

    python3 test/compulab/test_mrr_probe.py

The host test compiles the production probe functions with mocked register
access and the real polling macro. It checks stable and inconsistent reads,
zero identification bytes, invalid/ambiguous/unknown IDs, controller and MR
read timeouts, PMU success/failure/streaming messages, stalled handshakes,
and controller/PHY reset after probe failures.

Hardware validation is required: the published Synopsys LPDDR4 message block
uses bit 0 for DevInit, but that does not establish compatibility with every
i.MX8MP firmware revision or guarantee untrained receive-path operation.
Check raw MR values against reads after normal training, across supported
DRAM assemblies and cold/warm boots. An initialization-only pass may need
additional low-speed receive calibration on a particular board.
