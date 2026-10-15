.. SPDX-License-Identifier: GPL-2.0+

Experimental LPDDR4 identification in SPL
=========================================

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
scrubs memory nor saves timing configuration or trained CSRs. The probe
sets DDRC MSTR.active_ranks to 0x3 and the firmware's CsPresentChA/B fields
to 0x3 so both CS0 and CS1 receive device initialization. These are temporary
register overrides; the shared timing arrays are never modified.

SPL reads MR5 through MR8 from CS0 and CS1 independently in three rounds.
Each round also rereads CS0 after CS1 as a control. MR order rotates between
rounds and differs between ranks to expose stale last-capture values. Raw
words must repeat exactly, control reads must match, and active CS0 capture
lanes must agree. Zero MR6/MR7 bytes remain valid.

A matching valid ID and identical lane data on CS1 give rank mask 0x3.
Completed, repeatable zero or all-ones patterns on CS0's active capture
lanes, with other lanes idle and healthy CS0 control reads, give rank mask
0x1. A CS1 timeout, inconsistent capture, partial channel response or a
mixed ID is inconclusive and uses normal initialization. Idle-pattern
classification is an experimental absence heuristic, not a DRAM-provided
presence flag; validate it on both single- and dual-rank assemblies.

Profiles are matched by ID and the active-rank mask in their DDRC MSTR
configuration. For ID 0xff070018, mask 0x3 selects Kingston 8 GB and mask
0x1 selects Etron 4 GB when those profiles are enabled. This bypasses trial
training of unrelated profiles. If two profiles still share an ID and rank
mask, normal discovery remains necessary.

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

The timing profiles omit unused per-profile CSR arrays and share identical
initialization tables through lpddr4_timing_shared.c. Normal training and
configuration saving use the driver's shared CSR array. Table sharing
preserves every register value, entry count and write order.

See ``timing-tables.rst`` for the step-by-step procedure to register a new
chip, use ``import.py`` to identify reusable shared tables, and verify the
compiled timing data before testing on hardware.

Validation
----------

Run the software failure-handling checks with::

    python3 test/compulab/test_mrr_probe.py

The host test compiles the production probe functions with mocked register
access and the real polling macro. Its 30 scenarios cover single- and
dual-rank selection, zero identification bytes, stale/partial/inconsistent
captures, failed control reads, CS1 timeout handling, remaining ID/topology
ambiguity, PMU handshakes and controller/PHY reset after failures.

On the previously captured Kingston 8 GB board, both ranks should report::

    DDRINFO: MRR probe CS0 raw MR5-8: 00ffff00 00070700 00000000 00181800
    DDRINFO: MRR probe CS1 raw MR5-8: 00ffff00 00070700 00000000 00181800
    DDRINFO: MRR probe ID 0xff070018, rank mask 0x3
    DDRINFO: probe selected Kingston 8192MB; full training follows

The CS1 lines above are expected output for testing, not measurements from
the old capture, which read CS0 only. With valid EEPROM identification,
PROBE_ALWAYS prints probe results and keeps the EEPROM-selected profile.

Hardware validation is required: the published Synopsys LPDDR4 message block
uses bit 0 for DevInit, but that does not establish compatibility with every
i.MX8MP firmware revision or guarantee untrained receive-path operation.
Check raw MR values against reads after normal training, across supported
DRAM assemblies and cold/warm boots. An initialization-only pass may need
additional low-speed receive calibration on a particular board.
