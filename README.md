# Disclaimer

| !IMPORTANT! | This is a development branch, that is not relelased by CompuLab yet|
|---|---|

# Configuring the build

## Prerequisites
It is up to developers to prepare the host machine; it requires:

* [Setup Cross Compiler](https://github.com/compulab-yokneam/meta-bsp-imx8mp/blob/kirkstone/Documentation/toolchain.md#linaro-toolchain-how-to)
* Install these packages: ``shareutils, swing``


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
|~~mcm-imx8m-plus~~|~~```export MACHINE=mcm-imx8m-plus```~~|
|~~som-imx8m-plus~~|~~```export MACHINE=som-imx8m-plus```~~|
|~~iot-gate-imx8plus~~|~~```export MACHINE=iot-gate-imx8plus```~~|
|~~iotdin-imx8p~~|~~```export MACHINE=iotdin-imx8p```~~|

* Clone the source code:
```
git clone --branch u-boot-compulab_v2023.04 https://github.com/compulab-yokneam/u-boot-compulab.git
cd u-boot-compulab
```

## Create U-boot binary

* Apply the machine Config
```
make O=${BUILD} ${MACHINE}_defconfig
```

* Build
```
nice make -j`nproc` O=${BUILD} flash.bin
```

* Result
```
ls -al ${BUILD}/flash.bin
```
