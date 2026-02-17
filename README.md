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

### Available configuration fragments:

|conf fragmen file name|description|
|---|---|
|d2.config|Dram D2 support
|d4.config|Dram D4 support|
|d1d8.config|Dram D1/D8 support|
|lab_temp.config|Extended temp range|
|spl_size.config|Extra spl size|

### Examples for applying configuration fragments:
* d2 with all options:
```
make O=${BUILD} ${MACHINE}_defconfig d2.config lab_temp.config spl_size.config
```
* d4 with all options:
```
make O=${BUILD} ${MACHINE}_defconfig d4.config lab_temp.config spl_size.config
```
* d1d8 with all options:
```
make O=${BUILD} ${MACHINE}_defconfig d1d8.config lab_temp.config spl_size.config
```
