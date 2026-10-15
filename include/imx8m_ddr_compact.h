/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __IMX8M_DDR_COMPACT_H
#define __IMX8M_DDR_COMPACT_H

#include <asm/arch/ddr.h>

/* Packed little-endian entries: DDRC offset16/value32, or PHY reg24/value16. */
struct dram_compact_table {
	const u8 *data;
	const u8 *base;
	u16 count;
	u16 size;
	u16 base_count;
	u8 width;
};

struct dram_compact_fsp_msg {
	u32 drate;
	enum fw_type fw_type;
	const struct dram_compact_table *cfg;
};

/* Separate from the public, expanded dram_timing_info suspend/resume ABI. */
struct dram_compact_timing {
	const struct dram_compact_table *ddrc_cfg;
	const struct dram_compact_table *ddrphy_cfg;
	const struct dram_compact_table *ddrphy_pie;
	const struct dram_compact_fsp_msg *fsp_msg;
	u32 fsp_msg_num;
	u32 fsp_table[4];
};

struct dram_compact_iter {
	const struct dram_compact_table *table;
	const u8 *cursor;
	const u8 *run;
	u16 remaining;
	u16 emitted;
};

void ddr_compact_iter_init(struct dram_compact_iter *it,
			   const struct dram_compact_table *table);
int ddr_compact_next(struct dram_compact_iter *it, struct dram_cfg_param *entry);
int ddr_compact_write(const struct dram_compact_table *table, bool phy);
int ddr_compact_save(const struct dram_compact_timing *timing, ulong address);
int ddr_init_compact(const struct dram_compact_timing *timing);
int ddr_init_mrr_compact(const struct dram_compact_timing *timing,
			 unsigned int fsp);
int ddr_cfg_phy_compact(struct dram_timing_info *timing,
			const struct dram_compact_timing *compact, int probe_fsp);

#endif
