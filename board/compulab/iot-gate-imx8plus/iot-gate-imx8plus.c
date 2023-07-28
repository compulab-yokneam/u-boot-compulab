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
	IOTG_IMX8PLUS_EXT_FIRST,
	IOTG_IMX8PLUS_EXT_M2TPM = IOTG_IMX8PLUS_EXT_FIRST, /* TPM module */
	IOTG_IMX8PLUS_EXT_EMPTY,
	IOTG_IMX8PLUS_EXT_LAST = IOTG_IMX8PLUS_EXT_EMPTY,
	IOTG_IMX8PLUS_EXT_NUM,
} iotg_imx8plus_ext_type;

static char *iotg_imx8plus_ext_type_name[IOTG_IMX8PLUS_EXT_LAST] = {
	[IOTG_IMX8PLUS_EXT_M2TPM] = "M2TPM",
};

/* Device tree names array */
static char *iotg_imx8plus_dtb[IOTG_IMX8PLUS_EXT_NUM] = {
	[IOTG_IMX8PLUS_EXT_M2TPM] = "iot-gate-imx8plus-m2tpm.dtb",
	[IOTG_IMX8PLUS_EXT_EMPTY] = "iot-gate-imx8plus.dtb",
};

/* I2C bus numbers array */
static int iotg_imx8plus_ext_i2c_bus[IOTG_IMX8PLUS_EXT_LAST] = {
	[IOTG_IMX8PLUS_EXT_M2TPM] = 4,
};

/* I2C device addresses array */
static uint iotg_imx8plus_ext_i2c_addr[IOTG_IMX8PLUS_EXT_LAST] = {
	[IOTG_IMX8PLUS_EXT_M2TPM] = 0x54,
};

/* Extension board type detected */
static int iotg_imx8plus_ext_id = IOTG_IMX8PLUS_EXT_EMPTY;
/*
 * iotg_imx8plus_detect_ext() - extended board detection
 * The detection is done according to the detected I2C devices.
 */
static void iotg_imx8plus_detect_ext(void)
{
	int ret;
	struct udevice *i2c_bus, *i2c_dev;
	int type;

	for (type = IOTG_IMX8PLUS_EXT_FIRST; type < IOTG_IMX8PLUS_EXT_LAST; type++) {
		debug("%s: type_idx = %d, probing I2C bus %d\n", __func__, type, iotg_imx8plus_ext_i2c_bus[type]);
		ret = uclass_get_device_by_seq(UCLASS_I2C, iotg_imx8plus_ext_i2c_bus[type], &i2c_bus);
		if (ret) {
			debug("%s: Failed probing I2C bus %d\n", __func__, iotg_imx8plus_ext_i2c_bus[type]);
			continue;
		}

		debug("%s: type_idx = %d, probing I2C addr = %d\n", __func__, type, iotg_imx8plus_ext_i2c_addr[type]);
		ret = dm_i2c_probe(i2c_bus, iotg_imx8plus_ext_i2c_addr[type], 0, &i2c_dev);
		if (!ret) {
			iotg_imx8plus_ext_id = type;
			debug("%s: detected module type_idx = %d, type_name = %s\n", __func__, type,
				iotg_imx8plus_ext_type_name[type]);
			return;
		}
	}
}

#define IOTG_IMX8PLUS_ENV_FDT_FILE "fdtfile"
/*
 * iot_gate_imx8plus_select_dtb() - select the kernel device tree blob
 * The device tree blob is selected according to the detected extended board.
 */
static void iotg_imx8plus_select_dtb(void)
{
	char *env_fdt_file = env_get(IOTG_IMX8PLUS_ENV_FDT_FILE);

	debug("%s: set %s = %s\n", __func__, IOTG_IMX8PLUS_ENV_FDT_FILE,
		iotg_imx8plus_dtb[iotg_imx8plus_ext_id]);
	env_set(IOTG_IMX8PLUS_ENV_FDT_FILE,
		iotg_imx8plus_dtb[iotg_imx8plus_ext_id]);
}

void board_vendor_late_init(void) {
	/* Detect extension module in M.2 expantion connector */
	iotg_imx8plus_detect_ext();
	/* Apply an appropriate dtb */
	iotg_imx8plus_select_dtb();
}
