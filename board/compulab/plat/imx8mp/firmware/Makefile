all: bl31.bin.fw lpddr4_pmu_train_1d_dmem_202006.bin.fw lpddr4_pmu_train_1d_dmem.bin.fw lpddr4_pmu_train_1d_imem_202006.bin.fw lpddr4_pmu_train_1d_imem.bin.fw lpddr4_pmu_train_2d_dmem_202006.bin.fw lpddr4_pmu_train_2d_dmem.bin.fw lpddr4_pmu_train_2d_imem_202006.bin.fw lpddr4_pmu_train_2d_imem.bin.fw tee.bin.fw

bl31.bin.fw lpddr4_pmu_train_1d_dmem_202006.bin.fw lpddr4_pmu_train_1d_dmem.bin.fw lpddr4_pmu_train_1d_imem_202006.bin.fw lpddr4_pmu_train_1d_imem.bin.fw lpddr4_pmu_train_2d_dmem_202006.bin.fw lpddr4_pmu_train_2d_dmem.bin.fw lpddr4_pmu_train_2d_imem_202006.bin.fw lpddr4_pmu_train_2d_imem.bin.fw tee.bin.fw:
	uudecode $@.uue

clean:
	rm -rf *.bin.fw
