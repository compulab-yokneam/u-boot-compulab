#
# Copyright 2017 CompuLab LTD.
#
# SPDX-License-Identifier:      GPL-2.0+
#

ccflags-y += -I$(srctree)/board/compulab/plat/imx8mq/
ccflags-y += -I$(srctree)/board/compulab/plat/imx8/

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
obj-y += ../common/pfuze.o
else
obj-y += cl-som-imx8.o
endif

obj-y += ../plat/imx8mq/
obj-y += ../plat/imx8/
