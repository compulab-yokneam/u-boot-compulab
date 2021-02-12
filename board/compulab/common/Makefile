#
# (C) Copyright 2011 - 2013 CompuLab, Ltd. <www.compulab.co.il>
#
# Author: Igor Grinberg <grinberg@compulab.co.il>
#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-y				+= common.o
obj-y				+= mmc.o
ifneq "$(or $(CONFIG_SYS_I2C),$(CONFIG_DM_I2C))" ""
obj-y				+= eeprom.o
endif # (CONFIG_SYS_I2C || CONFIG_DM_I2C2)
obj-$(CONFIG_LCD)		+= omap3_display.o
obj-$(CONFIG_SMC911X)		+= omap3_smc911x.o
