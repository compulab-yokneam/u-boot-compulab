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
#include <asm/arch/imx8mp_pins.h>
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

DECLARE_GLOBAL_DATA_PTR;

#if defined(CONFIG_FEC_MXC) || defined(CONFIG_DWC_ETH_QOS)
#include "../common/eeprom.h"
void imx_get_mac_from_fuse(int dev_id, unsigned char *mac)
{
	cl_eeprom_read_n_mac_addr(mac, dev_id, CONFIG_SYS_I2C_EEPROM_BUS);
	debug("%s: MAC%d: %02x.%02x.%02x.%02x.%02x.%02x\n",
	      __func__, dev_id, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return;
}
#endif

/* IOT-GATE-IMX8PLUS M.2 extension boards ID */
typedef enum {
	IOTG_IMX8PLUS_ADDON_FIRST,
	IOTG_IMX8PLUS_ADDON_M2EMMC = IOTG_IMX8PLUS_ADDON_FIRST, /* eMMC+TPM module */
	IOTG_IMX8PLUS_ADDON_M2ADC, /* ADC+TPM module */
	IOTG_IMX8PLUS_ADDON_M2TPM, /* TPM module */
	IOTG_IMX8PLUS_ADDON_EMPTY,
	IOTG_IMX8PLUS_ADDON_LAST = IOTG_IMX8PLUS_ADDON_EMPTY,
	IOTG_IMX8PLUS_ADDON_NUM,
} iotg_imx8plus_addon_type;

static char *iotg_imx8plus_addon_type_name[IOTG_IMX8PLUS_ADDON_NUM] = {
	[IOTG_IMX8PLUS_ADDON_M2EMMC] = "M2EMMC",
	[IOTG_IMX8PLUS_ADDON_M2ADC] = "M2ADC",
	[IOTG_IMX8PLUS_ADDON_M2TPM] = "M2TPM",
	[IOTG_IMX8PLUS_ADDON_EMPTY] = "none",
};

/* Device tree names array */
static char *iotg_imx8plus_dtb[IOTG_IMX8PLUS_ADDON_NUM] = {
	[IOTG_IMX8PLUS_ADDON_M2EMMC] = "iot-gate-imx8plus-m2emmc.dtb",
	[IOTG_IMX8PLUS_ADDON_M2ADC] = "iot-gate-imx8plus-m2adc.dtb",
	[IOTG_IMX8PLUS_ADDON_M2TPM] = "iot-gate-imx8plus-m2tpm.dtb",
	[IOTG_IMX8PLUS_ADDON_EMPTY] = "iot-gate-imx8plus.dtb",
};

/* I2C bus numbers array */
static int iotg_imx8plus_addon_i2c_bus[IOTG_IMX8PLUS_ADDON_LAST] = {
	[IOTG_IMX8PLUS_ADDON_M2EMMC] = 4,
	[IOTG_IMX8PLUS_ADDON_M2ADC] = 4,
	[IOTG_IMX8PLUS_ADDON_M2TPM] = 4,
};

/* I2C device addresses array */
static uint iotg_imx8plus_addon_i2c_addr[IOTG_IMX8PLUS_ADDON_LAST] = {
	[IOTG_IMX8PLUS_ADDON_M2EMMC] = 0x20,
	[IOTG_IMX8PLUS_ADDON_M2ADC] = 0x48,
	[IOTG_IMX8PLUS_ADDON_M2TPM] = 0x54,
};

/* Extension board type detected */
static int iotg_imx8plus_addon_id = IOTG_IMX8PLUS_ADDON_EMPTY;

#define IOTG_IMX8PLUS_ENV_FDT_FILE	"fdtfile"
#define IOTG_IMX8PLUS_ENV_ADDON_SETUP	"addon_smart_setup"
#define IOTG_IMX8PLUS_ENV_ADDON_BOARD	"addon_board"

#define EMMC_SIZE(_detval)		((_detval & 0xf) << 4)

/*
 * iotg_imx8plus_detect_addon() - extended add-on board detection
 * The detection is done according to the detected I2C devices.
 */
static void iotg_imx8plus_detect_addon(void)
{
	int ret;
	struct udevice *i2c_bus, *i2c_dev;
	int type;

	for (type = IOTG_IMX8PLUS_ADDON_FIRST; type < IOTG_IMX8PLUS_ADDON_LAST; type++) {
		debug("%s: type_idx = %d, probing I2C bus %d\n", __func__, type, iotg_imx8plus_addon_i2c_bus[type]);
		ret = uclass_get_device_by_seq(UCLASS_I2C, iotg_imx8plus_addon_i2c_bus[type], &i2c_bus);
		if (ret) {
			debug("%s: Failed probing I2C bus %d\n", __func__, iotg_imx8plus_addon_i2c_bus[type]);
			continue;
		}

		debug("%s: type_idx = %d, probing I2C addr = %d\n", __func__, type, iotg_imx8plus_addon_i2c_addr[type]);
		ret = dm_i2c_probe(i2c_bus, iotg_imx8plus_addon_i2c_addr[type], 0, &i2c_dev);
		if (!ret) {
			iotg_imx8plus_addon_id = type;
			debug("%s: detected module type_idx = %d, type_name = %s\n", __func__, type,
				iotg_imx8plus_addon_type_name[type]);
			printf("Add-on Board:   %s", iotg_imx8plus_addon_type_name[type]);
			if (type == IOTG_IMX8PLUS_ADDON_M2EMMC) {
				/* Detect eMMC size: read offset 0 (Input port 0 reg) and inspect 4 lower bits */
				ret = dm_i2c_reg_read(i2c_dev, 0);
				printf("(%dG)", EMMC_SIZE(ret));
			}
			printf("\n");
			env_set(IOTG_IMX8PLUS_ENV_ADDON_BOARD, iotg_imx8plus_addon_type_name[type]);

			return;
		}
	}

	env_set(IOTG_IMX8PLUS_ENV_ADDON_BOARD, iotg_imx8plus_addon_type_name[IOTG_IMX8PLUS_ADDON_EMPTY]);
}

/*
 * iot_gate_imx8plus_select_dtb() - select the kernel device tree blob
 * The device tree blob is selected according to the detected add-on board.
 */
static void iotg_imx8plus_select_dtb(void)
{
	if (!env_get_yesno(IOTG_IMX8PLUS_ENV_ADDON_SETUP))
		return;

	debug("%s: set %s = %s\n", __func__, IOTG_IMX8PLUS_ENV_FDT_FILE,
		iotg_imx8plus_dtb[iotg_imx8plus_addon_id]);
	env_set(IOTG_IMX8PLUS_ENV_FDT_FILE,
		iotg_imx8plus_dtb[iotg_imx8plus_addon_id]);
}

void board_vendor_late_init(void) {
#ifdef CONFIG_ADDON_SMART_SETUP
	/* Check feature strategy and set to default if not defined explicitly */
	if (env_get_yesno(IOTG_IMX8PLUS_ENV_ADDON_SETUP) == -1) {
	#ifdef CONFIG_ADDON_SMART_SETUP_DEFAULT_ON
		env_set(IOTG_IMX8PLUS_ENV_ADDON_SETUP, "yes");
	#else
		env_set(IOTG_IMX8PLUS_ENV_ADDON_SETUP, "no");
	#endif
	}

	/* Detect extension module in M.2 expantion connector */
	iotg_imx8plus_detect_addon();
	/* Apply an appropriate dtb */
	iotg_imx8plus_select_dtb();
#endif
}
