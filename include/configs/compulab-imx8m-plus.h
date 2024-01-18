/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 CompuLab
 */

#ifndef __COMPULAB_IMX8M_PLUS
#define __COMPULAB_IMX8M_PLUS

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#include "imx_env.h"

#define CFG_SYS_UBOOT_BASE	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#define CONFIG_SPL_STACK		0x96dff0
#define SHARED_DDR_INFO			CONFIG_SPL_STACK

#ifdef CONFIG_DISTRO_DEFAULTS
#define BOOT_TARGET_DEVICES(func) \
	func(USB, usb, 0) \
	func(MMC, mmc, 1) \
	func(MMC, mmc, 2)

#include <config_distro_bootcmd.h>
#else
#define BOOTENV
#endif

#define CFG_MFG_ENV_SETTINGS \
	CFG_MFG_ENV_SETTINGS_DEFAULT \
	"initrd_addr=0x43800000\0" \
	"initrd_high=0xffffffffffffffff\0" \
	"emmc_dev=2\0"\
	"sd_dev=1\0" \

/* Initial environment variables */
#define CFG_EXTRA_ENV_SETTINGS		\
	CFG_MFG_ENV_SETTINGS \
	BOOTENV \
	"video_link=1\0" \
	"stdout=serial,vidconsole\0" \
	"stderr=serial,vidconsole\0" \
	"stdin=serial,usbkbd\0" \
	"autoload=off\0" \
	"scriptaddr=0x43500000\0" \
	"kernel_addr_r=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
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
	"mmcargs=setenv bootargs console=${console} root=${mmcroot}\0 " \
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
	"netargs=setenv bootargs console=${console} " \
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
		"ulbootscript=load ${iface} ${dev}:${part} ${loadaddr} ${script};\0" \
		"ulimage=echo loading ${image}; load ${iface} ${dev}:${part} ${loadaddr} ${image}\0" \
		"ulfdto=setenv fdto1file; for fdto1file in ${fdtofile}; do "\
			    "echo loading ${fdto1file}; "\
			    "load ${iface} ${dev}:${part} ${fdto_addr_r} ${fdto1file} "\
			    "&& fdt addr ${fdt_addr_r} "\
			    "&& fdt resize 0x8000 "\
			    "&& fdt apply ${fdto_addr_r};"\
		"done; true;\0"\
		"ulfdt=if test ${boot_fdt} = yes || test ${boot_fdt} = try; then " \
			"echo loading ${fdtfile}; load ${iface} ${dev}:${part} ${fdt_addr_r} ${fdtfile}; " \
			"if env exists fdtofile;then "\
					"run ulfdto; "\
				"else "\
					"true; "\
				"fi; "\
		"fi;\0" \
		"bootlist=usb_ul sd_ul emmc_ul\0" \
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
#endif

#define CONFIG_MMCROOT			"/dev/mmcblk1p2"  /* USDHC2 */

#define CFG_SYS_INIT_RAM_ADDR	0x40000000
#define CFG_SYS_INIT_RAM_SIZE	0x80000

/* Totally 6GB DDR */
#define CFG_SYS_SDRAM_BASE		0x40000000
#define PHYS_SDRAM			0x40000000
#define PHYS_SDRAM_SIZE			0x80000000	/* 2 GB */
#define PHYS_SDRAM_2			0x100000000
#define PHYS_SDRAM_2_SIZE		0x00000000	/* 0 GB */

#define CFG_MXC_UART_BASE		UART2_BASE_ADDR

#define CFG_SYS_FSL_USDHC_NUM	2

#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#ifdef CONFIG_SYS_PROMPT
#undef CONFIG_SYS_PROMPT
#define CONFIG_SYS_PROMPT CONFIG_SYS_BOARD"=> "
#endif
