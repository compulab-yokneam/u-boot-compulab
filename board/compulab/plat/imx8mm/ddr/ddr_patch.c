/*
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <linux/kernel.h>
#include <asm/arch/imx8m_ddr.h>
#include "ddr_patch.h"
#include "ddr_cfg_sizes.h"

unsigned int dram_cfg_apply_patches(const struct dram_cfg_patchset *ps,
				     struct dram_cfg_param *out)
{
	unsigned int bi = 0, pi = 0, oi = 0;

	while (bi < ps->base_num || pi < ps->patch_num) {
		while (pi < ps->patch_num && ps->patch[pi].pos <= bi) {
			const struct dram_cfg_patch *p = &ps->patch[pi];

			if (p->op != DRAM_PATCH_DELETE) {
				out[oi].reg = p->reg;
				out[oi].val = p->val;
				oi++;
			}
			if (p->op != DRAM_PATCH_INSERT)
				bi++;
			pi++;
		}
		if (bi < ps->base_num)
			out[oi++] = ps->base[bi++];
	}
	return oi;
}

/*
 * ddr_init()/dram_config_save() need ddrc_cfg, ddrphy_cfg, every fsp_cfg and
 * ddrphy_pie resident simultaneously for the whole ddr_init() call, so these
 * scratch buffers can't be reused one-at-a-time; they must all coexist.
 * Sized to the worst case across the active profile set (ddr_cfg_sizes.h).
 */
static struct dram_cfg_param scratch_ddrc_cfg[DRAM_DDRC_CFG_MAX];
static struct dram_cfg_param scratch_ddrphy_cfg[DRAM_DDRPHY_CFG_MAX];
static struct dram_cfg_param scratch_fsp0_cfg[DRAM_FSP0_CFG_MAX];
static struct dram_cfg_param scratch_fsp1_cfg[DRAM_FSP1_CFG_MAX];
static struct dram_cfg_param scratch_fsp2_cfg[DRAM_FSP2_CFG_MAX];
static struct dram_cfg_param scratch_fsp0_2d_cfg[DRAM_FSP0_2D_CFG_MAX];
static struct dram_cfg_param scratch_ddrphy_pie[DRAM_DDRPHY_PIE_MAX];
static struct dram_fsp_msg scratch_fsp_msg[4];

void dram_profile_build_timing(const struct dram_profile_desc *profile,
				struct dram_timing_info *out)
{
	int i;

	out->ddrc_cfg = scratch_ddrc_cfg;
	out->ddrc_cfg_num = dram_cfg_apply_patches(&profile->ddrc_cfg,
						    scratch_ddrc_cfg);

	out->ddrphy_cfg = scratch_ddrphy_cfg;
	out->ddrphy_cfg_num = dram_cfg_apply_patches(&profile->ddrphy_cfg,
						      scratch_ddrphy_cfg);

	out->ddrphy_pie = scratch_ddrphy_pie;
	out->ddrphy_pie_num = dram_cfg_apply_patches(&profile->ddrphy_pie,
						      scratch_ddrphy_pie);

	/* drate/fw_type are identical across every profile today */
	scratch_fsp_msg[0] = (struct dram_fsp_msg){
		.drate = profile->fsp_table[0],
		.fw_type = FW_1D_IMAGE,
		.fsp_cfg = scratch_fsp0_cfg,
		.fsp_cfg_num = dram_cfg_apply_patches(&profile->fsp0_cfg,
						       scratch_fsp0_cfg),
	};
	scratch_fsp_msg[1] = (struct dram_fsp_msg){
		.drate = profile->fsp_table[1],
		.fw_type = FW_1D_IMAGE,
		.fsp_cfg = scratch_fsp1_cfg,
		.fsp_cfg_num = dram_cfg_apply_patches(&profile->fsp1_cfg,
						       scratch_fsp1_cfg),
	};
	scratch_fsp_msg[2] = (struct dram_fsp_msg){
		.drate = profile->fsp_table[2],
		.fw_type = FW_1D_IMAGE,
		.fsp_cfg = scratch_fsp2_cfg,
		.fsp_cfg_num = dram_cfg_apply_patches(&profile->fsp2_cfg,
						       scratch_fsp2_cfg),
	};
	scratch_fsp_msg[3] = (struct dram_fsp_msg){
		.drate = profile->fsp_table[0],
		.fw_type = FW_2D_IMAGE,
		.fsp_cfg = scratch_fsp0_2d_cfg,
		.fsp_cfg_num = dram_cfg_apply_patches(&profile->fsp0_2d_cfg,
						       scratch_fsp0_2d_cfg),
	};

	out->fsp_msg = scratch_fsp_msg;
	out->fsp_msg_num = ARRAY_SIZE(scratch_fsp_msg);

	for (i = 0; i < 4; i++)
		out->fsp_table[i] = profile->fsp_table[i];
}
