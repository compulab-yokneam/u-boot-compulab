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

int board_late_init(void)
{
	const char *fdtfile = "cl-imx93.dtb";
	const char *model;
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

	model = fdt_getprop(gd->fdt_blob, 0, "model", NULL);
	if ((model && strstr(model, "UCM-i.MX93")) ||
	    !fdt_node_check_compatible(gd->fdt_blob, 0,
				       "compulab,ucm-imx93"))
		fdtfile = "ucm-imx93.dtb";
	else if ((model && strstr(model, "MCM-i.MX93")) ||
		 !fdt_node_check_compatible(gd->fdt_blob, 0,
					      "compulab,mcm-imx93"))
		fdtfile = "mcm-imx93.dtb";
	else if ((model && strstr(model, "IOT-LINK")) ||
		 !fdt_node_check_compatible(gd->fdt_blob, 0,
					      "compulab,iot-link"))
		fdtfile = "iot-link.dtb";

	ret = env_set("fdtfile", fdtfile);
	if (ret)
		printf("Failed to set fdtfile=%s, ret=%d\n", fdtfile, ret);
	else
		printf("FDT:   %s\n", fdtfile);

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", "11X11_EVK");
	env_set("board_rev", "iMX93");
#endif
	return 0;
}

#ifdef CONFIG_FSL_FASTBOOT
#ifdef CONFIG_ANDROID_RECOVERY
int is_recovery_key_pressing(void)
{
	return 0;
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/
