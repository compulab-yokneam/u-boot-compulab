/*
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __DDR_H__
#define __DDR_H__

#ifdef CONFIG_SPL_BUILD
extern struct dram_timing_info ucm_dram_timing_01061010_2G;
extern struct dram_timing_info ucm_dram_timing_ff000010;
extern struct dram_timing_info ucm_dram_timing_01061010_4G;
extern struct dram_timing_info ucm_dram_timing_01061010_1G;
extern struct dram_timing_info ucm_dram_timing_01061010_1G_4000;
extern struct dram_timing_info ucm_dram_timing_ff060018;
#endif

void spl_dram_init(void);

struct lpddr4_tcm_desc {
	unsigned int size;
	unsigned int sign;
	unsigned int index;
	unsigned int ddr_init_status;
};

struct lpddr4_desc {
	char name[16];
	unsigned int id;
	unsigned int size;
	unsigned int count;
	/* an optional field
	 * use it if default is not the
	 * 1-st array entry */
	unsigned int _default;
	/* An optional field to distiguish DRAM chips that
	 * have different geometry, though return the same MRR.
	 * Default value 0xff
	 */
	u8	subind;
	struct dram_timing_info *timing;
	char *desc[4];
};

static const struct lpddr4_desc lpddr4_array[] = {
#ifdef CONFIG_SAMSUNG_2G
	{ .name = "deadbeaf",	.id = 0xdeadbeef, .subind = 0x2, .size = 2048, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &ucm_dram_timing_01061010_2G
#endif
	},
#endif
#ifdef CONFIG_SAMSUNG_4G
	{ .name = "Samsung",	.id = 0x01061010, .subind = 0x4, .size = 4096, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &ucm_dram_timing_01061010_4G
#endif
	},
#endif
#ifdef CONFIG_MICRON_4G
	{ .name = "Micron",	.id = 0xff000010, .subind = 0x4, .size = 4096, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &ucm_dram_timing_ff000010
#endif
	},
	{ .name = "Micron",	.id = 0xff000110, .subind = 0x4, .size = 4096, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &ucm_dram_timing_ff000010
#endif
	},
#endif
#ifdef CONFIG_SAMSUNG_2G
	{ .name = "Nanya",	.id = 0x05000010, .subind = 0x2, .size = 2048, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &ucm_dram_timing_01061010_2G
#endif
	},
	{ .name = "Samsung",	.id = 0x01061010, .subind = 0x2, .size = 2048, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &ucm_dram_timing_01061010_2G
#endif
	},
#endif
#ifdef CONFIG_SAMSUNG_1G
	{ .name = "deadbeaf",	.id = 0xdeadbeaf, .subind = 0x1, .size = 1024, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &ucm_dram_timing_01061010_1G
#endif
	},
	{ .name = "Samsung",	.id = 0x01050008, .subind = 0x1, .size = 1024, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &ucm_dram_timing_01061010_1G
#endif
	},
	{ .name = "Samsung",	.id = 0x01060008, .subind = 0x1, .size = 1024, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &ucm_dram_timing_01061010_1G_4000
#endif
	},
#endif
#ifdef CONFIG_MICRON_8G
	{ .name = "Micron",	.id = 0xff060018, .subind = 0x8, .size = 8192, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &ucm_dram_timing_ff060018
#endif
	},
	{ .name = "Micron",	.id = 0xff070018, .subind = 0x8, .size = 8192, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &ucm_dram_timing_ff060018
#endif
	},
#endif
};

unsigned int lpddr4_get_mr(void);
const struct lpddr4_desc *lpddr4_get_desc_by_id(unsigned int id);
size_t lppdr4_get_ramsize(void);
#endif
