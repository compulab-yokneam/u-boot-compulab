/*
 * Copyright 2021 Compulab Ltd.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __LPDDR_TIMING_BLOCK_H__
#define __LPDDR_TIMING_BLOCK_H_

struct lpddr4_timing_block {
	/* Data, read from MR5-8, DRAM chip identifier, assigned by a vendor */
	unsigned long id;
	/* An optional field to distiguish DRAM chips that
	 * have different geometry, though return the same MRR.
	 * Default value 0xff
	 */
	u8	subind;
	char name[16]; // Name to be displayed
	unsigned int size; // Total size, MB
	struct dram_cfg_param ddr_ddrc_cfg[103];
	struct dram_cfg_param ddr_ddrphy_cfg[200];
	struct dram_cfg_param ddr_ddrphy_trained_csr[719];
	struct dram_cfg_param ddr_fsp0_cfg [35];
	struct dram_cfg_param ddr_fsp1_cfg [36];
	struct dram_cfg_param ddr_fsp2_cfg [36];
	struct dram_cfg_param ddr_fsp0_2d_cfg [36];
	struct dram_cfg_param ddr_phy_pie [592];
	struct dram_fsp_msg ddr_dram_fsp_msg[4];
	struct dram_timing_info dram_timing;
};
#endif
