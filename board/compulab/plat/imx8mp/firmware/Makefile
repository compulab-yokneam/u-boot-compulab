files := bl31.bin lpddr4_pmu_train_1d_dmem.bin lpddr4_pmu_train_1d_imem.bin lpddr4_pmu_train_2d_dmem.bin lpddr4_pmu_train_2d_imem.bin tee.bin

all: $(files)

$(files):
	for file in $(files); do \
		uudecode --output-file $(O)/$$file $$file.uue; \
	done;

clean:
	for file in $(files); do \
		rm -rf $(O)/$$file; \
	done;
