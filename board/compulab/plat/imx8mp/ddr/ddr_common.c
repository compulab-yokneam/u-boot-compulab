#include <common.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/ddr.h>
#include "ddr.h"

static unsigned int _lpddr4_mr_read(unsigned int mr_rank, unsigned int mr_addr)
{
	unsigned int tmp;
	reg32_write(DRC_PERF_MON_MRR0_DAT(0), 0x1);
	do {
		tmp = reg32_read(DDRC_MRSTAT(0));
	} while (tmp & 0x1);

	reg32_write(DDRC_MRCTRL0(0), (mr_rank << 4) | 0x1);
	reg32_write(DDRC_MRCTRL1(0), (mr_addr << 8));
	reg32setbit(DDRC_MRCTRL0(0), 31);
	do {
		tmp = reg32_read(DRC_PERF_MON_MRR0_DAT(0));
	} while ((tmp & 0x8) == 0);
	tmp = reg32_read(DRC_PERF_MON_MRR1_DAT(0));
	reg32_write(DRC_PERF_MON_MRR0_DAT(0), 0x4);
	while(tmp) { //try to find a significant byte in the word
		if(tmp & 0xff) {
			tmp &= 0xff;
			break;
		}
		tmp >>= 8;
	}
	return tmp;
}

unsigned int lpddr4_get_mr(void)
{
	int i = 0, attempts = 5;
	unsigned int ddr_info = 0;
	unsigned int regs[] = { 5, 6, 7, 8 };

	do {
		for ( i = 0 ; i < ARRAY_SIZE(regs) ; i++ ) {
			unsigned int data = 0;
			data = _lpddr4_mr_read(0xF, regs[i]);
			ddr_info <<= 8;
			ddr_info += (data & 0xFF);
		}
		if ((ddr_info != 0xFFFFFFFF) && (ddr_info != 0))
			break; // The attempt was successfull
	} while ( --attempts );
	return	ddr_info;
}

const struct lpddr4_desc *lpddr4_get_desc_by_id(unsigned int id) {
	int i = 0;
	for ( i = 0; i < ARRAY_SIZE(lpddr4_array); i++ ) {
		if (lpddr4_array[i].id == id)
			return &lpddr4_array[i];
	}
	return NULL;
}

#ifdef SHARED_DDR_INFO
size_t lppdr4_get_ramsize() {
	struct lpddr4_tcm_desc *lpddr4_tcm_desc = (void *) SHARED_DDR_INFO;
	return lpddr4_tcm_desc->size;
}
#else
size_t lppdr4_get_ramsize() {
	size_t ramsize = 0;
	unsigned int id = lpddr4_get_mr();
	const struct lpddr4_desc *desc = lpddr4_get_desc_by_id(id);
	if (desc)
		ramsize = desc->size;
	return ramsize;
}
#endif
