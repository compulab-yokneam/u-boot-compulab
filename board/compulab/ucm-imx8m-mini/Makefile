#
# Copyright 2018 CompuLab
#
# SPDX-License-Identifier:      GPL-2.0+
#

ccflags-y += -I$(srctree)/board/compulab/plat/imx8mm/
ccflags-y += -I$(srctree)/board/compulab/plat/imx8/

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
else
obj-y += ucm-imx8m-mini.o
endif

obj-y += ../plat/imx8mm/
obj-y += ../plat/imx8/
