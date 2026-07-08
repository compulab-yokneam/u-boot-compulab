obj-$(CONFIG_SPL_BUILD) += ddr.o
#obj-y += ddr.o

TIMING_SRC = $(notdir $(wildcard $(srctree)/$(src)/lpddr4_timing_*.c))
TIMING_BIN = $(sort $(TIMING_SRC:.c=.bin))

lib-m += timings.bin

$(obj)/lpddr4_timing_%.bin: $(obj)/lpddr4_timing_%.o
	${OBJCOPY} -O binary $< $@.tmp
	@dd if=$@.tmp of=$@ bs=512 conv=sync

$(obj)/timings.bin: $(addprefix $(obj)/,$(TIMING_BIN))
	cat $^ > $@

clean:
	rm *.o *.bin *.tmp
