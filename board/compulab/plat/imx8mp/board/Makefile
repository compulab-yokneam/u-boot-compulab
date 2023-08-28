ccflags-y += -I$(srctree)/board/compulab/
ccflags-y += -I$(srctree)/board/compulab/plat/imx8mp/
ccflags-y += -I$(srctree)/board/freescale/

obj-y += board.o
ifndef CONFIG_SPL_BUILD
obj-y += cmd.o
endif
