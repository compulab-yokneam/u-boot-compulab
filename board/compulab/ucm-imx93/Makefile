#
# Copyright 2023 CompuLab LTD
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += ucm-imx93.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
ifdef CONFIG_IMX9_LOW_DRIVE_MODE
obj-$(CONFIG_UCM_IMX93_LPDDR4X) += lpddr4x_timing_ld.o
else
obj-$(CONFIG_UCM_IMX93_LPDDR4X) += lpddr4x_timing.o
endif
endif
