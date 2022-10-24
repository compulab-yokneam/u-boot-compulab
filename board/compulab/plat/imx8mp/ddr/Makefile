ifeq ($(CONFIG_DRAM_D2D4),y)
obj-$(CONFIG_SPL_BUILD) += lpddr4_timing_ff000010.o
obj-$(CONFIG_SPL_BUILD) += lpddr4_timing_01061010_4G.o
obj-$(CONFIG_SPL_BUILD) += lpddr4_timing_01061010_2G.o
else
obj-$(CONFIG_SPL_BUILD) += lpddr4_timing_ff060018.o
obj-$(CONFIG_SPL_BUILD) += lpddr4_timing_01061010_1G.o
obj-$(CONFIG_SPL_BUILD) += lpddr4_timing_01061010_1G_4000.o
endif
obj-$(CONFIG_SPL_BUILD) += ddr.o
obj-y += ddr_common.o
