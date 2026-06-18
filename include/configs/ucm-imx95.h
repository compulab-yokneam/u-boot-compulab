/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2025 NXP
 */

#ifndef __UCM_IMX95_H
#define __UCM_IMX95_H

#include <linux/sizes.h>
#include <linux/stringify.h>
#include <asm/arch/imx-regs.h>

#define CFG_SYS_INIT_RAM_ADDR        0x90000000
#define CFG_SYS_INIT_RAM_SIZE        0x200000

#define CFG_SYS_SDRAM_BASE           0x90000000
#define PHYS_SDRAM                   0x90000000
/* Totally ???GB */
#define PHYS_SDRAM_SIZE			0x70000000UL /* 2GB  - 256MB DDR */

#ifdef CONFIG_DRAM_D16
#define PHYS_SDRAM_2_SIZE 		0x380000000UL /* 14GB */
#endif

#ifdef CONFIG_DRAM_D8
#define PHYS_SDRAM_2_SIZE 		0x180000000UL /* 4GB temp workaround, should be 8GB */
#endif

#ifdef CONFIG_DRAM_D4
#define PHYS_SDRAM_2_SIZE 		0x080000000UL
#endif

#define CFG_SYS_FSL_USDHC_NUM	2

#define WDOG_BASE_ADDR          WDG3_BASE_ADDR


#ifdef CONFIG_ANDROID_SUPPORT
#include "ucm-imx95_android.h"
#endif

#endif
