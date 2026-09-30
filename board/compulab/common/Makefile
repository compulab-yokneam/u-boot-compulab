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
obj-y				+= eeprom.o
endif
endif

ifdef CONFIG_TARGET_UCM_IMX8M_PLUS
ifndef CONFIG_SPL_BUILD
obj-y += fake_backlight.o
obj-y += ../../nxp/common/mmc.o
endif
endif

ifeq ($(CONFIG_TARGET_UCM_IMX8M_PLUS)$(CONFIG_SPL_BUILD),yy)
obj-y += eeprom.o
endif
