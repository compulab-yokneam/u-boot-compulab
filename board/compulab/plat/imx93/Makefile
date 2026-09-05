#
# Copyright 2023 CompuLab LTD
#
# SPDX-License-Identifier:      GPL-2.0+
#
obj-y += board.o
obj-y += ../../common/
obj-y += ddr/
ifdef CONFIG_XPL_BUILD
obj-y += spl.o
endif
obj-$(CONFIG_XPL_BUILD) += eeprom_spl.o
ccflags-y += -I$(srctree)/board/compulab/common
