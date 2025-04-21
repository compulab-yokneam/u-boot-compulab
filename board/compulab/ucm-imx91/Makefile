#
# Copyright 2024 NXP
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += ucm-imx91.o
obj-y += ddr/

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
endif
obj-$(CONFIG_SPL_BUILD) += eeprom_spl.o
ccflags-y += -I$(srctree)/board/compulab/common
