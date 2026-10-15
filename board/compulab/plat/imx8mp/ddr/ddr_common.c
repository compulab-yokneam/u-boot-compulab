#include <common.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/ddr.h>
#include <linux/iopoll.h>
#include "ddr.h"

#if defined(CONFIG_SPL_BUILD) && defined(CONFIG_COMPULAB_LPDDR4_MRR_PROBE)
/* Read one rank at a time; capture completion is not a DRAM presence bit. */
static int lpddr4_mr_read_timeout(unsigned int rank, unsigned int mr_addr,
				  u32 *data)
{
	u32 tmp;
	int ret;

	reg32_write(DRC_PERF_MON_MRR0_DAT(0), 0x1);
	ret = read_poll_timeout(reg32_read, tmp, !(tmp & 1), false,
				10000, DDRC_MRSTAT(0));
	if (ret)
		goto out;
	reg32_write(DDRC_MRCTRL0(0), (BIT(rank) << 4) | 1);
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

static int lpddr4_read_rank(unsigned int rank, unsigned int pass, u32 raw[4])
{
	unsigned int n, mr;
	int ret;

	for (n = 0; n < 4; n++) {
		/* Vary order so a stale last-MR value cannot form a valid ID. */
		mr = rank ? (3 - n + pass) % 4 : (n + pass) % 4;
		ret = lpddr4_mr_read_timeout(rank, mr + 5, &raw[mr]);
		if (ret)
			return ret;
	}
	printf("DDRINFO: MRR probe CS%u raw MR5-8: %08x %08x %08x %08x\n",
	       rank, raw[0], raw[1], raw[2], raw[3]);
	return 0;
}

static u32 lpddr4_raw_id(const u32 raw[4])
{
	u32 value = 0, byte;
	unsigned int mr, lane;

	for (mr = 0; mr < 4; mr++) {
		/* Match CompuLab's extraction, including valid zero MR6/MR7. */
		byte = raw[mr];
		for (lane = 0; lane < 3 && !(byte & 0xff); lane++)
			byte >>= 8;
		value = (value << 8) | (byte & 0xff);
	}
	return value;
}

int lpddr4_get_mr_probe(u32 *id, u32 *rank_mask)
{
	u32 cs0[4], cs1[4], first0[4], first1[4], control[4];
	u32 id0, id1, lanes = 0, fill;
	unsigned int pass, mr, lane;
	int ret;

	for (pass = 0; pass < 3; pass++) {
		ret = lpddr4_read_rank(0, pass, cs0);
		if (ret)
			return ret;
		if (pass && memcmp(first0, cs0, sizeof(cs0)))
			return -EIO;
		memcpy(first0, cs0, sizeof(cs0));

		ret = lpddr4_read_rank(1, pass, cs1);
		if (ret)
			return ret;
		if (pass && memcmp(first1, cs1, sizeof(cs1)))
			return -EIO;
		memcpy(first1, cs1, sizeof(cs1));

		/* An absent or noisy rank must not compromise CS0 reception. */
		ret = lpddr4_read_rank(0, pass + 1, control);
		if (ret || memcmp(cs0, control, sizeof(cs0)))
			return ret ? ret : -EIO;
	}

	id0 = lpddr4_raw_id(cs0);
	if (!(id0 >> 24) || id0 == 0xffffffff)
		return -EIO;
	/* Require identical data on the CS0 capture lanes, with others idle. */
	for (lane = 0; lane < 4; lane++)
		if ((cs0[0] >> (lane * 8)) & 0xff)
			lanes |= 0xffU << (lane * 8);
	for (mr = 0; mr < 4; mr++) {
		fill = ((id0 >> ((3 - mr) * 8)) & 0xff) * 0x01010101U;
		if (cs0[mr] != (fill & lanes))
			return -EIO;
	}

	id1 = lpddr4_raw_id(cs1);
	if (id1 == id0 && !memcmp(cs0, cs1, sizeof(cs0))) {
		*rank_mask = 3;
	} else if (!id1 || id1 == 0xffffffff) {
		/* Completed, repeatable idle patterns only; timeout is unknown. */
		for (mr = 0; mr < 4; mr++)
			if (cs1[mr] != (id1 == 0 ? 0 : lanes))
				return -EIO;
		*rank_mask = 1;
	} else {
		/* Mixed IDs/channels or stale captures need normal discovery. */
		return -EIO;
	}

	*id = id0;
	printf("DDRINFO: MRR probe ID 0x%08x, rank mask 0x%x\n", id0, *rank_mask);
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
