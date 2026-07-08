/*
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __DDR_H__
#define __DDR_H__

#include "ddr_patch.h"

extern const struct dram_profile_desc ucm_dram_profile_ff020008;
extern const struct dram_profile_desc ucm_dram_profile_ff000110;
extern const struct dram_profile_desc ucm_dram_profile_01061010;
extern const struct dram_profile_desc ucm_dram_profile_01050008;
extern const struct dram_profile_desc ucm_dram_profile_05000010;
extern const struct dram_profile_desc ucm_dram_profile_1b000008;
extern const struct dram_profile_desc ucm_dram_profile_ff070010;
extern const struct dram_profile_desc ucm_dram_profile_ff070018;
extern const struct dram_profile_desc ucm_dram_profile_ff070110;

void spl_dram_init(void);

#define TCM_DATA_CFG 0x7e0000

struct lpddr4_tcm_desc {
	unsigned int size;
	unsigned int sign;
	unsigned int index;
	unsigned int count;
};

#endif
