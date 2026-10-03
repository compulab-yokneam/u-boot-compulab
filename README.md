# Disclaimer

# Configuring the build

## Prerequisites
It is up to developers to prepare the host machine; it requires:

* [Setup Cross Compiler](https://github.com/compulab-yokneam/meta-bsp-imx8mp/blob/kirkstone/Documentation/toolchain.md#linaro-toolchain-how-to)
* Install these packages: ``shareutils, swig``


## Setup U-Boot environment

* WorkDir:
```
mkdir -p compulab-bootloader/build && cd compulab-bootloader
export BUILD=$(pwd)/build
```

* Set a CompuLab machine:

| Machine | Command Line |
|---|---|
|ucm-imx8m-plus|```export MACHINE=ucm-imx8m-plus```|
|ucm-imx8m-plus (eval V2)|```export MACHINE=ucm-imx8m-plus-sbev```|
|mcm-imx8m-plus|```export MACHINE=mcm-imx8m-plus```|
|som-imx8m-plus|```export MACHINE=som-imx8m-plus```|
|iot-gate-imx8plus|```export MACHINE=iot-gate-imx8plus```|
|iotdin-imx8p|```export MACHINE=iotdin-imx8p```|

* Clone the source code:
```
git clone --branch u-boot-compulab_v2023.04-d1d8_d2d4 https://github.com/compulab-yokneam/u-boot-compulab.git
cd u-boot-compulab
```

## Create U-boot binary

* Apply the default machine config
```
make O=${BUILD} ${MACHINE}_defconfig
```

### DRAM configuration

The DRAM configuration controls which LPDDR4 timing sets are included in SPL:

| Option | Supported capacity |
|---|---:|
| `d1` | 1 GiB |
| `d2` | 2 GiB |
| `d4` | 4 GiB |
| `d8` | 8 GiB |

A CompuLab i.MX8MP defconfig selects D4 by default. All DRAM configurations
use `CONFIG_SPL_MAX_SIZE=0x2C000`.

Configuration fragments are processed from left to right. Always apply a
machine defconfig first. Use `d0.config` before a manual selection to disable
all default DRAM options, and then enable the required capacities.

#### Predefined two-size configurations

The predefined D1D8 and D2D4 fragments select both required timing sets:

```bash
# Include 1 GiB and 8 GiB timing sets
make O=${BUILD} ${MACHINE}_defconfig d1d8.config

# Include 2 GiB and 4 GiB timing sets
make O=${BUILD} ${MACHINE}_defconfig d2d4.config
```

#### Single-size configurations

Single-size configurations use the same `0x2C000` SPL size:

```bash
# D1: 1 GiB
make O=${BUILD} ${MACHINE}_defconfig d0.config d1.config

# D2: 2 GiB
make O=${BUILD} ${MACHINE}_defconfig d0.config d2.config

# D4: 4 GiB
make O=${BUILD} ${MACHINE}_defconfig d0.config d4.config

# D8: 8 GiB
make O=${BUILD} ${MACHINE}_defconfig d0.config d8.config
```

Do not apply a single-size fragment without `d0.config`. For example,
`d1.config` alone is added to the normal D4 default and produces a D1+D4
configuration.

#### Manual two-size configurations

Manual two-size configurations combine two individual DRAM fragments:

```bash
# D1D2
make O=${BUILD} ${MACHINE}_defconfig d0.config d1.config d2.config

# D1D4
make O=${BUILD} ${MACHINE}_defconfig d0.config d1.config d4.config

# D1D8 (the predefined d1d8.config fragment is preferred)
make O=${BUILD} ${MACHINE}_defconfig d0.config d1.config d8.config

# D2D4 (the predefined d2d4.config fragment is preferred)
make O=${BUILD} ${MACHINE}_defconfig d0.config d2.config d4.config

# D2D8
make O=${BUILD} ${MACHINE}_defconfig d0.config d2.config d8.config

# D4D8
make O=${BUILD} ${MACHINE}_defconfig d0.config d4.config d8.config
```

At least one DRAM capacity must be selected. Only use combinations supported
by the target hardware.

* Build flash.bin file:
```
nice make -j`nproc` O=${BUILD} flash.bin
```

* Create u-boot-initial-env file:
```
make O=${BUILD} u-boot-initial-env
```

* Results
```
ls -al ${BUILD}/{flash.bin,u-boot-initial-env}
```

## HDMI video output

U-Boot HDMI video is available on UCM-iMX8M-Plus, SBEV-UCMIMX8PLUS,
SOM-iMX8M-Plus, MCM-iMX8M-Plus, and IOT-GATE-IMX8PLUS. IOTDIN-IMX8P is not
supported because its carrier uses the HDMI DDC, HPD, and CEC pads as GPIOs.

The existing LVDS and MIPI display selection is preserved. HDMI is video link
2. Select HDMI and one of its supported modes persistently, then reboot:

```text
setenv video_link 2
setenv hdmi_mode 1080p60
saveenv
reset
```

The `hdmi_mode` values are:

| Value | Behaviour |
|---|---|
| `720p60` | Force 1280x720 at 60 Hz; this is the default. |
| `1080p60` | Force 1920x1080 at 60 Hz. |
| `auto` | Prefer EDID-advertised 1080p60, otherwise use 720p60. |

Use `videolink` at the U-Boot prompt to list the available display pipelines.

## Extra

### Extended-temperature configuration

`lab_temp.config` can be appended to any supported DRAM configuration to
enable the extended-temperature range:

```bash
make O=${BUILD} ${MACHINE}_defconfig d2d4.config lab_temp.config

make O=${BUILD} ${MACHINE}_defconfig d0.config d4.config lab_temp.config
```
