/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 C-Lab
 */

#ifndef __COMPULAB_IMX93_H
#define __COMPULAB_IMX93_H

#include <linux/sizes.h>
#include <linux/stringify.h>
#include <asm/arch/imx-regs.h>
#include "imx_env.h"

/*#define CONFIG_SYS_BOOTM_LEN		(SZ_64M)*/
/*#define CONFIG_SPL_MAX_SIZE		(148 * 1024)*/
/*#define CONFIG_SYS_MONITOR_LEN		SZ_512K*/
#define CONFIG_SYS_UBOOT_BASE	\
	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SPL_STACK		0x20519dd0
#define CONFIG_SPL_BSS_START_ADDR	0x2051a000
/*#define CONFIG_SPL_BSS_MAX_SIZE	SZ_8K */
#define CONFIG_SYS_SPL_MALLOC_START	0x83200000 /* Need disable simple malloc where still uses malloc_f area */
#define CONFIG_SYS_SPL_MALLOC_SIZE	SZ_512K	/* 512 KB */

/* For RAW image gives a error info not panic */
#define CONFIG_SPL_ABORT_ON_RAW_IMAGE

#endif

/*#define CONFIG_CMD_READ*/
#define CONFIG_SERIAL_TAG

#ifdef CONFIG_AHAB_BOOT
#define AHAB_ENV "sec_boot=yes\0"
#else
#define AHAB_ENV "sec_boot=no\0"
#endif

#ifdef CONFIG_DISTRO_DEFAULTS
#define BOOT_TARGET_DEVICES(func) \
	func(USB, usb, 0) \
	func(MMC, mmc, 1) \
	func(MMC, mmc, 0)

#include <config_distro_bootcmd.h>
#else
#define BOOTENV
#endif

#define JAILHOUSE_ENV \
	"jh_mmcboot=setenv fdtfile ucm-imx93-root.dtb; " \
		    "setenv jh_clk clk_ignore_unused mem=1248MB kvm-arm.mode=nvhe; " \
		    "if run loadimage; then run mmcboot;" \
		    "else run jh_netboot; fi; \0" \
	"jh_netboot=setenv fdtfile ucm-imx93-root.dtb; " \
		    "setenv jh_clk clk_ignore_unused mem=1248MB kvm-arm.mode=nvhe; run netboot; \0 "

#define CONFIG_MFG_ENV_SETTINGS \
	CONFIG_MFG_ENV_SETTINGS_DEFAULT \
	"initrd_addr=0x83800000\0" \
	"initrd_high=0xffffffffffffffff\0" \
	"emmc_dev=0\0"\
	"sd_dev=1\0" \

/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS		\
	JAILHOUSE_ENV \
	CONFIG_MFG_ENV_SETTINGS \
	BOOTENV \
	AHAB_ENV \
	"autoload=off\0" \
	"scriptaddr=0x83500000\0" \
	"kernel_addr_r=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"image=Image\0" \
	"splashimage=0x90000000\0" \
	"console=ttyLP0,115200 earlycon\0" \
	"fdt_addr_r=0x83000000\0"			\
	"fdto_addr_r=0x83800000\0"			\
	"fdt_addr=0x83000000\0"			\
	"fdt_high=0xffffffffffffffff\0"		\
	"cntr_addr=0x98000000\0"			\
	"cntr_file=os_cntr_signed.bin\0" \
	"boot_fit=no\0" \
	"fdtfile=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"bootm_size=0x10000000\0" \
	"mmcdev="__stringify(CONFIG_SYS_MMC_ENV_DEV)"\0" \
	"mmcpart=1\0" \
	"mmcroot=" CONFIG_MMCROOT " rootwait rw\0" \
	"mmcautodetect=yes\0" \
	"mmcargs=setenv bootargs ${jh_clk} console=${console} root=${mmcroot}\0 " \
	"loadbootscript=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${script};\0" \
	"bootscript=echo Running bootscript from mmc ...; " \
		"source\0" \
	"loadimage=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${image}\0" \
	"loadfdt=fatload mmc ${mmcdev}:${mmcpart} ${fdt_addr_r} ${fdtfile}\0" \
	"loadcntr=fatload mmc ${mmcdev}:${mmcpart} ${cntr_addr} ${cntr_file}\0" \
	"auth_os=auth_cntr ${cntr_addr}\0" \
	"boot_os=booti ${loadaddr} - ${fdt_addr_r};\0" \
	"mmcboot=echo Booting from mmc ...; " \
		"run mmcargs; " \
		"if test ${sec_boot} = yes; then " \
			"if run auth_os; then " \
				"run boot_os; " \
			"else " \
				"echo ERR: failed to authenticate; " \
			"fi; " \
		"else " \
			"if test ${boot_fit} = yes || test ${boot_fit} = try; then " \
				"bootm ${loadaddr}; " \
			"else " \
				"if run loadfdt; then " \
					"run boot_os; " \
				"else " \
					"echo WARN: Cannot load the DT; " \
				"fi; " \
			"fi;" \
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
		"if test ${sec_boot} = yes; then " \
			"${get_cmd} ${cntr_addr} ${cntr_file}; " \
			"if run auth_os; then " \
				"run boot_os; " \
			"else " \
				"echo ERR: failed to authenticate; " \
			"fi; " \
		"else " \
			"${get_cmd} ${loadaddr} ${image}; " \
			"if test ${boot_fit} = yes || test ${boot_fit} = try; then " \
				"bootm ${loadaddr}; " \
			"else " \
				"if ${get_cmd} ${fdt_addr_r} ${fdtfile}; then " \
					"run boot_os; " \
				"else " \
					"echo WARN: Cannot load the DT; " \
				"fi; " \
			"fi;" \
		"fi;\0" \
		"emmc_root=/dev/mmcblk0p2\0" \
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
		"ulbootscript=load ${iface} ${dev}:${part} ${loadaddr} ${script};\0" \
		"ulimage=load ${iface} ${dev}:${part} ${loadaddr} ${image}\0" \
		"ulfdt=if test ${boot_fdt} = yes || test ${boot_fdt} = try; then " \
			"echo load ${iface} ${dev}:${part} ${fdt_addr_r} ${fdtfile}; " \
			"load ${iface} ${dev}:${part} ${fdt_addr_r} ${fdtfile}; " \
				"if itest.s x != x${fdtofile}; then " \
				    "load ${iface} ${dev}:${part} ${fdto_addr_r} ${fdtofile};"\
				    "fdt addr ${fdt_addr_r}; fdt resize 0x8000; fdt apply ${fdto_addr_r};" \
				"else " \
				    "true;" \
				"fi;" \
			"fi;\0" \
		"bootlist=sd_ul usb_ul emmc_ul\0" \
	"bsp_bootcmd=echo Running BSP bootcmd ...; " \
		"for src in ${bootlist}; do " \
			"run ${src}; " \
			"env exist boot_opt && env exists bootargs && setenv bootargs ${bootargs} ${boot_opt}; " \
			"if run ulbootscript; then " \
				"run bootscript; " \
			"else " \
				"if run ulimage; then " \
					"if run ulfdt; then " \
						"booti ${loadaddr} - ${fdt_addr_r}; " \
					"else " \
						"if test ${boot_fdt} != yes; then " \
							"booti ${loadaddr}; " \
						"fi; " \
					"fi; " \
				"fi; " \
			"fi; " \
		"done; "

/* Link Definitions */

#define CONFIG_SYS_INIT_RAM_ADDR        0x80000000
#define CONFIG_SYS_INIT_RAM_SIZE        0x200000
#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

#define CONFIG_MMCROOT			"/dev/mmcblk1p2"  /* USDHC2 */

#define CFG_SYS_INIT_RAM_ADDR        0x80000000
#define CFG_SYS_INIT_RAM_SIZE        0x200000

#define CFG_SYS_SDRAM_BASE           0x80000000
#define PHYS_SDRAM                      0x80000000
#ifdef CONFIG_IMX9_DRAM_INLINE_ECC
#define PHYS_SDRAM_SIZE			0x70000000 /* 1/8 DDR is used by ECC */
#else
#define PHYS_SDRAM_SIZE			0x80000000 /* 2GB DDR */
#endif

/* Monitor Command Prompt */
#define CONFIG_SYS_CBSIZE		2048
#define CONFIG_SYS_MAXARGS		64
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
/*#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)*/

#define CONFIG_IMX_BOOTAUX

#define CONFIG_SYS_FSL_USDHC_NUM	2

/* Using ULP WDOG for reset */
#define WDOG_BASE_ADDR          WDG3_BASE_ADDR

#define CONFIG_SYS_I2C_SPEED		100000

/* USB configs */
#define CONFIG_USB_MAX_CONTROLLER_COUNT         2

#if defined(CONFIG_CMD_NET)
#define CONFIG_ETHPRIME                 "eth0"
#define CONFIG_FEC_XCV_TYPE             RGMII
#define PHY_ANEG_TIMEOUT 20000

#endif

/* EEPROM */
#define CONFIG_SYS_I2C_EEPROM_ADDR_LEN	1

#define CONFIG_SYS_I2C_EEPROM_BUS	0
#define CONFIG_SYS_I2C_EEPROM_ADDR	0x50

#define CONFIG_SYS_I2C_EEPROM_BUS_SB	2
#define CONFIG_SYS_I2C_EEPROM_ADDR_SB	0x54

#endif
