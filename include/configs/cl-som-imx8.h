/*
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CL_SOM_IMX8_H
#define __CL_SOM_IMX8_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>
#include "imx_env.h"

#define CONFIG_SPL_MAX_SIZE		(148 * 1024)
#define CONFIG_SYS_MONITOR_LEN		(512 * 1024)
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	(0x300 + CONFIG_SECONDARY_BOOT_SECTOR_OFFSET)
#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1

#ifdef CONFIG_SPL_BUILD
/*#define CONFIG_ENABLE_DDR_TRAINING_DEBUG*/
#define CONFIG_SPL_LDSCRIPT		"arch/arm/cpu/armv8/u-boot-spl.lds"
#define CONFIG_SPL_STACK		0x187FF0
#define CONFIG_SPL_BSS_START_ADDR      0x00180000
#define CONFIG_SPL_BSS_MAX_SIZE        0x2000	/* 8 KB */
#define CONFIG_SYS_SPL_MALLOC_START    0x42200000
#define CONFIG_SYS_SPL_MALLOC_SIZE    0x80000	/* 512 KB */
#define CONFIG_SYS_SPL_PTE_RAM_BASE    0x41580000

#define CONFIG_MALLOC_F_ADDR		0x182000 /* malloc f used before GD_FLG_FULL_MALLOC_INIT set */

#define CONFIG_SPL_ABORT_ON_RAW_IMAGE /* For RAW image gives a error info not panic */

#undef CONFIG_DM_MMC
#undef CONFIG_DM_PMIC
#undef CONFIG_DM_PMIC_PFUZE100

#define CONFIG_SYS_I2C

#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_PFUZE100
#define CONFIG_POWER_PFUZE100_I2C_ADDR 0x08
#endif

#define CONFIG_REMAKE_ELF

/* ENET Config */
/* ENET1 */
#if defined(CONFIG_CMD_NET)
#define CONFIG_ETHPRIME                 "FEC"

#define CONFIG_FEC_XCV_TYPE             RGMII
#define CONFIG_FEC_MXC_PHYADDR          0
#define FEC_QUIRK_ENET_MAC

#define CONFIG_PHY_GIGE
#define IMX_FEC_BASE			0x30BE0000

#define CONFIG_PHYLIB
#define CONFIG_PHY_ATHEROS
#endif

/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS		\
	"script=boot.scr\0" \
	"image=Image\0" \
	"console=ttymxc2,115200 earlycon=ec_imx6q,0x30880000,115200\0" \
	"fdt_addr=0x43000000\0"			\
	"fdt_high=0xffffffffffffffff\0"		\
	"fdt_file="CONFIG_DEFAULT_DTB"\0" \
	"initrd_addr=0x43800000\0"		\
	"initrd_high=0xffffffffffffffff\0" \
	"mmcautodetect=yes\0" \
	"autoload=off\0" \
	"part=1\0" \
	"bootscript=echo Running bootscript from ${iface} ...; source\0"	\
	"iface_boot=if run loadbootscript; then run bootscript; else if run loadimage; then run loadfdt;" \
	" booti ${loadaddr} - ${fdt_addr}; fi; fi;\0"	\
	"iface_args=setenv bootargs console=${console} root=${rootdev} rootwait rw \0"	\
	"loadbootscript=load ${iface} ${dev}:${part} ${loadaddr} ${script}\0"	\
	"loadfdt=load ${iface} ${dev}:${part} ${fdt_addr} ${fdt_file}\0"	\
	"loadimage=load ${iface} ${dev}:${part} ${loadaddr} ${image}\0"	\
	"mmc_boot=setenv iface mmc; run ${iface}_pre; run ${iface}_init; run iface_args; run iface_boot\0"	\
	"mmc_init=mmc rescan\0"	\
	"mmc_pre=setenv iface mmc; setenv dev ${mmcdev}; setenv rootdev /dev/mmcblk${mmcdev}p2\0"	\
	"usb_boot=setenv iface usb; run ${iface}_pre; run ${iface}_init; run iface_args; run iface_boot\0"	\
	"usb_init=usb start\0"	\
	"usb_pre=setenv iface usb; setenv dev 0; setenv rootdev /dev/sda2\0"	\

#define CONFIG_BOOTCOMMAND \
	"setenv mmcdev 1; run mmc_boot; run usb_boot; setenv mmcdev 0; run mmc_boot;"

/* Link Definitions */
#define CONFIG_LOADADDR			0x40480000

#define CONFIG_SYS_LOAD_ADDR           CONFIG_LOADADDR

#define CONFIG_SYS_INIT_RAM_ADDR        0x40000000
#define CONFIG_SYS_INIT_RAM_SIZE        0x80000
#define CONFIG_SYS_INIT_SP_OFFSET \
        (CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
        (CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

#define CONFIG_ENV_OVERWRITE
#define CONFIG_SYS_MMC_ENV_DEV		0   /* USDHC1/eMMC */
#define CONFIG_SYS_MMC_ENV_PART		1   /* boot0 area  */
#define CONFIG_MMCROOT			"/dev/mmcblk0p2"  /* USDHC1 */

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		((CONFIG_ENV_SIZE + (2*1024) + (16*1024)) * 1024)

#define PHYS_SDRAM                      0x40000000
#define PHYS_SDRAM_2			0x100000000

/* This value will be recalculated using the ddr detection code */
#define PHYS_SDRAM_SIZE			0x40000000
#define PHYS_SDRAM_2_SIZE		0x0

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM

#define MEMTEST_DIVIDER   2
#define MEMTEST_NUMERATOR 1

#define CONFIG_BAUDRATE			115200

#define CONFIG_MXC_UART_BASE		UART3_BASE_ADDR

/* Monitor Command Prompt */
#undef CONFIG_SYS_PROMPT
#define CONFIG_SYS_PROMPT		"u-boot=> "
#define CONFIG_SYS_PROMPT_HUSH_PS2     "> "
#define CONFIG_SYS_CBSIZE              2048
#define CONFIG_SYS_MAXARGS             64
#define CONFIG_SYS_BARGSIZE CONFIG_SYS_CBSIZE
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)

#define CONFIG_IMX_BOOTAUX

#define CONFIG_SYS_FSL_USDHC_NUM	2
#define CONFIG_SYS_FSL_ESDHC_ADDR       0

#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#define CONFIG_CMD_FUSE

/* I2C Configs */
#define CONFIG_SYS_I2C_SPEED		  100000

/* USB configs */
#ifndef CONFIG_SPL_BUILD

#define CONFIG_CMD_USB
#define CONFIG_USB_STORAGE

#define CONFIG_CMD_USB_MASS_STORAGE
#define CONFIG_USB_GADGET_MASS_STORAGE
#define CONFIG_USB_FUNCTION_MASS_STORAGE

#define CONFIG_CMD_READ

#endif

#define CONFIG_USB_MAX_CONTROLLER_COUNT         2

#define CONFIG_USBD_HS

#define CPL_NET_DISC_LOGICS

/* Framebuffer */
#ifdef CONFIG_VIDEO
#define CONFIG_VIDEO_IMXDCSS
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_BMP_16BPP
#define CONFIG_VIDEO_LOGO
#define CONFIG_VIDEO_BMP_LOGO
#define CONFIG_IMX_VIDEO_SKIP
#endif

#if defined(CONFIG_ANDROID_SUPPORT)
#include "cl-som-imx8_android.h"
#endif

/* EEPROM */
#define CONFIG_ENV_EEPROM_IS_ON_I2C
#define CONFIG_SYS_EEPROM_PAGE_WRITE_BITS      4
#define CONFIG_SYS_EEPROM_PAGE_WRITE_DELAY_MS  5

#define CONFIG_SYS_I2C_EEPROM_ADDR_LEN	1
#define CONFIG_SYS_EEPROM_SIZE		256
#define CONFIG_SYS_I2C_EEPROM_BUS	1

#endif
