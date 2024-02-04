files=bl31.bin lpddr4_pmu_train_1d_dmem.bin lpddr4_pmu_train_1d_imem.bin lpddr4_pmu_train_2d_dmem.bin lpddr4_pmu_train_2d_imem.bin tee.bin
fw_src=$(srctree)/board/compulab/plat/imx8mp/firmware

all: $(files)

$(files):
	uudecode --output-file $@ $(fw_src)/$@.uue

clean:
	rm -rf $(files)
