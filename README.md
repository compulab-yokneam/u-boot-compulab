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
git clone --branch u-boot-compulab_v2023.04 https://github.com/compulab-yokneam/u-boot-compulab.git
cd u-boot-compulab
```

## Create U-boot binary

* Apply the default machine config
```
make O=${BUILD} ${MACHINE}_defconfig
```

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

## Extra

### Configuration fragments compatibility matrix:

|_|_|d2d4|d1d8|etron|lab_temp|spl_size|
|---|---|---|---|---|---|---|
|Dram D2/D4 support|d2d4|.|n/a|v|v|v|
|Dram D1/D8 support|d1d8|n/a|.|v|v|v|
|Dram Etron support|etron|v|v|.|v|v|
|Extended temp range|lab_temp|v|v|v|.|v|
|Extra spl size|spl_size|v|v|v|v|.|

### Examples for applying configuration fragments:
* d2d4 with all options:
```
make O=${BUILD} ${MACHINE}_defconfig d2d4.config etron.config lab_temp.config spl_size.config
```
* d1d8 with all options:
```
make O=${BUILD} ${MACHINE}_defconfig d1d8.config etron.config lab_temp.config spl_size.config
```
