# Building CompuLab U-Boot 2026.04

## 1. Prepare the build host

Use a Linux PC with an AArch64 cross compiler and the
[U-Boot build dependencies](doc/build/gcc.rst).

Select your cross compiler:

```sh
export CROSS_COMPILE=aarch64-linux-gnu-
```

On the existing development machine, use the installed tools instead:

```sh
export CROSS_COMPILE=/opt/gcc-15.2.0-nolibc/aarch64-linux/bin/aarch64-linux-
export PATH=/mnt/devel/Kostya/u-boot/ucm-imx8plus/kostya-build-tools/usr/bin:$PATH
```

## 2. Download the source

```sh
mkdir -p compulab-bootloader
cd compulab-bootloader
git clone --branch u-boot-compulab_v2026.04-ucm-imx8mp-experimental \
    https://github.com/compulab-yokneam/u-boot-compulab.git
cd u-boot-compulab
```

## 3. Select the product

Run **one** command from the table:

| Product | Command |
|---|---|
| UCM-iMX8M-Plus | `export MACHINE=ucm-imx8m-plus` |
| UCM-iMX8M-Plus — SBEV / eval V2 | `export MACHINE=ucm-imx8m-plus-sbev` |
| MCM-iMX8PLUS | `export MACHINE=mcm-imx8m-plus` |
| CL-SOM-iMX8PLUS | `export MACHINE=som-imx8m-plus` |
| 🔴 IOT-GATE-iMX8PLUS — not supported in this port | ~~`export MACHINE=iot-gate-imx8plus`~~ |
| 🔴 IOT-DIN-iMX8PLUS — not supported in this port | ~~`export MACHINE=iotdin-imx8p`~~ |

## 4. Select RAM capacity and configure

Run **one** RAM-selection command matching your module:

| Module RAM | Command |
|---|---|
| 2 GB | `export DRAM=d2` |
| 4 GB, including ISSI | `export DRAM=d4` |
| 1 GB or 8 GB | `export DRAM=d1d8` |

Then configure the build:

```sh
export BUILD="$(pwd)/../build-${MACHINE}-${DRAM}"
make O="$BUILD" "${MACHINE}_defconfig" "${DRAM}.config"
```

For the tested **4 GB MCM and CL-SOM modules**, select `d4`.

## 5. Build

```sh
make -j"$(nproc)" O="$BUILD" flash.bin
make O="$BUILD" u-boot-initial-env
```

The required firmware is included in the repository and extracted automatically.

## 6. Find the output files

```sh
ls -lh "$BUILD/flash.bin" "$BUILD/u-boot-initial-env"
sha256sum "$BUILD/flash.bin"
```

| File | Purpose |
|---|---|
| `flash.bin` | Bootloader image |
| `u-boot-initial-env` | Default U-Boot environment as text |

Building does not program the board.

## Optional configuration fragments

| Fragment | Purpose |
|---|---|
| `lab_temp.config` | Lab temperature settings |
| `spl_size.config` | Larger SPL size allowance |

To apply these options, replace the configuration command in step 4 with:

```sh
make O="$BUILD" "${MACHINE}_defconfig" "${DRAM}.config" \
    lab_temp.config spl_size.config
```

Then repeat step 5. Running a defconfig resets previous configuration changes.

## Optional USB RAM-boot test

Connect the board's SDP USB port to the build PC, select SDP boot mode, and reset
the board. With NXP UUU installed, run:

```sh
sudo uuu -d -v -b spl "$BUILD/flash.bin"
```

Stop UUU with **Ctrl-C** when finished. This loads the image into RAM without
flashing eMMC. DDR discovery may update the module's EEPROM.

## Additional notes

- CL-SOM uses the Startec 1024 × 600 LVDS panel as its U-Boot display. HDMI is
  not enabled in U-Boot.
- This is an experimental branch. See the separate
  [build compatibility review](doc/board/compulab/build-review-2026.md) for
  validation results and remaining limitations.
