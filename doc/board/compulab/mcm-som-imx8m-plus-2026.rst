MCM-iMX8PLUS and CL-SOM-iMX8PLUS: 2026.04 port
============================================

Source and scope
----------------

Board support is imported from u-boot-compulab_v2023.04 (4c9fce9e0b5)
onto the UCM experimental 2026.04 branch. The existing newer shared
CompuLab platform, DDR timing tables and bundled firmware are reused.

Product configurations:

* MCM-iMX8PLUS: mcm-imx8m-plus_defconfig.
* CL-SOM-iMX8PLUS: som-imx8m-plus_defconfig.

The hidden COMPULAB_IMX8MP symbol scopes common EEPROM, image packaging,
network, GPIO-expander and optional authentication support to these two
products and UCM-iMX8M-Plus. UCM/SBEV panel-specific settings stay scoped
to UCM. The imported SOM board retains its expansion-power, Wi-Fi and
board-revision PHY-address handling.

The new boards retain EQOS assigned clocks (125 MHz transmit, 100 MHz
timer). CL-SOM retains the legacy LCDIF1 divider code and matching 594 MHz
video PLL. MCM configures its Startec 1024x600 LVDS output with a 594 MHz
video PLL, 42.428572 MHz pixel clock and 297 MHz LVDS clock.

Building
--------

On the development machine, from the source directory::

    export CROSS_COMPILE=/opt/gcc-15.2.0-nolibc/aarch64-linux/bin/aarch64-linux-
    export PATH=/mnt/devel/Kostya/u-boot/ucm-imx8plus/kostya-build-tools/usr/bin:$PATH
    make O=../build-mcm-2026.04 mcm-imx8m-plus_defconfig d2.config
    make O=../build-mcm-2026.04 -j8 flash.bin u-boot-initial-env

Use som-imx8m-plus_defconfig and a separate output directory for CL-SOM.
Select d4.config for the 4 GB timing subset or d1d8.config for the 1/8 GB
subset. D2 is the default. Match the subset to the physical module.
Image helpers also provide flash.bin-with-env. These targets only create
files; they do not flash the board.

Validation
----------

GCC 15.2.0 build checks cover both products with D2, D4 and D1/D8.
UCM and SBEV are rebuilt to check the common-symbol refactoring.
Inspect the generated DTBs for product model, EQOS assigned rates and
SPL PMIC availability; image packaging uses the existing binman layout.

These additional products have not been boot-tested during this port.
Before deployment, validate ROM/SDP and eMMC/SD boot, DDR detection and
stability, both Ethernet interfaces, USB, the actual carrier and panel,
environment persistence and Linux boot on each physical board.
A successful build does not establish HAB/EFI authentication behavior;
no new signing or security qualification is performed here.

MCM Startec LVDS follow-up
-------------------------

A user-provided hardware log confirms MCM boots commit 4e796967a0d2 and
initializes Samsung 4 GB DDR at 3200 MT/s. The panel fails to probe with
-ENODEV. The I2C expander at bus 1/address 0x20 and fixed panel regulator
are present; the missing PWM backlight driver is the panel dependency
that cannot bind.

The fix enables DM_PWM/PWM_IMX (and therefore BACKLIGHT_PWM), applies the
Startec pixel/LVDS clock dividers to MCM, enables the LDB clock through
the clock framework, and makes the LDB driver honor fsl,data-mapping
instead of forcing JEIDA for non-UCM boards. Display-enable errors now
propagate to the caller.

The corrected MCM D4 image and SBEV regression image compile successfully.
Visual confirmation on the corrected MCM image is still required; the
backlight-only symptom on the earlier image is not a display pass.
