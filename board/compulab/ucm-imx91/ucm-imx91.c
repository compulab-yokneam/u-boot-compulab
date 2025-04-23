// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 NXP
 */

#include <common.h>
#include <env.h>
#include <efi_loader.h>
#include <init.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/global_data.h>
#include <asm/arch-imx9/ccm_regs.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch-imx9/imx91_pins.h>
#include <asm/arch/clock.h>
#include <power/pmic.h>
#include "../common/tcpc.h"
#include <dm/device.h>
#include <dm/uclass.h>
#include <usb.h>
#include <dwc3-uboot.h>
#include <asm/gpio.h>
#include "../common/eeprom.h"
#include "ddr/ddr.h"

DECLARE_GLOBAL_DATA_PTR;

#define UART_PAD_CTRL	(PAD_CTL_DSE(6) | PAD_CTL_FSEL2)
#define LCDIF_GPIO_PAD_CTRL	(PAD_CTL_DSE(0xf) | PAD_CTL_FSEL2 | PAD_CTL_PUE)

static iomux_v3_cfg_t const uart_pads[] = {
	MX91_PAD_UART1_RXD__LPUART1_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX91_PAD_UART1_TXD__LPUART1_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static iomux_v3_cfg_t const lcdif_gpio_pads[] = {
	MX91_PAD_GPIO_IO00__GPIO2_IO0| MUX_PAD_CTRL(LCDIF_GPIO_PAD_CTRL),
	MX91_PAD_GPIO_IO01__GPIO2_IO1 | MUX_PAD_CTRL(LCDIF_GPIO_PAD_CTRL),
	MX91_PAD_GPIO_IO02__GPIO2_IO2 | MUX_PAD_CTRL(LCDIF_GPIO_PAD_CTRL),
	MX91_PAD_GPIO_IO03__GPIO2_IO3 | MUX_PAD_CTRL(LCDIF_GPIO_PAD_CTRL),
};

#if CONFIG_IS_ENABLED(EFI_HAVE_CAPSULE_SUPPORT)
#define IMX_BOOT_IMAGE_GUID \
	EFI_GUID(0xbc550d86, 0xda26, 0x4b70, 0xac, 0x05, \
		 0x2a, 0x44, 0x8e, 0xda, 0x6f, 0x21)

struct efi_fw_image fw_images[] = {
	{
		.image_type_id = IMX_BOOT_IMAGE_GUID,
		.fw_name = u"UCM-IMX91-RAW",
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
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

	init_uart_clk(LPUART1_CLK_ROOT);

	return 0;
}


size_t lppdr4_get_ramsize() {
	struct lpddr4_tcm_desc *desc = (void *) SHARED_DDR_INFO;
	if (desc)
        return desc->size;
    return 0;
}

int board_phys_sdram_size(phys_size_t *size)
{
	size_t dramsize;
	if (!size)
		return -EINVAL;
	dramsize = lppdr4_get_ramsize();
	//*size = get_ram_size((void *)PHYS_SDRAM, PHYS_SDRAM_SIZE);
	*size = ((1L << 20) * dramsize );
	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

#if defined(CONFIG_FEC_MXC) || defined(CONFIG_DWC_ETH_QOS)
void imx_get_mac_from_fuse(int dev_id, unsigned char *mac)
{
	cl_eeprom_read_n_mac_addr(mac, dev_id, CONFIG_SYS_I2C_EEPROM_BUS);
	debug("%s: MAC%d: %02x.%02x.%02x.%02x.%02x.%02x\n",
	      __func__, dev_id, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return;
}
#endif

struct gpio_desc ext_pwren_desc, exp_sel_desc;

static void board_gpio_init(void)
{
	struct gpio_desc desc;
	int ret;

	/* Enable EXT1_PWREN for PCIE_3.3V */
	ret = dm_gpio_lookup_name("gpio@22_13", &desc);
	if (ret)
		return;

	ret = dm_gpio_request(&desc, "EXT1_PWREN");
	if (ret)
		return;

	dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT);
	dm_gpio_set_value(&desc, 1);

	/* Deassert SD3_nRST */
	ret = dm_gpio_lookup_name("gpio@22_12", &desc);
	if (ret)
		return;

	ret = dm_gpio_request(&desc, "SD3_nRST");
	if (ret)
		return;

	dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT);
	dm_gpio_set_value(&desc, 1);

	/* Enable EXT_PWREN for vRPi 5V */
	ret = dm_gpio_lookup_name("gpio@22_8", &ext_pwren_desc);
	if (ret)
		return;

	ret = dm_gpio_request(&ext_pwren_desc, "EXT_PWREN");
	if (ret)
		return;

	dm_gpio_set_dir_flags(&ext_pwren_desc, GPIOD_IS_OUT);
	dm_gpio_set_value(&ext_pwren_desc, 1);

	ret = dm_gpio_lookup_name("adp5585-gpio4", &exp_sel_desc);
	if (ret)
		return;

	ret = dm_gpio_request(&exp_sel_desc, "EXP_SEL");
	if (ret)
		return;

	dm_gpio_set_dir_flags(&exp_sel_desc, GPIOD_IS_OUT);
	dm_gpio_set_value(&exp_sel_desc, 1);
}

int board_init(void)
{
	board_gpio_init();

	return 0;
}

int board_late_init(void)
{
#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif

	env_set("sec_boot", "no");
#ifdef CONFIG_AHAB_BOOT
	env_set("sec_boot", "yes");
#endif

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", "UCM-iMX91");
	env_set("board_rev", "iMX91");
#endif
	return 0;
}

void board_quiesce_devices(void)
{
	/* Turn off 5V for backlight */
	dm_gpio_set_value(&ext_pwren_desc, 0);

	/* Turn off MUX for rpi */
	dm_gpio_set_value(&exp_sel_desc, 0);
}
