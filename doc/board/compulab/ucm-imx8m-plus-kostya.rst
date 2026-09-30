UCM-iMX8M-Plus: Kostya 2026.04 port
==================================

Provenance and scope
--------------------

Base: origin/u-boot-compulab_v2026.04, commit 68f1db46d59.
Source: u-boot-compulab_v2023.04, commit 4c9fce9e0b5.
Branch: u-boot-kostya-2026.04.

This is a source port of UCM-iMX8M-Plus and its shared dependencies,
including the standard, SBEV and Falcon configurations. Other CompuLab
boards are outside its scope. The original source commits remain in the
repository; this branch imports their final board state and adapts it to
the 2026.04 APIs rather than merging the old NXP/U-Boot core.

Included features
-----------------

* DDR identification, EEPROM persistence, shared SPL/U-Boot DDR metadata,
  and the source branch's D2, D4 and D1/D8 timing sets, including Kingston.
* Board/carrier pinmux, PMIC setup, FEC/EQOS, Realtek PHY LED configuration,
  early EQOS PHY initialization, USB host/gadget, RTC and GPIO expander.
* CompuLab boot environment, overlays, deployment command, device-tree
  metadata, MMC environment selection, display drivers and carrier setup.
* Binman flash.bin packaging with the source branch's bundled compressed
  BL31, OP-TEE and DDR training firmware; default/custom environment images.
* Optional lab-temperature settings, extended SPL size, redundant MMC
  environment, HAB FIT authentication and HAB-signed EFI image loading.

Adaptations
-----------

* The D1/D8 fragment selects the larger 0x2c000 SPL allowance needed by
  the current compiler. Full SPL binary size limits are explicitly checked.
* SPI flash explicitly enables the newer MTD/SPI_MEM dependencies.
* NXP board paths, split headers, SPL configuration names, boot phase
  properties, device-tree pin names and LCDIF nodes follow the newer tree.
* EQOS uses the current clock driver; USB uses the current interrupt path.
* Shared EEPROM/FDT hardening already present in 2026.04 is retained.
* i.MX8MP DRAM choices use IMX8MP_DRAM_D2, IMX8MP_DRAM_D4 and
  IMX8MP_DRAM_D1D8 to avoid colliding with the i.MX95 DRAM choice. Existing
  d2.config, d4.config and d1d8.config fragment filenames still work.
* Core behavior changes are limited to UCM-iMX8M-Plus where practical.
  LVDS display activation checks the display class before calling its ops.
* The GPIO-expander retry reads the output register into its output cache,
  correcting the mismatched direction-register retry in the source branch.
* The old global unlimited-gunzip-output workaround is deliberately not
  carried forward: the current bounded decompressor is retained.
* The old unconditional disabling of EFI networking is not carried forward.
  Current EFI networking and signature validation remain enabled.
* The old thermal-trip early return is unnecessary: that function is absent
  in this base. NXP HAB FIT-FDT verification is already in the base.
* Generic EEPROM layout command changes and other-board drivers are not
  imported because these board configurations do not use them.

Building
--------

Example using the development machine's installed compiler::

    export CROSS_COMPILE=/opt/gcc-15.2.0-nolibc/aarch64-linux/bin/aarch64-linux-
    export PATH=/mnt/devel/Kostya/u-boot/ucm-imx8plus/kostya-build-tools/usr/bin:$PATH
    export BUILD=/mnt/devel/Kostya/u-boot/ucm-imx8plus/build-kostya-2026.04
    make O="$BUILD" ucm-imx8m-plus_defconfig d2.config
    make O="$BUILD" -j16 flash.bin u-boot-initial-env

Use ucm-imx8m-plus-sbev_defconfig for SBEV or
ucm-imx8m-plus_falcon_defconfig for Falcon. Select d4.config or d1d8.config
for the appropriate memory subset. The source branch's default is D2;
select the subset matching the physical module before boot testing.

The efitools package was extracted into the local kostya-build-tools folder
for cert-to-efi-sig-list. No system package installation was required.
The build also needs the normal U-Boot host tools and Python/binman modules.

Optional targets::

    make O="$BUILD" flash.bin-with-env
    # Supply $BUILD/u-boot-custom-env as a text environment first:
    make O="$BUILD" flash.bin-with-custom-env

flash.bin is also copied to flash.bin.d2, flash.bin.d4 or flash.bin.d1d8.
The environment-image helpers produce an eMMC image with the environment
at the configured byte offset. They do not write any storage device.

Validation and remaining hardware work
-------------------------------------

Build logs and images are kept in sibling build-kostya-2026.04-* folders.
The test matrix covers D2, D4, D1/D8, SBEV, Falcon, combined optional
fragments, and HAB-enabled compilation. HAB build artifacts are not signed
production images, and no production signing keys were supplied.

Compilation does not establish hardware compatibility. The bundled firmware
is preserved from 2023.04 and has not been replaced or validated with the
2026.04 port on hardware. Before release, test cold/warm boot for each DDR
part, SD/eMMC boot and environment persistence, Ethernet, USB/UUU, displays,
RTC, Linux device-tree metadata, and any enabled Falcon/HAB/capsule flow.
No target hardware was flashed and nothing was pushed to a remote.

Recorded build results
----------------------

GCC 15.2.0 cross compilation passed for all seven configurations:
D2, D4, D1/D8, SBEV, Falcon, combined d4/lab_temp/spl_size/redenv,
and the default configuration with IMX_HAB enabled. Final build logs
contained no compiler warnings or errors. The default environment image
was checked for its 4 MiB size, embedded flash payload, environment offset
and CRC. All 14 DDR timing files retain the source branch's numeric data;
all six bundled firmware blobs match it byte for byte.
