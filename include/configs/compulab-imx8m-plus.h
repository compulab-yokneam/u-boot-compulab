/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 CompuLab
 */

#ifndef __COMPULAB_IMX8M_PLUS
#define __COMPULAB_IMX8M_PLUS

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#include "imx_env.h"

#define CONFIG_SYS_BOOTM_LEN		(32 * SZ_1M)

#define CONFIG_SPL_MAX_SIZE		(152 * 1024)
#define CONFIG_SYS_MONITOR_LEN		(512 * 1024)
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	0x300
#define CONFIG_SYS_UBOOT_BASE	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#define SPL_STACK_ADDR			0x96dff0
#define SHARED_DDR_INFO			SPL_STACK_ADDR
#ifdef CONFIG_SPL_BUILD
#define CONFIG_SPL_STACK		SPL_STACK_ADDR
#define CONFIG_SPL_BSS_START_ADDR	0x96e000
#define CONFIG_SPL_BSS_MAX_SIZE		SZ_4K	/* 4 KB */
#define CONFIG_SYS_SPL_MALLOC_START	0x42200000
#define CONFIG_SYS_SPL_MALLOC_SIZE	SZ_256K	/* 256 KB */

/* For RAW image gives a error info not panic */
#define CONFIG_SPL_ABORT_ON_RAW_IMAGE

#endif

#define CONFIG_CMD_READ
#define CONFIG_SERIAL_TAG
#define CONFIG_FASTBOOT_USB_DEV 0

#define CONFIG_REMAKE_ELF

#ifdef CONFIG_DISTRO_DEFAULTS
#define BOOT_TARGET_DEVICES(func) \
	func(USB, usb, 0) \
	func(MMC, mmc, 1) \
	func(MMC, mmc, 2)

#include <config_distro_bootcmd.h>
#else
#define BOOTENV
#endif

#ifndef MACHINE_EXTRA_ENV_SETTINGS
#define MACHINE_EXTRA_ENV_SETTINGS
#endif

#define JAILHOUSE_ENV \
	"jh_clk= \0 " \
	"jh_mmcboot=setenv fdtfile ucm-imx8m-plus-root.dtb;" \
		"setenv jh_clk clk_ignore_unused mem=2048MB; " \
			   "if run loadimage; then " \
				   "run mmcboot; " \
			   "else run jh_netboot; fi; \0" \
	"jh_netboot=setenv fdtfile ucm-imx8m-plus-root.dtb; setenv jh_clk clk_ignore_unused mem=2048MB; run netboot; \0 "

#define CONFIG_MFG_ENV_SETTINGS \
	CONFIG_MFG_ENV_SETTINGS_DEFAULT \
	"initrd_addr=0x43800000\0" \
	"initrd_high=0xffffffffffffffff\0" \
	"emmc_dev=2\0"\
	"sd_dev=1\0" \

/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS		\
	CONFIG_MFG_ENV_SETTINGS \
	JAILHOUSE_ENV \
	BOOTENV \
	MACHINE_EXTRA_ENV_SETTINGS \
	"stdout=serial,vidconsole\0" \
	"stderr=serial,vidconsole\0" \
	"stdin=serial,usbkbd\0" \
	"autoload=off\0" \
	"scriptaddr=0x43500000\0" \
	"kernel_addr_r=" __stringify(CONFIG_LOADADDR) "\0" \
	"bsp_script=boot.scr\0" \
	"image=Image\0" \
	"splashimage=0x50000000\0" \
	"console=ttymxc1,115200 console=tty1\0" \
	"fdt_addr_r=0x43000000\0"			\
	"fdto_addr_r=0x43800000\0"			\
	"fdt_addr=0x43000000\0"			\
	"boot_fdt=try\0" \
	"fdt_high=0xffffffffffffffff\0"		\
	"boot_fit=no\0" \
	"fdtfile=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"bootm_size=0x10000000\0" \
	"mmcdev="__stringify(CONFIG_SYS_MMC_ENV_DEV)"\0" \
	"mmcpart=" __stringify(CONFIG_SYS_MMC_IMG_LOAD_PART) "\0" \
	"mmcroot=" CONFIG_MMCROOT " rootwait rw\0" \
	"mmcautodetect=yes\0" \
	"mmcargs=setenv bootargs ${jh_clk} console=${console} root=${mmcroot}\0 " \
	"loadbootscript=load mmc ${mmcdev}:${mmcpart} ${loadaddr} ${bsp_script};\0" \
	"bootscript=echo Running bootscript from mmc ...; " \
		"source\0" \
	"loadimage=load mmc ${mmcdev}:${mmcpart} ${loadaddr} ${image}\0" \
	"loadfdt=load mmc ${mmcdev}:${mmcpart} ${fdt_addr_r} ${fdtfile}\0" \
	"mmcboot=echo Booting from mmc ...; " \
		"run mmcargs; " \
		"if test ${boot_fit} = yes || test ${boot_fit} = try; then " \
			"bootm ${loadaddr}; " \
		"else " \
			"if run loadfdt; then " \
				"booti ${loadaddr} - ${fdt_addr_r}; " \
			"else " \
				"echo WARN: Cannot load the DT; " \
			"fi; " \
		"fi;\0" \
	"netargs=setenv bootargs ${jh_clk} console=${console} " \
		"root=/dev/nfs " \
		"ip=dhcp nfsroot=${serverip}:${nfsroot},v3,tcp\0" \
	"netboot=echo Booting from net ...; " \
		"run netargs;  " \
		"if test ${ip_dyn} = yes; then " \
			"setenv get_cmd dhcp; " \
		"else " \
			"setenv get_cmd tftp; " \
		"fi; " \
		"${get_cmd} ${loadaddr} ${image}; " \
		"if test ${boot_fit} = yes || test ${boot_fit} = try; then " \
			"bootm ${loadaddr}; " \
		"else " \
			"if ${get_cmd} ${fdt_addr_r} ${fdtfile}; then " \
				"booti ${loadaddr} - ${fdt_addr_r}; " \
			"else " \
				"echo WARN: Cannot load the DT; " \
			"fi; " \
		"fi;\0" \
		"emmc_root=/dev/mmcblk2p2\0" \
		"sd_root=/dev/mmcblk1p2\0" \
		"usb_root=/dev/sda2\0" \
		"usb_dev=0\0" \
		"boot_part=1\0" \
		"root_opt=rootwait rw\0" \
		"emmc_ul=setenv iface mmc; setenv dev ${emmc_dev}; setenv part ${boot_part};" \
		"setenv bootargs console=${console} root=${emmc_root} ${root_opt};\0" \
		"sd_ul=setenv iface mmc; setenv dev ${sd_dev}; setenv part ${boot_part};" \
			"setenv bootargs console=${console} root=${sd_root} ${root_opt};\0" \
		"usb_ul=usb start; setenv iface usb; setenv dev ${usb_dev}; setenv part ${boot_part};" \
			"setenv bootargs console=${console} root=${usb_root} ${root_opt};\0" \
		"ulbootscript=load ${iface} ${dev}:${part} ${scriptaddr} ${bsp_script};\0" \
		"ulrunbootscript=echo Running bootscript from ${iface} ${dev}:${part} ...; " \
				"source ${scriptaddr}\0" \
		"ulimage=load ${iface} ${dev}:${part} ${loadaddr} ${image}\0" \
		"ulfdt=if test ${boot_fdt} = yes || test ${boot_fdt} = try; then " \
			"load ${iface} ${dev}:${part} ${fdt_addr_r} ${fdtfile}; " \
			"if itest.s x != x${fdtofile}; then " \
			    "load ${iface} ${dev}:${part} ${fdto_addr_r} ${fdtofile};" \
			    "fdt addr ${fdt_addr_r}; fdt resize 0x8000; fdt apply ${fdto_addr_r};" \
			"else " \
			    "true;" \
			"fi;" \
		"fi;\0" \
		"bootlist=usb_ul sd_ul emmc_ul\0" \
		"bsp_bootcmd=echo Running BSP bootcmd ...; " \
		"for src in ${bootlist}; do " \
			"echo Running ${src} ...; " \
			"run ${src}; " \
			"env exist boot_opt && env exists bootargs && setenv bootargs ${bootargs} ${boot_opt}; " \
			"if run ulbootscript; then " \
				"run ulrunbootscript; " \
			"fi; " \
			"if run ulimage; then " \
				"if run ulfdt; then " \
					"booti ${loadaddr} - ${fdt_addr_r}; " \
				"else " \
					"if test ${boot_fdt} != yes; then " \
						"booti ${loadaddr}; " \
					"fi; " \
				"fi; " \
			"fi; " \
		"done; "
#endif

/* Link Definitions */
#define CONFIG_LOADADDR			0x40480000

#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR

#define CONFIG_SYS_INIT_RAM_ADDR	0x40000000
#define CONFIG_SYS_INIT_RAM_SIZE	0x80000
#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

#define CONFIG_ENV_OVERWRITE
#define CONFIG_MMCROOT			"/dev/mmcblk1p2"  /* USDHC2 */

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		SZ_32M

/* Totally 6GB DDR */
#define CONFIG_SYS_SDRAM_BASE		0x40000000
#define PHYS_SDRAM			0x40000000
#define PHYS_SDRAM_SIZE			0x80000000	/* 2 GB */
#define PHYS_SDRAM_2			0x100000000
#define PHYS_SDRAM_2_SIZE		0x00000000	/* 0 GB */

#define CONFIG_MXC_UART_BASE		UART2_BASE_ADDR

/* Monitor Command Prompt */
#define CONFIG_SYS_PROMPT_HUSH_PS2	"> "
#define CONFIG_SYS_CBSIZE		2048
#define CONFIG_SYS_MAXARGS		64
#define CONFIG_SYS_BARGSIZE CONFIG_SYS_CBSIZE
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)

#define CONFIG_IMX_BOOTAUX
#define CONFIG_FSL_USDHC

#define CONFIG_SYS_FSL_USDHC_NUM	2
#define CONFIG_SYS_FSL_ESDHC_ADDR	0

#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#ifdef CONFIG_NAND_MXS
#define CONFIG_CMD_NAND_TRIMFFS

/* NAND stuff */
#define CONFIG_SYS_MAX_NAND_DEVICE     1
#define CONFIG_SYS_NAND_BASE           0x20000000
#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define CONFIG_SYS_NAND_USE_FLASH_BBT
#endif /* CONFIG_NAND_MXS */

#define CONFIG_SYS_I2C_SPEED		100000

/* USB configs */
#ifndef CONFIG_SPL_BUILD

#define CONFIG_CMD_USB_MASS_STORAGE
#define CONFIG_USB_GADGET_MASS_STORAGE
#define CONFIG_USB_FUNCTION_MASS_STORAGE
#endif

#define CONFIG_USB_MAX_CONTROLLER_COUNT         2
#define CONFIG_USBD_HS
#define CONFIG_USB_GADGET_VBUS_DRAW 2

#ifdef CONFIG_DM_VIDEO
#define CONFIG_VIDEO_LOGO
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_CMD_BMP
#define CONFIG_BMP_16BPP
#define CONFIG_BMP_24BPP
#define CONFIG_BMP_32BPP
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_VIDEO_BMP_LOGO
#endif

#ifdef CONFIG_SYS_PROMPT
#undef CONFIG_SYS_PROMPT
#define CONFIG_SYS_PROMPT CONFIG_SYS_BOARD"=> "
#endif
