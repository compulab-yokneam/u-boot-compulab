/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2025 NXP
 */

#ifndef __UCM_IMX952_H
#define __UCM_IMX952_H

#include <linux/sizes.h>
#include <linux/stringify.h>
#include <asm/arch/imx-regs.h>

#define CFG_SYS_INIT_RAM_ADDR	0x90000000
#define CFG_SYS_INIT_RAM_SIZE	0x200000

#define CFG_SYS_SDRAM_BASE		0x90000000
#define PHYS_SDRAM			0x90000000

#define PHYS_SDRAM_SIZE			0x70000000 /* 2GB - 256MB DDR */

#ifdef CONFIG_DRAM_D16
#define PHYS_SDRAM_2_SIZE       0x380000000 /* 14GB (Totally 16GB) */
#endif

#ifdef CONFIG_DRAM_D8
#define PHYS_SDRAM_2_SIZE       0x180000000 /* 6GB (Totally 8GB) */
#endif

#ifdef CONFIG_DRAM_D4
#define PHYS_SDRAM_2_SIZE       0x080000000 /* 2GB (Totally 4GB) */
#endif

#define CFG_SYS_SECURE_SDRAM_BASE	0x8A000000 /* Secure DDR region for A55, SPL could use first 2MB */
#define CFG_SYS_SECURE_SDRAM_SIZE	0x06000000

#define WDOG_BASE_ADDR			WDG3_BASE_ADDR

#ifdef CONFIG_ANDROID_SUPPORT
#include "ucm-imx952_android.h"
#endif

#endif
