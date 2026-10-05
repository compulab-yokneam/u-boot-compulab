// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2022 NXP
 */

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

#if CONFIG_IS_ENABLED(EFI_HAVE_CAPSULE_SUPPORT)
#define IMX_BOOT_IMAGE_GUID \
	EFI_GUID(0xbc550d86, 0xda26, 0x4b70, 0xac, 0x05, \
		 0x2a, 0x44, 0x8e, 0xda, 0x6f, 0x21)

struct efi_fw_image fw_images[] = {
	{
		.image_type_id = IMX_BOOT_IMAGE_GUID,
		.fw_name = u"IMX93-11X11-EVK-RAW",
		.image_index = 1,
	},
};

struct efi_capsule_update_info update_info = {
	.dfu_string = "mmc 0=flash-bin raw 0 0x2000 mmcpart 1",
	.num_images = ARRAY_SIZE(fw_images),
	.images = fw_images,
};
#endif /* EFI_HAVE_CAPSULE_SUPPORT */

#if 0 /* The CompuLab carriers do not use the EVK Type-C controllers. */
struct tcpc_port port1;
struct tcpc_port port2;
struct tcpc_port portpd;

static int setup_pd_switch(uint8_t i2c_bus, uint8_t addr)
{
	struct udevice *bus;
	struct udevice *i2c_dev = NULL;
	int ret;
	uint8_t valb;

	ret = uclass_get_device_by_seq(UCLASS_I2C, i2c_bus, &bus);
	if (ret) {
		printf("%s: Can't find bus\n", __func__);
		return -EINVAL;
	}

	ret = dm_i2c_probe(bus, addr, 0, &i2c_dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x\n",
			__func__, addr);
		return -ENODEV;
	}

	ret = dm_i2c_read(i2c_dev, 0xB, &valb, 1);
	if (ret) {
		printf("%s dm_i2c_read failed, err %d\n", __func__, ret);
		return -EIO;
	}
	valb |= 0x4; /* Set DB_EXIT to exit dead battery mode */
	ret = dm_i2c_write(i2c_dev, 0xB, (const uint8_t *)&valb, 1);
	if (ret) {
		printf("%s dm_i2c_write failed, err %d\n", __func__, ret);
		return -EIO;
	}

	/* Set OVP threshold to 23V */
	valb = 0x6;
	ret = dm_i2c_write(i2c_dev, 0x8, (const uint8_t *)&valb, 1);
	if (ret) {
		printf("%s dm_i2c_write failed, err %d\n", __func__, ret);
		return -EIO;
	}

	return 0;
}

int pd_switch_snk_enable(struct tcpc_port *port)
{
	if (port == &port1) {
		debug("Setup pd switch on port 1\n");
		return setup_pd_switch(2, 0x71);
	} else if (port == &port2) {
		debug("Setup pd switch on port 2\n");
		return setup_pd_switch(2, 0x73);
	} else
		return -EINVAL;
}

struct tcpc_port_config portpd_config = {
	.i2c_bus = 2, /*i2c3*/
	.addr = 0x52,
	.port_type = TYPEC_PORT_UFP,
	.max_snk_mv = 20000,
	.max_snk_ma = 3000,
	.max_snk_mw = 15000,
	.op_snk_mv = 9000,
};

struct tcpc_port_config port1_config = {
	.i2c_bus = 2, /*i2c3*/
	.addr = 0x50,
	.port_type = TYPEC_PORT_UFP,
	.max_snk_mv = 5000,
	.max_snk_ma = 3000,
	.max_snk_mw = 40000,
	.op_snk_mv = 9000,
	.switch_setup_func = &pd_switch_snk_enable,
	.disable_pd = true,
};

struct tcpc_port_config port2_config = {
	.i2c_bus = 2, /*i2c3*/
	.addr = 0x51,
	.port_type = TYPEC_PORT_UFP,
	.max_snk_mv = 9000,
	.max_snk_ma = 3000,
	.max_snk_mw = 40000,
	.op_snk_mv = 9000,
	.switch_setup_func = &pd_switch_snk_enable,
	.disable_pd = true,
};

static int setup_typec(void)
{
	int ret;

	debug("tcpc_init port pd\n");
	ret = tcpc_init(&portpd, portpd_config, NULL);
	if (ret) {
		printf("%s: tcpc portpd init failed, err=%d\n",
		       __func__, ret);
	}

	debug("tcpc_init port 2\n");
	ret = tcpc_init(&port2, port2_config, NULL);
	if (ret) {
		printf("%s: tcpc port2 init failed, err=%d\n",
		       __func__, ret);
	}

	debug("tcpc_init port 1\n");
	ret = tcpc_init(&port1, port1_config, NULL);
	if (ret) {
		printf("%s: tcpc port1 init failed, err=%d\n",
		       __func__, ret);
	}

	return ret;
}

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;
	struct tcpc_port *port_ptr;

	debug("board_usb_init %d, type %d\n", index, init);

	if (index == 0)
		port_ptr = &port1;
	else
		port_ptr = &port2;

	if (init == USB_INIT_HOST)
		ret = tcpc_setup_dfp_mode(port_ptr);
	else
		ret = tcpc_setup_ufp_mode(port_ptr);

	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	debug("board_usb_cleanup %d, type %d\n", index, init);

	if (init == USB_INIT_HOST) {
		if (index == 0)
			ret = tcpc_disable_src_vbus(&port1);
		else
			ret = tcpc_disable_src_vbus(&port2);
	}

	return ret;
}

int board_ehci_usb_phy_mode(struct udevice *dev)
{
	int ret = 0;
	enum typec_cc_polarity pol;
	enum typec_cc_state state;
	struct tcpc_port *port_ptr;

	debug("%s %d\n", __func__, dev_seq(dev));

	if (dev_seq(dev) == 0)
		port_ptr = &port1;
	else
		port_ptr = &port2;

	tcpc_setup_ufp_mode(port_ptr);

	ret = tcpc_get_cc_status(port_ptr, &pol, &state);

	tcpc_print_log(port_ptr);
	if (!ret) {
		if (state == TYPEC_STATE_SRC_RD_RA || state == TYPEC_STATE_SRC_RD)
			return USB_INIT_HOST;
	}

	return USB_INIT_DEVICE;
}
#endif

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

int board_late_init(void)
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
	return 0;
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
