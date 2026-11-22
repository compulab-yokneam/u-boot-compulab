#
# Copyright 2026 CompuLab
#
# SPDX-License-Identifier:      GPL-2.0+
#

ccflags-y += -I$(srctree)/board/compulab/plat/imx8mp/
ccflags-y += -I$(srctree)/board/compulab/plat/imx8/

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
else
obj-y += iot-gate-imx8pv2.o
endif

obj-y += ../plat/imx8mp/
obj-y += ../plat/imx8/
