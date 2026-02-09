/*
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Generated code from MX8M_DDR_tool
 * Align with uboot-imx_v2018.03_4.14.78_1.0.0_ga
 */

#include <linux/kernel.h>
#include <asm/arch/imx8m_ddr.h>
#include "lpddr_timing_block.h"
// Termination block
static struct timing_block timing_block_term
__attribute__((section (".data"), used)) = {
	.name = "TheLast",
	.id = 0xffffffff,
};
