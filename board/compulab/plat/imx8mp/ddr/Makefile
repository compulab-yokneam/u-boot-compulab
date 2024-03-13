ifeq ($(CONFIG_SPL_BUILD),y)
obj-$(CONFIG_MICRON_4G) += lpddr4_timing_ff000010.o
obj-$(CONFIG_MICRON_8G) += lpddr4_timing_ff060018.o
obj-$(CONFIG_SAMSUNG_1G) += lpddr4_timing_01061010_1G.o lpddr4_timing_01061010_1G_4000.o
obj-$(CONFIG_SAMSUNG_2G) += lpddr4_timing_01061010_2G.o
obj-$(CONFIG_SAMSUNG_4G) += lpddr4_timing_01061010_4G.o
endif
obj-$(CONFIG_SPL_BUILD) += ddr.o
obj-y += ddr_common.o
