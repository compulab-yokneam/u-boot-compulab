#ifndef __DDR_H__
#define __DDR_H__

#include <stddef.h>
#define SHARED_DDR_INFO 0x20490000

#define DDR_CTL_BASE			0x4E300000
#define REG_DDR_SDRAM_MD_CNTL_2 	      (DDR_CTL_BASE + 0x270)
#define REG_DDR_SDRAM_MD_CNTL			(DDR_CTL_BASE + 0x120)
#define REG_DDR_SDRAM_MPR5      	      (DDR_CTL_BASE + 0x290)
#define REG_DDR_SDRAM_MPR4      	      (DDR_CTL_BASE + 0x28C)

#ifdef CONFIG_SPL_BUILD
#ifdef CONFIG_UCM_IMX93_LPDDR4X
extern struct dram_timing_info dram_timing;
#endif
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
	struct dram_timing_info* timing;
	char* desc[4];
};

static const struct lpddr4_desc lpddr4_array[] = {
	{.name = "dummy cfg",	.id = 0xdeadbeef, .subind = 0x1, .size = 512, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &dram_timing
#endif
	},
	{.name = "Micron",	.id = 0xff000110, .subind = 0xff, .size = 2048, .count = 1,
#ifdef CONFIG_SPL_BUILD
	.timing = &dram_timing
#endif
	},
	{.name = "Micron",	.id = 0xff060018, .subind = 0x8, .size = 2048, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &dram_timing
#endif
	},
	{.name = "Samsung",	.id = 0x01050008, .subind = 0x1, .size = 512, .count = 1,
#ifdef CONFIG_SPL_BUILD
		.timing = &dram_timing
#endif
	},
};

unsigned int lpddr4_get_mr(void);
const struct lpddr4_desc* lpddr4_get_desc_by_id(unsigned int id);
size_t lppdr4_get_ramsize(void);
#endif
