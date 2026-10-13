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
timer). Their LCDIF1/DSI configuration retains the legacy divider code
and assigns the matching 594 MHz video PLL. The UCM Startec LVDS clock
override is not copied into these board trees.

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
