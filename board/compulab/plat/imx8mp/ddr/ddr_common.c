#include <common.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/ddr.h>
#include <linux/iopoll.h>
#include "ddr.h"

#if defined(CONFIG_SPL_BUILD) && defined(CONFIG_COMPULAB_LPDDR4_MRR_PROBE)
/* Preserve raw lane data: a zero MR6/MR7 byte is a valid result. */
static int lpddr4_mr_read_timeout(unsigned int mr_addr, u32 *data)
{
	u32 tmp;
	int ret;

	reg32_write(DRC_PERF_MON_MRR0_DAT(0), 0x1);
	ret = read_poll_timeout(reg32_read, tmp, !(tmp & 1), false,
				10000, DDRC_MRSTAT(0));
	if (ret)
		goto out;
	/* Read CS0 only; do not broadcast to possibly absent ranks. */
	reg32_write(DDRC_MRCTRL0(0), (1 << 4) | 1);
	reg32_write(DDRC_MRCTRL1(0), mr_addr << 8);
	reg32setbit(DDRC_MRCTRL0(0), 31);
	ret = read_poll_timeout(reg32_read, tmp, tmp & 0x8, false,
				10000, DRC_PERF_MON_MRR0_DAT(0));
	if (!ret)
		*data = reg32_read(DRC_PERF_MON_MRR1_DAT(0));
out:
	reg32_write(DRC_PERF_MON_MRR0_DAT(0), 0x4);
	return ret;
}

int lpddr4_get_mr_probe(u32 *id)
{
	u32 raw[4], previous[4], value, byte;
	int pass, mr, lane, ret;

	for (pass = 0; pass < 3; pass++) {
		value = 0;
		for (mr = 0; mr < 4; mr++) {
			ret = lpddr4_mr_read_timeout(mr + 5, &raw[mr]);
			if (ret)
				return ret;
			/* Match the existing CompuLab significant-byte extraction. */
			byte = raw[mr];
			for (lane = 0; lane < 3 && !(byte & 0xff); lane++)
				byte >>= 8;
			value = (value << 8) | (byte & 0xff);
		}
		printf("DDRINFO: MRR probe raw MR5-8: %08x %08x %08x %08x\n",
		       raw[0], raw[1], raw[2], raw[3]);
		if (!value || value == 0xffffffff)
			return -EIO;
		if (pass && memcmp(previous, raw, sizeof(raw)))
			return -EIO;
		memcpy(previous, raw, sizeof(raw));
	}
	*id = value;
	return 0;
}
#endif

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
int lppdr4_get_ram_name(char *name) {
    unsigned int id = lpddr4_get_mr();
    const struct lpddr4_desc *desc = lpddr4_get_desc_by_id(id);
    if (desc) {
        memcpy(name, desc->name, 16);
        return 0;
    }
    return 1;
}
