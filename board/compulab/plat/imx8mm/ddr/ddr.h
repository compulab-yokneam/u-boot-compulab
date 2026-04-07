/*
 * Copyright 2021 Compulab Ltd.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __DDR_H__
#define __DDR_H__

void spl_dram_init(void);

#define TCM_DATA_CFG 0x7e0000

struct lpddr4_tcm_desc {
	unsigned int size;
	unsigned int sign; // DDR quirq, read from MRR
	unsigned int index;
	unsigned int timing; // A sign of timing block, the DDR was trained with
};

#endif
