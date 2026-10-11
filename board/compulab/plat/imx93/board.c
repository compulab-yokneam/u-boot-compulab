// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2022 NXP
 */

#include <autoboot.h>
#include <env.h>
#include <efi_loader.h>
#include <fdt_support.h>
#include <init.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch-imx9/imx93_pins.h>
#include <asm/mach-imx/iomux-v3.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <usb.h>
#include <asm/gpio.h>
#include <i2c.h>
#include <linux/string.h>
#include <malloc.h>
#include <net.h>
#include "../../common/eeprom.h"
#include "../../common/fdt.h"

#if CONFIG_IS_ENABLED(EFI_HAVE_CAPSULE_SUPPORT)
#define IMX_BOOT_IMAGE_GUID \
	EFI_GUID(0xbc550d86, 0xda26, 0x4b70, 0xac, 0x05, \
		 0x2a, 0x44, 0x8e, 0xda, 0x6f, 0x21)

struct efi_fw_image fw_images[] = {
	{
		.image_type_id = IMX_BOOT_IMAGE_GUID,
		.fw_name = u"CL-IMX93-RAW",
		.image_index = 1,
	},
};

struct efi_capsule_update_info update_info = {
	.dfu_string = "mmc 0=flash-bin raw 0 0x2000 mmcpart 1",
	.num_images = ARRAY_SIZE(fw_images),
	.images = fw_images,
};
#endif /* EFI_HAVE_CAPSULE_SUPPORT */

static const iomux_v3_cfg_t gpio_pads[] = {
	MX93_PAD_PDM_CLK__GPIO1_IO08 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void board_gpio_init(void)
{
	struct gpio_desc desc;
	struct udevice *dev;
	int ret;

	imx_iomux_v3_setup_multiple_pads(gpio_pads, ARRAY_SIZE(gpio_pads));

	/* Enable the CompuLab carrier I/O expander before it is probed. */
	ret = uclass_get_device_by_seq(UCLASS_GPIO, 0, &dev);
	if (ret) {
		printf("%s: failed to find GPIO1, ret=%d\n", __func__, ret);
		return;
	}

	desc.dev = dev;
	desc.offset = 8;
	desc.flags = 0;
	ret = dm_gpio_request(&desc, "EXP_nPWREN");
	if (ret) {
		printf("%s: failed to request EXP_nPWREN, ret=%d\n", __func__, ret);
		return;
	}

	dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	dm_gpio_set_value(&desc, 1);
}

int board_init(void)
{
#if IS_ENABLED(CONFIG_USB_TCPC)
	setup_typec();
#endif

	board_gpio_init();

	return 0;
}

static const char *cl_imx93_detect_platform(void)
{
	const char *configured = env_get("platform");
	char product[PRODUCT_NAME_SIZE] = {};

	/* IOT-LINK deliberately remains a separate, RT-oriented platform. */
	if (!fdt_node_check_compatible(gd->fdt_blob, 0,
				       "compulab,iot-link")) {
		printf("Platform: IOT-LINK (control DT)\n");
		return "iot-link";
	}

	if (configured) {
		if (!strcasecmp(configured, "ucm") ||
		    !strcasecmp(configured, "ucm-imx93")) {
			printf("Platform: UCM-i.MX93 (environment override)\n");
			return "ucm-imx93";
		}

		if (!strcasecmp(configured, "mcm") ||
		    !strcasecmp(configured, "mcm-imx93")) {
			printf("Platform: MCM-i.MX93 (environment override)\n");
			return "mcm-imx93";
		}

		printf("WARN: ignoring unknown platform override '%s'\n",
		       configured);
	}

	cl_eeprom_read_som_name(product);
	if (!strncasecmp(product, "UCM", 3)) {
		printf("Platform: UCM-i.MX93 (EEPROM: %s)\n", product);
		return "ucm-imx93";
	}

	if (!strncasecmp(product, "MCM", 3)) {
		printf("Platform: MCM-i.MX93 (EEPROM: %s)\n", product);
		return "mcm-imx93";
	}

	printf("WARN: unknown i.MX93 module EEPROM product '%s'; "
	       "defaulting to UCM-i.MX93\n", product);
	printf("WARN: use 'setenv platform mcm-imx93' to override\n");
	return "ucm-imx93";
}

#define IOT_LINK_EEPROM_BUS	2
#define IOT_LINK_EEPROM_ADDR	0x50
#define IOT_LINK_EEPROM_SIZE	256

struct iot_link_overlay {
	const char *option;
	const char *configuration;
	const char *filename;
};

static const struct iot_link_overlay iot_link_overlays[] = {
	{ "FARS4", "#conf-iot-link-fars485.dtbo", "iot-link-fars485.dtbo" },
	{ "FACAN", "#conf-iot-link-facan.dtbo", "iot-link-facan.dtbo" },
	{ "FBCAN", "#conf-iot-link-fbcan.dtbo", "iot-link-fbcan.dtbo" },
	{ "FBRS4", "#conf-iot-link-fbrs485.dtbo", "iot-link-fbrs485.dtbo" },
};

static bool iot_link_eeprom_has_option(const u8 *data, size_t data_size,
				       const char *option)
{
	size_t option_size = strlen(option);
	size_t offset;

	for (offset = 0; offset + option_size <= data_size; offset++) {
		if (!memcmp(data + offset, option, option_size))
			return true;
	}

	return false;
}

static int iot_link_set_overlays(void)
{
	char configurations[160] = {};
	char filenames[160] = {};
	u8 eeprom[IOT_LINK_EEPROM_SIZE];
	struct udevice *dev;
	int found = 0;
	int ret;
	int i;

	ret = i2c_get_chip_for_busnum(IOT_LINK_EEPROM_BUS,
				      IOT_LINK_EEPROM_ADDR, 1, &dev);
	if (ret) {
		printf("IOT-LINK: baseboard EEPROM not found, ret=%d\n", ret);
		goto set_environment;
	}

	ret = dm_i2c_read(dev, 0, eeprom, sizeof(eeprom));
	if (ret) {
		printf("IOT-LINK: failed to read baseboard EEPROM, ret=%d\n", ret);
		goto set_environment;
	}

	for (i = 0; i < ARRAY_SIZE(iot_link_overlays); i++) {
		const struct iot_link_overlay *overlay = &iot_link_overlays[i];

		if (!iot_link_eeprom_has_option(eeprom, sizeof(eeprom),
						overlay->option))
			continue;

		strlcat(configurations, overlay->configuration,
			sizeof(configurations));
		if (*filenames)
			strlcat(filenames, " ", sizeof(filenames));
		strlcat(filenames, overlay->filename, sizeof(filenames));
		printf("IOT-LINK: detected %s, enabling %s\n",
		       overlay->option, overlay->filename);
		found++;
	}

	if (!found)
		printf("IOT-LINK: no optional CAN/RS-485 interfaces detected\n");

set_environment:
	/* Hardware configuration takes precedence over a saved environment. */
	if (env_set("overlays", configurations))
		return -EIO;
	if (env_set("overlays_files", filenames))
		return -EIO;

	return ret;
}

#if defined(CONFIG_FEC_MXC) || defined(CONFIG_DWC_ETH_QOS)
static void board_get_mac_from_eeprom(int dev_id)
{
	uchar mac[ARP_HLEN];

	cl_eeprom_read_n_mac_addr(mac, dev_id, CONFIG_SYS_I2C_EEPROM_BUS);
	if (is_zero_ethaddr(mac) || !is_valid_ethaddr(mac))
		return;

	eth_env_set_enetaddr_by_index("eth", dev_id, mac);
}
#else
static void board_get_mac_from_eeprom(int dev_id)
{
}
#endif

static void cl_imx93_setup_runtime_env(void)
{
	const char *platform;
	const char *fdtfile;
	int ret;

#if CONFIG_IS_ENABLED(ENV_IS_IN_MMC) || CONFIG_IS_ENABLED(ENV_IS_NOWHERE)
	board_late_mmc_env_init();
	/* SDP does not identify where the OS lives; prefer SD, then try eMMC. */
	if (is_usb_boot())
		env_set_ulong("mmcdev", env_get_ulong("sd_dev", 10, 1));
#endif

	env_set("sec_boot", "no");
#ifdef CONFIG_AHAB_BOOT
	env_set("sec_boot", "yes");
#endif

	platform = cl_imx93_detect_platform();
	if (!strcmp(platform, "iot-link"))
		fdtfile = "iot-link.dtb";
	else if (!strcmp(platform, "mcm-imx93"))
		fdtfile = "sbc-mcm-imx93.dtb";
	else
		fdtfile = "ucm-imx93.dtb";

	env_set("platform_detected", platform);
	if (!strcmp(platform, "iot-link")) {
		cl_eeprom_set_sb_path(IOT_LINK_EEPROM_BUS,
				      IOT_LINK_EEPROM_ADDR);
		env_set("image", "fitImage");
		env_set("boot_fit", "yes");
		ret = iot_link_set_overlays();
		if (ret)
			printf("WARN: IOT-LINK overlays disabled, ret=%d\n", ret);
	} else {
		env_set("image", "Image");
		env_set("boot_fit", "no");
		env_set("overlays", NULL);
		env_set("overlays_files", NULL);
	}

	ret = env_set("fdtfile", fdtfile);
	if (ret)
		printf("Failed to set fdtfile=%s, ret=%d\n", fdtfile, ret);
	else
		printf("FDT:   %s\n", fdtfile);

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", platform);
	env_set("board_rev", "iMX93");
#endif
	board_get_mac_from_eeprom(0);
	board_get_mac_from_eeprom(1);
}

int board_late_init(void)
{
	cl_imx93_setup_runtime_env();
	return 0;
}

void board_autoboot_env_init(void)
{
	/* SDP manufacturing boot resets the environment after board_late_init(). */
	cl_imx93_setup_runtime_env();
}

#ifdef CONFIG_OF_BOARD_SETUP
static int cl_imx93_set_linux_hostname(void *blob, const char *platform)
{
	const char *current;
	char *updated;
	size_t size;
	int chosen, ret;

	chosen = fdt_path_offset(blob, "/chosen");
	if (chosen < 0)
		return chosen;

	current = fdt_getprop(blob, chosen, "bootargs", NULL);
	if (!current)
		current = "";

	size = strlen(current) + strlen(platform) +
	       sizeof(" systemd.hostname=");
	updated = malloc(size);
	if (!updated)
		return -ENOMEM;

	if (*current)
		snprintf(updated, size, "%s systemd.hostname=%s",
			 current, platform);
	else
		snprintf(updated, size, "systemd.hostname=%s", platform);

	ret = fdt_setprop_string(blob, chosen, "bootargs", updated);
	free(updated);

	return ret;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	const char *platform = env_get("platform_detected");
	int node, ret;

	(void)bd;

	ret = fdt_set_env_addr(blob);
	if (ret) {
		printf("Failed to add environment information to FDT, ret=%d\n",
		       ret);
		return ret;
	}

	ret = fdt_set_sn(blob);
	if (ret) {
		printf("Failed to add EEPROM information to FDT, ret=%d\n", ret);
		return ret;
	}

	if (!platform)
		platform = cl_imx93_detect_platform();

	node = fdt_add_subnode(blob, 0, "som.info");
	if (node == -FDT_ERR_EXISTS)
		node = fdt_path_offset(blob, "/som.info");
	if (node < 0) {
		printf("Failed to create /som.info, ret=%d\n", node);
		return node;
	}

	ret = fdt_setprop_string(blob, node, "board.name", platform);
	if (ret) {
		printf("Failed to set /som.info/board.name, ret=%d\n", ret);
		return ret;
	}

	ret = cl_imx93_set_linux_hostname(blob, platform);
	if (ret) {
		printf("Failed to set Linux hostname to %s, ret=%d\n",
		       platform, ret);
		return ret;
	}

	return 0;
}
#endif

#ifdef CONFIG_FSL_FASTBOOT
#ifdef CONFIG_ANDROID_RECOVERY
int is_recovery_key_pressing(void)
{
	return 0;
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/
