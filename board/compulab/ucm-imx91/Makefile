#
# Copyright 2024 NXP
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += ucm-imx91.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
obj-$(CONFIG_UCM_IMX91_LPDDR4X) += lpddr4x_timing.o
endif
