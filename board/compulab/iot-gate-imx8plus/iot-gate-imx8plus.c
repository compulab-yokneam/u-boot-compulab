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

/* IOT-GATE-IMX8PLUS M.2 extension boards ID */
typedef enum {
	IOTG_IMX8PLUS_ADDON_FIRST,
	IOTG_IMX8PLUS_ADDON_M2EMMC = IOTG_IMX8PLUS_ADDON_FIRST, /* eMMC+TPM module */
	IOTG_IMX8PLUS_ADDON_M2M2, /* M.2 slot+IMC+TPM module */
	IOTG_IMX8PLUS_ADDON_M2ADC, /* ADC+TPM module */
	IOTG_IMX8PLUS_ADDON_M2TPM, /* TPM module */
	IOTG_IMX8PLUS_ADDON_EMPTY,
	IOTG_IMX8PLUS_ADDON_LAST = IOTG_IMX8PLUS_ADDON_EMPTY,
	IOTG_IMX8PLUS_ADDON_NUM,
} iotg_imx8plus_addon_type;

static char *iotg_imx8plus_addon_type_name[IOTG_IMX8PLUS_ADDON_NUM] = {
	[IOTG_IMX8PLUS_ADDON_M2EMMC] = "M2EMMC",
	[IOTG_IMX8PLUS_ADDON_M2M2] = "M2M2",
	[IOTG_IMX8PLUS_ADDON_M2ADC] = "M2ADC",
	[IOTG_IMX8PLUS_ADDON_M2TPM] = "M2TPM",
	[IOTG_IMX8PLUS_ADDON_EMPTY] = "none",
};

/* Device tree names array */
static char *iotg_imx8plus_dtb[IOTG_IMX8PLUS_ADDON_NUM] = {
	[IOTG_IMX8PLUS_ADDON_M2EMMC] = "iot-gate-imx8plus-m2emmc.dtb",
	[IOTG_IMX8PLUS_ADDON_M2M2] = "iot-gate-imx8plus-m2m2.dtb",
	[IOTG_IMX8PLUS_ADDON_M2ADC] = "iot-gate-imx8plus-m2adc.dtb",
	[IOTG_IMX8PLUS_ADDON_M2TPM] = "iot-gate-imx8plus-m2tpm.dtb",
	[IOTG_IMX8PLUS_ADDON_EMPTY] = "iot-gate-imx8plus.dtb",
};

/* I2C bus numbers array */
static int iotg_imx8plus_addon_i2c_bus[IOTG_IMX8PLUS_ADDON_LAST] = {
	[IOTG_IMX8PLUS_ADDON_M2EMMC] = 4,
	[IOTG_IMX8PLUS_ADDON_M2M2] = 4,
	[IOTG_IMX8PLUS_ADDON_M2ADC] = 4,
	[IOTG_IMX8PLUS_ADDON_M2TPM] = 4,
};

/* I2C device addresses array */
static uint iotg_imx8plus_addon_i2c_addr[IOTG_IMX8PLUS_ADDON_LAST] = {
	[IOTG_IMX8PLUS_ADDON_M2EMMC] = 0x20,
	[IOTG_IMX8PLUS_ADDON_M2M2] = 0x21,
	[IOTG_IMX8PLUS_ADDON_M2ADC] = 0x48,
	[IOTG_IMX8PLUS_ADDON_M2TPM] = 0x54,
};

/* Extension board type detected */
static int iotg_imx8plus_addon_id = IOTG_IMX8PLUS_ADDON_EMPTY;

static char *iotg_imx8plus_rev2_dtbo="sb-iotgimx8plus-rev2.dtbo";

#define IOTG_IMX8PLUS_ENV_FDT_FILE	"fdtfile"
#define IOTG_IMX8PLUS_ENV_FDTO_FILE	"fdtofile"
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

#include <dm.h>
#include <w1.h>
#include <w1-eeprom.h>
#include <dm/device-internal.h>

/*
 * sb_iotgimx8plus_w1_init() - Initialize the 1-Wire bus and access the
 *                             1-Wire EEPROM (supported on rev. 2.x and later)
 * Return: 0 on success, or an error code on failure.
 */
int sb_iotgimx8plus_w1_init(void)
{
	int bus_n = 0, offset = 0, len = 1;
	struct udevice *bus, *dev;
	int ret;
	u8 buf[32];

	/* Try to acquire the 1-Wire bus */
	ret = w1_get_bus(bus_n, &bus);
	if (ret)
		return ret;

	/* Check for a device on the bus */
	ret = device_find_first_child(bus, &dev);
	if (ret)
		return ret;

	/* Probe the device */
	ret = device_probe(dev);
	if (ret || !dev)
		return ret;

	/* Read the very first byte. Its value is ignored because the EEPROM may not have been initialized */
	ret = w1_eeprom_read_buf(dev, offset, (u8 *)buf, len);
	if (ret)
		return ret;

	return 0;
}

/*
 * sb_iotgimx8plus_rev2x_init() - Check whether the baseboard revision is 2.x and
 * initialize it if necessary
 */
void sb_iotgimx8plus_rev2x_init(void)
{
	int ret;

	ret = sb_iotgimx8plus_w1_init();
	if (ret)
		return;

	/* 1-Wire EEPROM detected: the board is revision 2.x;
	   enable 1-Wire support in the kernel using a Device Tree overlay */
	env_set(IOTG_IMX8PLUS_ENV_FDTO_FILE, iotg_imx8plus_rev2_dtbo);
}

void board_vendor_late_init(void)
{
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
	/* Check whether the baseboard revision is 2.x and initialize it if necessary */
	sb_iotgimx8plus_rev2x_init();
}
