# CompuLab i.MX8M-Plus U-Boot 2026.04 (experimental)

## Scope

This branch ports UCM-iMX8M-Plus, UCM SBEV, MCM-iMX8PLUS and
CL-SOM-iMX8PLUS support from the 2023.04 tree to U-Boot 2026.04.
It is an experimental branch, not a production or security qualification.
The build workflow below follows the 2023.04 README.

## Prerequisites

Use a Linux build host and an AArch64 cross compiler. See
[the U-Boot GCC build requirements](doc/build/gcc.rst) for your distribution.
Install the host tools and Python/binman dependencies listed there, including
SWIG and efitools (which provides cert-to-efi-sig-list).

For a distribution cross compiler:

```sh
export CROSS_COMPILE=aarch64-linux-gnu-
```

The development builds were tested with GCC 15.2.0:

```sh
export CROSS_COMPILE=/opt/gcc-15.2.0-nolibc/aarch64-linux/bin/aarch64-linux-
```

If using locally extracted host tools, add their bin directory to PATH.
The repository includes compressed BL31, OP-TEE and DDR training firmware
under board/compulab/plat/imx8mp/firmware; the build extracts these automatically.
Their inclusion does not imply a firmware update or security audit.

## Setup U-Boot environment

```sh
mkdir -p compulab-bootloader
cd compulab-bootloader
git clone --branch u-boot-compulab_v2026.04-ucm-imx8mp-experimental \
    https://github.com/compulab-yokneam/u-boot-compulab.git
cd u-boot-compulab
```

Select one machine:

| Product | Command |
|---|---|
| UCM-iMX8M-Plus | `export MACHINE=ucm-imx8m-plus` |
| UCM-iMX8M-Plus, SBEV/eval V2 | `export MACHINE=ucm-imx8m-plus-sbev` |
| MCM-iMX8PLUS | `export MACHINE=mcm-imx8m-plus` |
| CL-SOM-iMX8PLUS | `export MACHINE=som-imx8m-plus` |

IOT-GATE and IOT-DIN entries from the 2023.04 README are outside this port's
scope. Do not infer support for them from the shared platform code.

Select the RAM subset and a separate output directory for each combination:

```sh
export DRAM=d4
export BUILD="$(pwd)/../build-${MACHINE}-${DRAM}"
make O="$BUILD" "${MACHINE}_defconfig" "${DRAM}.config"
```

The bare defconfig defaults to D2. Use D4 for the tested 4 GB MCM and CL-SOM
modules, including the ISSI 4 GB part with ID 0x13000518.

## Create U-Boot binary

```sh
nice make -j"$(nproc)" O="$BUILD" flash.bin
make O="$BUILD" u-boot-initial-env
ls -l "$BUILD/flash.bin" "$BUILD/u-boot-initial-env"
sha256sum "$BUILD/flash.bin"
```

These commands create files only; they do not program the board.
u-boot-initial-env is a text environment, not a standalone boot image.

## Extra

### Configuration fragments

| Fragment | Description |
|---|---|
| d2.config | 2 GB DDR timing subset (default) |
| d4.config | 4 GB DDR timing subset, including ISSI |
| d1d8.config | 1 GB / 8 GB subset; includes the larger SPL allowance |
| lab_temp.config | Lab temperature configuration; not temperature qualification |
| spl_size.config | Larger SPL size allowance |

Examples matching the 2023.04 workflow:

```sh
make O="$BUILD" "${MACHINE}_defconfig" d2.config lab_temp.config spl_size.config
make O="$BUILD" "${MACHINE}_defconfig" d4.config lab_temp.config spl_size.config
make O="$BUILD" "${MACHINE}_defconfig" d1d8.config lab_temp.config spl_size.config
```

These are alternatives: choose one configuration, then build. Each defconfig
invocation resets the configuration. Match the RAM subset to the module.

### Optional environment images

```sh
make O="$BUILD" flash.bin-with-env
# First create $BUILD/u-boot-custom-env as a text environment:
make O="$BUILD" flash.bin-with-custom-env
```

These images embed an environment at the configured offset. Use them only
with a deployment procedure appropriate to the board and storage layout.

### USB RAM-boot testing

With the board in ROM SDP mode, run on the connected host:

```sh
sudo uuu -d -v -b spl "$BUILD/flash.bin"
```

The daemon mode allows DDR discovery resets and repeated downloads. Stop UUU
with Ctrl-C when finished. This command does not flash eMMC; DDR discovery
may update the module's EEPROM. Keep the existing bootloader backup before
using any separate flash procedure.

## Display and validation

CL-SOM uses Startec 1024x600 LVDS as its sole U-Boot display; MIPI-DSI is
disabled and HDMI is not enabled. This does not configure Linux's separate
device tree. MCM and UCM also have Startec LVDS support.

CL-SOM ISSI 4 GB boot and LVDS were verified on hardware: correct text,
colors and no flicker. UCM DHCP and LVDS were previously confirmed.
MCM boots with Samsung 4 GB; the user reports completing MCM validation,
but a detailed final test matrix was not recorded here.

See [UCM port notes](doc/board/compulab/ucm-imx8m-plus-kostya.rst) and
[MCM/CL-SOM port notes](doc/board/compulab/mcm-som-imx8m-plus-2026.rst).
Successful builds and these board tests do not establish complete
2023.04 feature parity, long-duration DDR stability, or secure-boot compliance.

See [the build compatibility review](doc/board/compulab/build-review-2026.md)
for the checked scope and outstanding style findings.
