/*
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __DDR_H__
#define __DDR_H__

extern struct dram_timing_info ucm_dram_timing_ff020008;
extern struct dram_timing_info ucm_dram_timing_ff000110;
extern struct dram_timing_info ucm_dram_timing_01061010;
extern struct dram_timing_info ucm_dram_timing_01050008;
extern struct dram_timing_info ucm_dram_timing_05000010;

void spl_dram_init(void);

#define TCM_DATA_CFG 0x7e0000

struct lpddr4_tcm_desc {
	unsigned int size;
	unsigned int sign;
	unsigned int index;
	unsigned int count;
};

#endif
