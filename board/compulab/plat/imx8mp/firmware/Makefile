all: bl31.bin lpddr4_pmu_train_1d_dmem_202006.bin lpddr4_pmu_train_1d_dmem.bin lpddr4_pmu_train_1d_imem_202006.bin lpddr4_pmu_train_1d_imem.bin lpddr4_pmu_train_2d_dmem_202006.bin lpddr4_pmu_train_2d_dmem.bin lpddr4_pmu_train_2d_imem_202006.bin lpddr4_pmu_train_2d_imem.bin tee.bin

bl31.bin lpddr4_pmu_train_1d_dmem_202006.bin lpddr4_pmu_train_1d_dmem.bin lpddr4_pmu_train_1d_imem_202006.bin lpddr4_pmu_train_1d_imem.bin lpddr4_pmu_train_2d_dmem_202006.bin lpddr4_pmu_train_2d_dmem.bin lpddr4_pmu_train_2d_imem_202006.bin lpddr4_pmu_train_2d_imem.bin tee.bin:
	uudecode --output-file $@ $@.uue

clean:
	rm -rf *.bin
