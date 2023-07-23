#
# Copyright 2023 CompuLab LTD
#
# SPDX-License-Identifier:      GPL-2.0+
#
obj-y += ucm-imx93.o
obj-y += ddr/
ifdef CONFIG_SPL_BUILD
obj-y += spl.o
endif
obj-$(CONFIG_SPL_BUILD) += eeprom_spl.o
ccflags-y += -I$(srctree)/board/compulab/common