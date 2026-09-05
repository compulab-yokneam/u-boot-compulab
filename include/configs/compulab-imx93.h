/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2022 NXP
 * Copyright 2026 CompuLab
 */

#ifndef __COMPULAB_IMX93_H
#define __COMPULAB_IMX93_H

#include <asm/arch/imx-regs.h>

#define CFG_SYS_UBOOT_BASE \
	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#ifdef CONFIG_ENV_MMC_DEVICE_INDEX
#define COMPULAB_IMX93_MMC_ENV_DEV CONFIG_ENV_MMC_DEVICE_INDEX
#else
#define COMPULAB_IMX93_MMC_ENV_DEV 0
#endif

/* Link definitions inherited from the NXP i.MX93 EVK platform. */
#define CFG_SYS_INIT_RAM_ADDR	0x80000000
#define CFG_SYS_INIT_RAM_SIZE	0x200000

#define CFG_SYS_SDRAM_BASE	0x80000000
#define PHYS_SDRAM		0x80000000
#ifdef CONFIG_IMX9_DRAM_INLINE_ECC
#define PHYS_SDRAM_SIZE		0x70000000
#else
#define PHYS_SDRAM_SIZE		0x80000000
#endif

/* Using ULP WDOG for reset, as on the NXP i.MX93 EVK. */
#define WDOG_BASE_ADDR		WDG3_BASE_ADDR

/* CompuLab carrier EEPROM layout. */
#define CONFIG_SYS_I2C_EEPROM_ADDR_LEN	1
#define CONFIG_SYS_I2C_EEPROM_BUS	0
#define CONFIG_SYS_I2C_EEPROM_ADDR	0x50
#define CONFIG_SYS_I2C_EEPROM_BUS_SB	2
#define CONFIG_SYS_I2C_EEPROM_ADDR_SB	0x54

#endif
