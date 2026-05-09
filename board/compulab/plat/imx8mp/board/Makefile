ccflags-y += -I$(srctree)/board/compulab/
ccflags-y += -I$(srctree)/board/compulab/plat/imx8mp/
ccflags-y += -I$(srctree)/board/freescale/

DRAM_CONF :=
ifeq ($(CONFIG_DRAM_D1),y)
	DRAM_CONF := $(DRAM_CONF)d1
endif

ifeq ($(CONFIG_DRAM_D2),y)
	DRAM_CONF := $(DRAM_CONF)d2
endif

ifeq ($(CONFIG_DRAM_D4),y)
	DRAM_CONF := $(DRAM_CONF)d4
endif

ifeq ($(CONFIG_DRAM_D8),y)
	DRAM_CONF := $(DRAM_CONF)d8
endif

ifeq ($(DRAM_CONF),)
$(error DRAM_CONF is empty. Please provide a valid configuration: [d1,d2,d4,d8].config + spl_size.config )
endif

ccflags-y += -DDRAM_CONF='"$(DRAM_CONF)"'

obj-y += board.o
obj-y += efi_capsule.o
ifndef CONFIG_SPL_BUILD
obj-y += cmd.o
endif
