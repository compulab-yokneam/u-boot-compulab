/*
 * Copyright 2020 CompuLab
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CPL_IMX8M_MINI_H
#define __CPL_IMX8M_MINI_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>
#include "imx_env.h"

#define CFG_SYS_UBOOT_BASE	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#ifdef CONFIG_SPL_BUILD

/* malloc f used before GD_FLG_FULL_MALLOC_INIT set */
#define CFG_MALLOC_F_ADDR	0x930000

#endif /*ifdef CONFIG_SPL_BUILD*/

#undef CONFIG_CMD_IMLS

#undef CONFIG_CMD_CRC32
#undef CONFIG_BOOTM_NETBSD

/* ENET Config */
/* ENET1 */
#if defined(CONFIG_CMD_NET)

#define CFG_FEC_XCV_TYPE             RGMII
#define CFG_FEC_MXC_PHYADDR          0
#define FEC_QUIRK_ENET_MAC
#define CONFIG_PHYLIB

#endif

#define CFG_MFG_ENV_SETTINGS \
	"mfgtool_args=setenv bootargs console=${console},${baudrate} " \
		"rdinit=/linuxrc " \
		"g_mass_storage.stall=0 g_mass_storage.removable=1 " \
		"g_mass_storage.idVendor=0x066F g_mass_storage.idProduct=0x37FF "\
		"g_mass_storage.iSerialNumber=\"\" "\
		"clk_ignore_unused "\
		"\0" \
	"initrd_addr=0x43800000\0" \
	"initrd_high=0xffffffff\0" \
	"emmc_dev=1\0" \
	"sd_dev=0\0" \
	"bootcmd_mfg=run mfgtool_args;  if iminfo ${initrd_addr}; then "\
					   "booti ${loadaddr} ${initrd_addr} ${fdt_addr};"\
					"else echo \"Run fastboot ...\"; fastboot 0; fi\0" \
/* Initial environment variables */
#define CFG_EXTRA_ENV_SETTINGS		\
	CFG_MFG_ENV_SETTINGS \
	"autoload=off\0" \
	"script=boot.scr\0" \
	"image=Image\0" \
	"console=ttymxc2,115200 earlycon=ec_imx6q,0x30880000,115200\0" \
	"fdt_addr=0x43000000\0"			\
	"fdt_high=0xffffffffffffffff\0"		\
	"boot_fdt=yes\0" \
	"fdt_file="CONFIG_DEFAULT_FDT"\0" \
	"initrd_addr=0x43800000\0"		\
	"initrd_high=0xffffffffffffffff\0" \
	"mmcdev="__stringify(CONFIG_SYS_MMC_ENV_DEV)"\0" \
	"mmcpart=" __stringify(CONFIG_SYS_MMC_IMG_LOAD_PART) "\0" \
	"mmcroot=" CONFIG_MMCROOT " rootwait rw\0" \
	"mmcautodetect=yes\0" \
	"root_opt=rootwait rw\0" \
	"emmc_ul=setenv iface mmc; setenv dev 2; setenv part 1;" \
	"setenv bootargs console=${console} root=/dev/mmcblk2p2 ${root_opt};\0" \
	"sd_ul=setenv iface mmc; setenv dev 1; setenv part 1;" \
	"setenv bootargs console=${console} root=/dev/mmcblk1p2 ${root_opt};\0" \
	"usb_ul=usb start; setenv iface usb; setenv dev 0; setenv part 1;" \
	"setenv bootargs console=${console} root=/dev/sda2 ${root_opt};\0" \
	"ulbootscript=load ${iface} ${dev}:${part} ${loadaddr} ${script};\0" \
	"ulimage=load ${iface} ${dev}:${part} ${loadaddr} ${image}\0" \
	"ulfdt=if test ${boot_fdt} = yes || test ${boot_fdt} = try; then " \
		"load ${iface} ${dev}:${part} ${fdt_addr} ${fdt_file}; fi;\0" \
	"bootscript=echo Running bootscript from mmc ...; " \
		"source\0" \
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
		"if test ${boot_fdt} = yes || test ${boot_fdt} = try; then " \
			"if ${get_cmd} ${fdt_addr} ${fdt_file}; then " \
				"booti ${loadaddr} - ${fdt_addr}; " \
			"else " \
				"echo WARN: Cannot load the DT; " \
			"fi; " \
		"else " \
			"booti; " \
		"fi;\0"
#define CONFIG_BOOTCOMMAND \
	"for src in sd_ul usb_ul emmc_ul; do " \
		"run ${src}; " \
		"if run ulbootscript; then " \
			"run bootscript; " \
		"else " \
			"if run ulimage; then " \
				"if run ulfdt; then " \
					"booti ${loadaddr} - ${fdt_addr}; " \
				"else " \
					"if test ${boot_fdt} != yes; then " \
						"booti ${loadaddr}; " \
					"fi; " \
				"fi; " \
			"fi; " \
		"fi; " \
	"done; " \
	"usb start; ums 0 mmc ${mmcdev};"

/* Link Definitions */

#define CONFIG_LOADADDR		0x40480000
#define CONFIG_SYS_LOAD_ADDR	CONFIG_LOADADDR

#define CFG_SYS_INIT_RAM_ADDR        0x40000000
#define CFG_SYS_INIT_RAM_SIZE        0x200000
#define CFG_SYS_INIT_SP_OFFSET \
        (CFG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CFG_SYS_INIT_SP_ADDR \
        (CFG_SYS_INIT_RAM_ADDR + CFG_SYS_INIT_SP_OFFSET)

#define CONFIG_ENV_OVERWRITE
#define CONFIG_MMCROOT			"/dev/mmcblk1p2"  /* USDHC2 */


#define PHYS_SDRAM              0x40000000
#define PHYS_SDRAM_2            0x100000000
#define PHYS_SDRAM_SIZE   0 /* Memory chip autodetection */
#define PHYS_SDRAM_2_SIZE 0 /* Memory chip autodetection */
#define CFG_NR_DRAM_BANKS    4
#define CFG_SYS_SDRAM_BASE   PHYS_SDRAM

#define MEMTEST_DIVIDER   2
#define MEMTEST_NUMERATOR 1

#define CFG_MXC_UART_BASE		UART3_BASE_ADDR

#define CONFIG_SYS_BARGSIZE CONFIG_SYS_CBSIZE

#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

/* USDHC */

#define CFG_SYS_FSL_USDHC_NUM	2
#define CFG_SYS_FSL_ESDHC_ADDR       0

/* EEPROM */
#define CONFIG_ENV_EEPROM_IS_ON_I2C
#define CFG_SYS_I2C_SLAVE			0x00

#define CONFIG_MXC_USB_PORTSC  (PORT_PTS_UTMI | PORT_PTS_PTW)

#endif
