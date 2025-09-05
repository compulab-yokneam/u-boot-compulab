#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-y				+= fdt.o
obj-y				+= common.o
ifndef CONFIG_SYS_REDUNDAND_ENVIRONMENT
obj-y				+= mmc.o
endif
obj-y				+= ../../freescale/common/mmc.o
ifneq "$(or $(CONFIG_SYS_I2C),$(CONFIG_DM_I2C))" ""
obj-y				+= eeprom.o
endif
