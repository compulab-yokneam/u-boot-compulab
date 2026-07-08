/*
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Base-profile + diff/patch storage for the LPDDR4 timing tables.
 *
 * Each `struct dram_timing_info` sub-table (ddrc_cfg, ddrphy_cfg, the four
 * fsp*_cfg tables, ddrphy_pie) is stored once as a shared "base" array plus,
 * per DRAM profile, a short list of patches (set/insert/delete against the
 * base). The patches are merged into scratch buffers at runtime, immediately
 * before ddr_init() is called, since ddr_init()/dram_config_save() need all
 * of them resident simultaneously.
 */

#ifndef __DDR_PATCH_H__
#define __DDR_PATCH_H__

#include <asm/arch/imx8m_ddr.h>

enum dram_cfg_patch_op {
	DRAM_PATCH_SET,		/* replace base[pos] with {reg,val} */
	DRAM_PATCH_INSERT,	/* emit {reg,val} just before base[pos] */
	DRAM_PATCH_DELETE,	/* drop base[pos] entirely */
};

struct dram_cfg_patch {
	unsigned short pos;	/* index into the base array this op targets */
	unsigned char op;	/* enum dram_cfg_patch_op */
	unsigned char _pad;
	unsigned int reg;	/* SET/INSERT only */
	unsigned int val;	/* SET/INSERT only */
};

/*
 * patch[] must be sorted by non-decreasing pos; ops sharing the same pos
 * must order any INSERTs before the (at most one) SET/DELETE for that pos.
 * This is the natural emission order of a sequence diff, enforced by
 * gen_ddr_tables.py, and lets dram_cfg_apply_patches() do a single
 * ascending pass with no runtime sort.
 */
struct dram_cfg_patchset {
	const struct dram_cfg_param *base;
	unsigned int base_num;
	const struct dram_cfg_patch *patch;
	unsigned int patch_num;
};

struct dram_profile_desc {
	struct dram_cfg_patchset ddrc_cfg;
	struct dram_cfg_patchset ddrphy_cfg;
	struct dram_cfg_patchset fsp0_cfg;
	struct dram_cfg_patchset fsp1_cfg;
	struct dram_cfg_patchset fsp2_cfg;
	struct dram_cfg_patchset fsp0_2d_cfg;
	struct dram_cfg_patchset ddrphy_pie;
	unsigned int fsp_table[4];
};

unsigned int dram_cfg_apply_patches(const struct dram_cfg_patchset *ps,
				     struct dram_cfg_param *out);

void dram_profile_build_timing(const struct dram_profile_desc *profile,
				struct dram_timing_info *out);

#endif /* __DDR_PATCH_H__ */
