#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-y				+= common.o
obj-y				+= ../../freescale/common/mmc.o
ifneq "$(or $(CONFIG_SYS_I2C),$(CONFIG_DM_I2C))" ""
obj-y				+= eeprom.o
endif # (CONFIG_SYS_I2C || CONFIG_DM_I2C2)
