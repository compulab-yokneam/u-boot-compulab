# SPDX-License-Identifier: GPL-2.0+
ifeq ($(CONFIG_TARGET_COMPULAB_IMX8MP),y)
#
# SPDX-License-Identifier:	GPL-2.0+
#

ifndef CONFIG_SPL_BUILD
obj-y				+= fdt.o
obj-y				+= common.o
ifndef CONFIG_SYS_REDUNDAND_ENVIRONMENT
obj-y				+= mmc.o
endif
ifneq "$(or $(CONFIG_SYS_I2C),$(CONFIG_DM_I2C))" ""
obj-y				+= eeprom_imx8mp.o
endif
endif

ifdef CONFIG_COMPULAB_IMX8MP
ifndef CONFIG_SPL_BUILD
obj-y += fake_backlight.o
obj-y += ../../nxp/common/mmc.o
endif
endif

ifeq ($(CONFIG_COMPULAB_IMX8MP)$(CONFIG_SPL_BUILD),yy)
obj-y += eeprom_imx8mp.o
endif

else
# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2011 - 2013 CompuLab, Ltd. <www.compulab.co.il>
#
# Author: Igor Grinberg <grinberg@compulab.co.il>

CL_EEPROM=y

ifdef CONFIG_TARGET_TRIMSLICE
CL_EEPROM=
endif

obj-y				+= common.o
obj-$(CL_EEPROM)		+= eeprom.o
obj-$(CONFIG_SMC911X)		+= omap3_smc911x.o

endif
