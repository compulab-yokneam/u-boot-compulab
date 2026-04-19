/*
 * Copyright 2021 Compulab Ltd.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __LPDDR_TIMING_BLOCK_H__
#define __LPDDR_TIMING_BLOCK_H__
#include <asm/arch-imx8m/ddr.h>

struct timing_desc {
	char	name[16]; // Name, to be displayed
	/* Data, read from MR5-8, DRAM chip identifier, assigned by a vendor */
	u32	id;
	/* An optional field to distiguish DRAM chips that
	 * have different geometry, though return the same MRR.
	 * Default value 0xff
	 */
	u8	subind;
	u64	timing_sign; // A uniq signature of timing block to be loaded for this chip
};

// Array sizes are assigned bigger than required. An actual length is assigned in each timing file individually
#define LPDDR_EMPTY_MAGIC "LPDDREMPTYMAGIC"
#define LPDDR_BLOCK_MAGIC "LPDDRBLOCKMAGIC"	// All magics are the same size
#define LPDDR_SINGLE_MAGIC "LPDDRSINGLMAGIC"
#define _DDR_DDRC_CFG_NUM 110
#define _DDR_DDRPHY_CFG_NUM 210
#define _DDR_DDRPHY_TRAINED_CSR_NUM 730
#define _DDR_FSP_CFG_NUM 40
#define _DDR_PHY_PIE_NUM 600

#define DDR_DRAM_FSP_MSG_NUM 4
struct lpddr4_timing_block {
	char magic[sizeof(LPDDR_BLOCK_MAGIC)];
	int size;
	u32 id;
	// Training payload
	struct dram_cfg_param ddr_ddrc_cfg[_DDR_DDRC_CFG_NUM];
	struct dram_cfg_param ddr_ddrphy_cfg[_DDR_DDRPHY_CFG_NUM];
	struct dram_cfg_param* ddr_ddrphy_trained_csr;
	struct dram_cfg_param ddr_fsp0_cfg[_DDR_FSP_CFG_NUM];
	struct dram_cfg_param ddr_fsp1_cfg[_DDR_FSP_CFG_NUM];
	struct dram_cfg_param ddr_fsp2_cfg[_DDR_FSP_CFG_NUM];
	struct dram_cfg_param ddr_fsp0_2d_cfg[_DDR_FSP_CFG_NUM];
	struct dram_cfg_param ddr_phy_pie[_DDR_PHY_PIE_NUM];
	struct dram_fsp_msg ddr_dram_fsp_msg[DDR_DRAM_FSP_MSG_NUM];
	struct dram_timing_info dram_timing;
	u32 crc32;
};
#endif
