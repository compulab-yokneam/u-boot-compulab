// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2025-2026 NXP
 */

#include <env.h>
#include <efi_loader.h>
#include <init.h>
#include <fdt_simplefb.h>
#include <fdt_support.h>
#include <asm/arch/clock.h>
#include <usb.h>
#include <dwc3-uboot.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <asm/gpio.h>
#include <asm/global_data.h>
#include <power/regulator.h>
#include <scmi_agent.h>
#include <asm/arch/sys_proto.h>
#include <i2c.h>
#include <dm/uclass.h>
#include <dm/uclass-internal.h>
#include <dm/device.h>
#include <asm/arch/crrm.h>
#include <net-common.h>
#include "../arch/arm/dts/imx952-power.h"

#include "../common/eeprom.h"
#include "../common/fdt.h"

#define PD_HSIO_TOP IMX952_PD_HSIO_TOP
#define PD_NETC IMX952_PD_NETC
#define PD_DISPLAY IMX952_PD_DISPLAY
#define PD_CAMERA IMX952_PD_CAMERA

DECLARE_GLOBAL_DATA_PTR;

extern int board_fix_fdt_fuse(void *fdt);

#if CONFIG_IS_ENABLED(EFI_HAVE_CAPSULE_SUPPORT)
#define IMX_BOOT_IMAGE_GUID \
	EFI_GUID(0xb097e8d2, 0xfb37, 0x50c9, 0x9e, 0xb6, \
		0xa7, 0xf0, 0x74, 0x85, 0x44, 0x38)

struct efi_fw_image fw_images[] = {
	{
		.image_type_id = IMX_BOOT_IMAGE_GUID,
		.fw_name = u"UCM-IMX952-RAW",
		.image_index = 1,
	},
};

struct efi_capsule_update_info update_info = {
	.dfu_string = "mmc 0=flash-bin raw 0 0x2000 mmcpart 1",
	.num_images = ARRAY_SIZE(fw_images),
	.images = fw_images,
};
#endif /* EFI_HAVE_CAPSULE_SUPPORT */

int board_early_init_f(void)
{
	/* UART1: A55, UART2: M33, UART3: M7 */
	init_uart_clk(0);

	return 0;
}

static int imx9_scmi_power_domain_enable(u32 domain, bool enable)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device_by_name(UCLASS_CLK, "protocol@14", &dev);
	if (ret)
		return ret;

	return scmi_pwd_state_set(dev, 0, domain, enable ? 0 : BIT(30));
}

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 0 && init == USB_INIT_DEVICE) {
		ret = imx9_scmi_power_domain_enable(PD_HSIO_TOP, true);
		if (ret) {
			printf("SCMI_POWWER_STATE_SET Failed for USB\n");
			return ret;
		}

	} else if (index == 0 && init == USB_INIT_HOST) {
		return ret;
	}

	return 0;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;
	return ret;
}

static void netc_phy_rst(const char *gpio_name, const char *label)
{
	int ret, free_ret;
	struct gpio_desc desc;

	/* ENET_RST_B */
	ret = dm_gpio_lookup_name(gpio_name, &desc);
	if (ret) {
		printf("%s lookup %s failed ret = %d\n", __func__, gpio_name, ret);
		return;
	}

	ret = dm_gpio_request(&desc, label);
	if (ret) {
		printf("%s request %s failed ret = %d\n", __func__, label, ret);
		return;
	}

	/* assert the ENET_RST_B */
	ret = dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT |
				    GPIOD_IS_OUT_ACTIVE | GPIOD_ACTIVE_LOW);
	if (ret) {
		printf("%s configure %s failed ret = %d\n", __func__, label, ret);
		goto free_gpio;
	}

	udelay(10000);
	ret = dm_gpio_set_value(&desc, 0); /* deassert the ENET_RST_B */
	if (ret) {
		printf("%s deassert %s failed ret = %d\n", __func__, label, ret);
		goto free_gpio;
	}

	udelay(80000);

free_gpio:
	free_ret = dm_gpio_free(NULL, &desc);
	if (free_ret)
		printf("%s free %s failed ret = %d\n", __func__, label,
		       free_ret);
}

static bool is_netc_cfg(void)
{
	char cfgname[SCMI_MISC_MAX_CFGNAME];
	u32 msel;
	int ret;
	const char *netcfg = "mx952cplnetc";

	ret = scmi_misc_cfginfo(&msel, cfgname);
	if (!ret) {
		debug("SM: %s\n", cfgname);
		if (!strcmp(netcfg, cfgname))
			return true;
	}

	return false;
}

void netc_init(void)
{
	int ret;

	if (is_netc_cfg())
		return;

	ret = imx9_scmi_power_domain_enable(PD_NETC, false);
	udelay(10000);

	/* Power up the NETC MIX. */
	ret = imx9_scmi_power_domain_enable(PD_NETC, true);
	if (ret) {
		printf("SCMI_POWWER_STATE_SET Failed for NETC MIX\n");
		return;
	}

	netc_phy_rst("GPIO5_13", "ENET1_RST_B");
	netc_phy_rst("GPIO4_14", "ENET2_RST_B");
}

void imx_get_mac_from_fuse(int dev_id, unsigned char *mac)
{
	cl_eeprom_read_n_mac_addr(mac, dev_id, CONFIG_SYS_I2C_EEPROM_BUS);
	if (is_zero_ethaddr(mac) || !is_valid_ethaddr(mac))
		net_random_ethaddr(mac);

	eth_env_set_enetaddr_by_index("eth", dev_id, mac);
}

static void pcie_setup(void)
{
	int ret;
	struct udevice *dev;

	ret = regulator_get_by_devname("regulator-m2-pwr", &dev);
	if (ret) {
		printf("Get regulator-m2-pwr regulator failed %d\n", ret);
		return;
	}

	ret = regulator_set_enable_if_allowed(dev, true);
	if (ret) {
		printf("Enable regulator-m2-pwr regulator %d\n", ret);
		return;
	}
}

void lvds_backlight_on(void)
{
	/* None */
}

int board_init(void)
{
	int ret;
	ret = imx9_scmi_power_domain_enable(PD_HSIO_TOP, true);
	if (ret) {
		printf("SCMI_POWWER_STATE_SET Failed for USB\n");
		return ret;
	}

	imx9_scmi_power_domain_enable(PD_DISPLAY, false);
	imx9_scmi_power_domain_enable(PD_CAMERA, false);

	pcie_setup();

	netc_init();

	power_on_m7("mx952cplrpmsg");

	lvds_backlight_on();

#if IS_ENABLED(CONFIG_IMX_CRRM)
	crrm_uboot_init();
#endif

	return 0;
}

int board_late_init(void)
{
	const u64 jh_high_base = 0x180000000ULL;
	char jh_root_mem[64];
	u64 jh_high_size = 0;
	int i, ret;

	if (IS_ENABLED(CONFIG_ENV_IS_IN_MMC))
		board_late_mmc_env_init();

	env_set("sec_boot", "no");
#ifdef CONFIG_AHAB_BOOT
	env_set("sec_boot", "yes");
#endif
	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		u64 start = gd->bd->bi_dram[i].start;
		u64 end = start + gd->bd->bi_dram[i].size;

		if (start <= jh_high_base && end > jh_high_base) {
			jh_high_size = end - jh_high_base;
			break;
		}
	}

	if (jh_high_size)
		snprintf(jh_root_mem, sizeof(jh_root_mem),
			 "0x58000000@0x90000000,0x%llx@0x%llx",
			 jh_high_size, jh_high_base);
	else
		snprintf(jh_root_mem, sizeof(jh_root_mem),
			 "0x58000000@0x90000000");

	ret = env_set("jailhouse_root_mem", jh_root_mem);
	if (ret)
		return ret;

#if IS_ENABLED(CONFIG_IMX_CRRM)
	crrm_uboot_late_init();
#endif

	return 0;
}

#ifdef CONFIG_OF_BOARD_SETUP
static int ft_board_setup_compulab(void *blob)
{
	int ret;

	ret = fdt_set_env_addr(blob);
	if (ret)
		return ret;

	return fdt_set_sn(blob);
}

static int jh_mem_fdt_setup(void *blob)
{
	return fdt_fixup_jailhouse_memory(blob);
}

static int ft_board_setup_simplefb(void *blob)
{
	unsigned int display_phandle;
	int address_cells, size_cells;
	int chosen, framebuffer;
	int ret;

	if (!IS_ENABLED(CONFIG_FDT_SIMPLEFB))
		return 0;

	ret = fdt_increase_size(blob, 512);
	if (ret)
		return ret;

	chosen = fdt_find_or_add_subnode(blob, 0, "chosen");
	if (chosen < 0)
		return chosen;

	/*
	 * A simple-framebuffer node below /chosen needs an identity-mapped bus.
	 * Match the root cell sizes so Linux can decode its reg property.
	 */
	address_cells = fdt_address_cells(blob, 0);
	if (address_cells < 0)
		return address_cells;

	size_cells = fdt_size_cells(blob, 0);
	if (size_cells < 0)
		return size_cells;

	ret = fdt_setprop_u32(blob, chosen, "#address-cells", address_cells);
	if (ret)
		return ret;

	ret = fdt_setprop_u32(blob, chosen, "#size-cells", size_cells);
	if (ret)
		return ret;

	ret = fdt_setprop_empty(blob, chosen, "ranges");
	if (ret)
		return ret;

	framebuffer = fdt_node_offset_by_compatible(blob, -1,
						    "simple-framebuffer");
	if (framebuffer == -FDT_ERR_NOTFOUND) {
		framebuffer = fdt_add_subnode(blob, chosen, "framebuffer");
		if (framebuffer < 0)
			return framebuffer;
	} else if (framebuffer < 0) {
		return framebuffer;
	}

	ret = fdt_setprop_string(blob, framebuffer, "compatible",
				 "simple-framebuffer");
	if (ret)
		return ret;

	ret = fdt_setprop_string(blob, framebuffer, "status", "disabled");
	if (ret)
		return ret;

	display_phandle = fdt_create_phandle_by_compatible(blob,
							   "nxp,imx952-dpu");
	if (display_phandle) {
		ret = fdt_setprop_u32(blob, framebuffer, "display",
				      display_phandle);
		if (ret)
			return ret;
	}

	return fdt_simplefb_enable_and_mem_rsv(blob);
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	int ret;
	ret = jh_mem_fdt_setup(blob);
	if (ret) {
		printf("jailhouse memory process fail.\n");
		return ret;
	}

	/* Disable XSPI1 node for CRRM */
#if IS_ENABLED(CONFIG_IMX_CRRM)
	int nodeoff;
	const char *status = "disabled";

	nodeoff = fdt_path_offset(blob, "/soc/bus@42000000/spi@42400000");
	if (nodeoff > 0) {
		ret = fdt_increase_size(blob, 256);
		if (ret) {
			printf("Unable to increase fdt size, err=%s\n", fdt_strerror(ret));
			return ret;
		}

		ret = fdt_setprop(blob, nodeoff, "status", status,
				  strlen(status) + 1);
		if (ret) {
			printf("Unable to disable XSPI1, err=%s\n", fdt_strerror(ret));
			return ret;
		}
	}
#endif
	ret = ft_board_setup_simplefb(blob);
	if (ret)
		printf("Unable to set up simple framebuffer, err=%s\n",
		       fdt_strerror(ret));

	return ft_board_setup_compulab(blob);
}
#endif

void board_quiesce_devices(void)
{
	int ret;
	struct uclass *uc_dev;

	ret = imx9_scmi_power_domain_enable(PD_HSIO_TOP, false);
	if (ret) {
		printf("%s: Failed for HSIO MIX: %d\n", __func__, ret);
		return;
	}

	ret = imx9_scmi_power_domain_enable(PD_NETC, false);
	if (ret) {
		printf("%s: Failed for NETC MIX: %d\n", __func__, ret);
		return;
	}

	ret = uclass_get(UCLASS_SPI_FLASH, &uc_dev);
	if (uc_dev)
		ret = uclass_destroy(uc_dev);
	if (ret)
		printf("couldn't remove SPI FLASH devices\n");
}

#if IS_ENABLED(CONFIG_OF_BOARD_FIXUP)

static void disable_fdt_resources(void *fdt)
{
	int i = 0;
	int nodeoff, ret;
	const char *status = "disabled";
	static const char * const dsi_nodes[] = {
		"/soc/bus@42000000/i2c@422e0000",
		"/soc/netc-blk-ctrl@4cd20000"
	};

	for (i = 0; i < ARRAY_SIZE(dsi_nodes); i++) {
		nodeoff = fdt_path_offset(fdt, dsi_nodes[i]);
		if (nodeoff > 0) {
set_status:
			ret = fdt_setprop(fdt, nodeoff, "status", status,
					  strlen(status) + 1);
			if (ret == -FDT_ERR_NOSPACE) {
				ret = fdt_increase_size(fdt, 512);
				if (!ret)
					goto set_status;
			}
		}
	}
}

int board_fix_fdt(void *fdt)
{
	/* Remove nodes based on fuses. */
	board_fix_fdt_fuse(fdt);

	if (is_netc_cfg())
		disable_fdt_resources(fdt);

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
