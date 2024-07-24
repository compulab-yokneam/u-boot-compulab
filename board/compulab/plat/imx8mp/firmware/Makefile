files=bl31.bin tee.bin lpddr4_pmu_train_1d_dmem_202006.bin lpddr4_pmu_train_1d_imem_202006.bin lpddr4_pmu_train_2d_dmem_202006.bin lpddr4_pmu_train_2d_imem_202006.bin 
fw_src=$(srctree)/board/compulab/plat/imx8mp/firmware

all: $(files)

$(files):
	uudecode --output-file $@ $(fw_src)/$@.uue

clean:
	rm -rf $(files)
