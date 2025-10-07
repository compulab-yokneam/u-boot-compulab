obj-y += ddr.o

lib-m += lpddr4_timing_term.bin
lib-m += lpddr4_timing_01061010.bin

lpddr4_timing_%.bin: lpddr4_timing_%.o
	${OBJCOPY} -O binary $< $@.tmp
	@dd if=$@.tmp of=$@ bs=512 conv=sync

clean:
	rm *.o *.bin *.tmp
