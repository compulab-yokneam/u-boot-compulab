.. SPDX-License-Identifier: GPL-2.0+

CompuLab MCM-iMX8Pv2
====================

Select ``mcm-imx8pv2_defconfig`` to build MCM-iMX8Pv2 with SBC-MCM8PV2.
The target selects TARGET_COMPULAB_IMX8MP and uses the shared CompuLab SPL,
PMIC initialization, compact LPDDR4 profiles and EEPROM discovery code.
The experimental LPDDR4 MRR probe remains optional and disabled by default.

Schematic basis
---------------

The module description was compared against these supplied schematics:

* MCM_IMX8M_PLUS.V1_0.pdf, document 8000176000, revision 1.0.
* MCM-IMX8PV2_R1V0_12_CL2_T.PDF, document 8000161001. Its cover identifies
  MCM-IMX8PV2 revision 1.0; individual drawing footers retain the old
  MCM-iMX8PLUS title and show revision 1.1.

The new module retains UART2 on its dedicated pads, PCA9450C on I2C1 at
0x25, EEPROM on I2C2 at 0x50, eight-bit eMMC on USDHC3, the active-low
GPIO1_IO12 LED and WDOG1 on GPIO1_IO02. The LPDDR4 drawing retains the
channel/byte-lane connections of the predecessor; the existing profiles
are reused, subject to validation on the actual memory assemblies.

The RTC changes from AM1805 to RX8010 on I2C2 at 0x32 (sheet 9). The cover's
I2C address summary still lists the old RTC address; the device drawing is
used here. The EEPROM symbol says 24C08 while its annotation names CAT24C02;
the existing 24C08 description is retained pending assembly confirmation.

The carrier interface changes substantially. In particular, the old
FlexSPI NAND-pad signals are no longer exposed, FEC reset is exported on
SAI2_TXFS (GPIO4_IO24), and EQOS reset can be connected through R72/R73 to
GPIO1_IO01 or GPIO1_IO05. Those links and the external PHY addresses must
match the SBC-MCM8PV2 reset routing described below.

Carrier integration
-------------------

The module DTS includes ``arch/arm/dts/sbc-mcm8pv2.dtsi``, based on
SBC-MCM8PV2_REV1V0.pdf, document 8000294000, revision 1.0. Carrier Ethernet,
LVDS, MIPI-DSI and HDMI definitions live in that separate file. The I2C3 bus and
PCA9555 are enabled because both display reset signals depend on them.

The carrier configuration is:

* EQOS: RTL8211F, MDIO address 4, active-low reset GPIO1_IO01 (sheet 4).
* FEC: KSZ9131, MDIO address 7, active-low reset GPIO4_IO24/SAI2_TXFS
  (sheet 5 annotates address 5; address 7 was confirmed on hardware). The Micrel KSZ90x1 driver includes KSZ9131 support.
* Both MACs use ``rgmii-id`` so their PHY drivers enable RX/TX delays.
* PCA9555: I2C3 at 0x20. IO0.2 resets DSI (active low); IO0.3 releases
  LVDS display reset (active high).
* LVDS backlight: TPS61165 CTRL on SPDIF_RX/PWM2.
* DSI backlight: EN/PWM on SPDIF_TX/PWM3. The schematic annotation says
  PWM1, but the i.MX8MP pin mux assigns PWM3 to this pad.
* HDMI: P22 on sheet 8, with dedicated HDMI DDC, HPD and CEC pads.
  Its 5 V supply and level-shifter enables are fixed in hardware; no
  software-controlled GPIO power or reset is required.
* Carrier EEPROM: I2C3 at 0x54. The carrier EEPROM Kconfig bus is therefore
  2 (zero-based); the module EEPROM remains on bus 1.

The carrier needs the Micrel/Realtek PHY, PCA953x GPIO, i.MX PWM,
PWM-backlight, LCDIFv3, LVDS, SEC-DSI and i.MX8MP HDMI drivers enabled
in U-Boot. These are included in the defconfig. The U-Boot DTSI supplies the FEC driver's
MAC-level reset properties and removes clock assignments managed directly
by the Ethernet and display drivers.

Panel assumptions
~~~~~~~~~~~~~~~~~

The carrier schematic specifies connectors, not attached panel models.
The current defaults reuse the existing MCM setup: a 1024x600 LVDS panel
at 42.105 MHz with 24-bit JEIDA mapping, and a four-lane Startek ILI9881C
720x1280 DSI panel. Confirm these models, timings and bit mapping against
the actual panels. A different DSI panel may need a different driver and
initialization sequence; enabling its connector alone is insufficient.

Both backlights use 20 kHz PWM. This is within the TPS61165's recommended
PWM range (see https://www.ti.com/lit/ds/symlink/tps61165.pdf). Confirm the
assembled DSI backlight driver's PWM specification during hardware testing.
The ILI9881C driver now honors an optional ``backlight`` phandle and
turns the PWM backlight on after panel initialization and off on removal.
Panels without a backlight phandle retain their existing behavior.

U-Boot's video-link framework uses one selected display path per boot.
Run ``videolink list`` to identify each path, then set ``video_link`` to
the desired link number, run ``saveenv`` and reboot. Check the panel outputs
separately with the matching panel attached. This board defaults to
``video_link=2`` (LCDIF3/HDMI) and ``hdmi_mode=auto``, with EDID-based
720p60/1080p60 selection and a 720p60 fallback. Existing saved environments
retain their previous settings: use ``setenv video_link 2``,
``setenv hdmi_mode auto``, ``saveenv`` and ``reset`` to select HDMI.
Confirm link numbering with ``videolink list``.
Other CompuLab boards retain their existing default link.

The module's eMMC, UART2, PMIC, EEPROM, RTC, LED and USB1 peripheral mode
remain enabled. USDHC2, external SPI flash and USB host remain disabled.
U-Boot's MMC aliases retain eMMC as mmc2, and environment storage and the
default MMC root point to eMMC.

Build and validation
--------------------

For a compile/link check, with an AArch64 cross compiler installed::

    make O=/tmp/mcm-imx8pv2-build CROSS_COMPILE=aarch64-linux-gnu- mcm-imx8pv2_defconfig
    make O=/tmp/mcm-imx8pv2-build CROSS_COMPILE=aarch64-linux-gnu- -j8 u-boot.bin spl/u-boot-spl

Use the BSP image packaging process with the matching LPDDR4 PHY firmware,
TF-A and other required boot components to produce a flashable image.
Compile/link checks do not validate DDR signal integrity, timing margins,
PMIC behavior, RTC operation or cold/warm boot on hardware. Validate those
on the actual module and memory assembly before deployment.

Run the host backlight lifecycle/error checks with::

    python3 test/compulab/test_ili9881c_backlight.py

On hardware, check both PHY identities with ``mii info`` and test traffic
on each MAC separately with ``ethrotate=no`` to prevent fallback to the
other interface. Verify display reset levels, PWM outputs and cold/warm
boots with each selected panel. Build checks and mocked backlight tests
do not establish operation on the actual carrier.
