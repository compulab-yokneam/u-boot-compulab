/*
 * Copyright 2020 CompuLab
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <asm/io.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/clock.h>
#include <spl.h>
#include <asm/mach-imx/dma.h>
#include <power/pmic.h>
#include <power/bd71837.h>
#include <usb.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/video.h>
#include "../common/eeprom.h"

DECLARE_GLOBAL_DATA_PTR;

#define ENV_FDT_FILE "fdt_file"
#define TYPE_2L_OPT "WBM" //Wi/Fi module is LBES5PL2EL by Murata, PCB rev 1.4
static void ucm_imx8_mini_select_dtb(void)
{
	const char *fdt_src;
	const char *env_fdt_file = env_get(ENV_FDT_FILE);
	char buf[PRODUCT_OPTION_SIZE * PRODUCT_OPTION_NUM + 1];
// Variable fdt_file unset or empty -- choose FDT, basing on the device options
	if (NULL == env_fdt_file || 0 == env_fdt_file[0]) {
		env_fdt_file = CONFIG_DEFAULT_FDT_WB5; //Valid DTB for all PCB revisions, but 1.4
		if (cl_eeprom_read_som_options(buf)) {
			buf[-1] = 0; // Precaution, sinse we dont have strnstr function
			if (strstr(buf, TYPE_2L_OPT))
				env_fdt_file = CONFIG_DEFAULT_FDT_2EL;

			fdt_src = "eeprom"; // board revision determines FDT name
		} else { // Nothing found, m.b uninitialized eeprom
			fdt_src = "default";
		}
		env_set(ENV_FDT_FILE, env_fdt_file);
	} else {
		fdt_src = "env"; // FDT name is gotten from environment
	}

	printf("FDT[%s]: %s %s\n", fdt_src, env_fdt_file);
}

int sub_board_late_init(void)
{
	ucm_imx8_mini_select_dtb();
	return 0;
}


