ifdef CONFIG_SPL_BUILD
ifdef CONFIG_IMX9_LOW_DRIVE_MODE
obj-$(CONFIG_UCM_IMX93_LPDDR4X) += lpddr4x_timing_ld.o
else
obj-$(CONFIG_UCM_IMX93_LPDDR4X) += lpddr4x_timing.o
obj-$(CONFIG_UCM_IMX93_LPDDR4X) += lpddr4x_timing_for_samsung_2G.o
endif
endif
obj-$(CONFIG_SPL_BUILD) += ddr.o
