// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 CompuLab LTD
 */

#include <common.h>
#include <env.h>
#include <init.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/global_data.h>
#include <asm/arch-imx9/ccm_regs.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch-imx9/imx93_pins.h>
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
#define WDOG_PAD_CTRL	(PAD_CTL_DSE(6) | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)

static iomux_v3_cfg_t const uart_pads[] = {
	MX93_PAD_UART1_RXD__LPUART1_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX93_PAD_UART1_TXD__LPUART1_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static iomux_v3_cfg_t const gpio_pads[] = {
	MX93_PAD_PDM_CLK__GPIO1_IO08 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

int board_early_init_f(void)
{
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

	init_uart_clk(LPUART1_CLK_ROOT);

	return 0;
}

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;

	debug("board_usb_init %d, type %d\n", index, init);

	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	debug("board_usb_cleanup %d, type %d\n", index, init);

	return ret;
}

int board_ehci_usb_phy_mode(struct udevice *dev)
{
	int ret = 0;
	debug("%s %d\n", __func__, dev_seq(dev));

	if (dev_seq(dev) == 0)
		ret=USB_INIT_DEVICE;
	else
		ret=USB_INIT_HOST;

	return ret;
}

static int setup_fec(void)
{
	return set_clk_enet(ENET_125MHZ);
}

int board_phy_config(struct phy_device *phydev)
{
	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

static int setup_eqos(void)
{
	struct blk_ctrl_wakeupmix_regs *bctrl =
		(struct blk_ctrl_wakeupmix_regs *)BLK_CTRL_WAKEUPMIX_BASE_ADDR;

	/* set INTF as RGMII, enable RGMII TXC clock */
	clrsetbits_le32(&bctrl->eqos_gpr,
			BCTRL_GPR_ENET_QOS_INTF_MODE_MASK,
			BCTRL_GPR_ENET_QOS_INTF_SEL_RGMII | BCTRL_GPR_ENET_QOS_CLK_GEN_EN);

	return set_clk_eqos(ENET_125MHZ);
}

static void board_gpio_init(void)
{
	int ret;
	struct gpio_desc desc;
	struct udevice *dev;

	imx_iomux_v3_setup_multiple_pads(gpio_pads, ARRAY_SIZE(gpio_pads));

	/* enable i2c port expander assert reset line first */
	/* we can't use dm_gpio_lookup_name for GPIO1_8, because the func will probe the
	 * uclass list until find the device. The expander device is at begin of the list due to
	 * I2c nodes is prior than gpio in the DTS. So if the func goes through the uclass list,
	 * probe to expander will fail, and exit the dm_gpio_lookup_name func. Thus, we always
	 * fail to get the device
	*/
	ret = uclass_get_device_by_seq(UCLASS_GPIO, 0, &dev);
	if (ret) {
		printf("%s failed to find GPIO1 device, ret = %d\n", __func__, ret);
		return;
	}

	desc.dev = dev;
	desc.offset = 8;
	desc.flags = 0;

	ret = dm_gpio_request(&desc, "EXP_nPWREN");
	if (ret) {
		printf("%s request ioexp_rst failed ret = %d\n", __func__, ret);
		return;
	}
	dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	dm_gpio_set_value(&desc, 1);
}

#if defined(CONFIG_FEC_MXC) || defined(CONFIG_DWC_ETH_QOS)
static void board_get_mac_from_eeprom(int dev_id) {
	unsigned char mac[6];
	cl_eeprom_read_n_mac_addr(mac, dev_id, CONFIG_SYS_I2C_EEPROM_BUS);

	if (is_zero_ethaddr(mac) || !is_valid_ethaddr(mac))
		net_random_ethaddr(mac);

	eth_env_set_enetaddr_by_index("eth",dev_id,mac);
	return;
}
#else
static void board_get_mac_from_eeprom(int dev_id) { return;}
#endif

int board_init(void)
{
	if (IS_ENABLED(CONFIG_FEC_MXC))
		setup_fec();

	if (IS_ENABLED(CONFIG_DWC_ETH_QOS))
		setup_eqos();

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
	env_set("board_name", "UCM-iMX93");
	env_set("board_rev", "iMX93");
#endif
	board_get_mac_from_eeprom(0);
	board_get_mac_from_eeprom(1);

	printf("%s [ 0x%llx ]\n",__func__,gd->ram_size);
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

#ifdef CONFIG_OF_BOARD_SETUP
static void fdt_set_sn(void *blob)
{
	u32 rev;
	char buf[100];
	int len;
	union {
		struct tag_serialnr	s;
		u64			u;
	} serialnr;

	len = cl_eeprom_read_som_name(buf);
	fdt_setprop(blob, 0, "product-name", buf, len);

	len = cl_eeprom_read_sb_name(buf);
	fdt_setprop(blob, 0, "baseboard-name", buf, len);

	cpl_get_som_serial(&serialnr.s);
	fdt_setprop(blob, 0, "product-sn", buf, sprintf(buf, "%llx", serialnr.u) + 1);

	cpl_get_sb_serial(&serialnr.s);
	fdt_setprop(blob, 0, "baseboard-sn", buf, sprintf(buf, "%llx", serialnr.u) + 1);

	rev = cl_eeprom_get_som_revision();
	fdt_setprop(blob, 0, "product-revision", buf,
		sprintf(buf, "%u.%02u", rev/100 , rev%100 ) + 1);

	rev = cl_eeprom_get_sb_revision();
	fdt_setprop(blob, 0, "baseboard-revision", buf,
		sprintf(buf, "%u.%02u", rev/100 , rev%100 ) + 1);

	len = cl_eeprom_read_som_options(buf);
	fdt_setprop(blob, 0, "product-options", buf, len);

	len = cl_eeprom_read_sb_options(buf);
	fdt_setprop(blob, 0, "baseboard-options", buf, len);

	return;
}

static int env_dev = -1;
static int env_part= -1;

static int fdt_set_env_addr(void *blob)
{
	char tmp[32];
	int nodeoff = fdt_add_subnode(blob, 0, "fw_env");
	if(0 > nodeoff)
		return nodeoff;

	fdt_setprop(blob, nodeoff, "env_off", tmp, sprintf(tmp, "0x%x", CONFIG_ENV_OFFSET));
	fdt_setprop(blob, nodeoff, "env_size", tmp, sprintf(tmp, "0x%x", CONFIG_ENV_SIZE));
	if(0 < env_dev) {
		switch(env_part) {
		case 1 ... 2:
			fdt_setprop(blob, nodeoff, "env_dev", tmp, sprintf(tmp, "/dev/mmcblk%iboot%i", env_dev, env_part - 1));
			break;
		default:
			fdt_setprop(blob, nodeoff, "env_dev", tmp, sprintf(tmp, "/dev/mmcblk%i", env_dev));
			break;
		}
	}
	return 0;
}

int disable_npu_nodes(void *blob);
int disable_m33_nodes(void *blob);
int disable_ele_nodes(void *blob);

int ft_board_setup(void *blob, struct bd_info *bd)
{
	phys_size_t sdram_size;
	int error = board_phys_sdram_size(&sdram_size);

	fdt_set_env_addr(blob);
	fdt_set_sn(blob);
	if (error) {
		return error;
	}
	/* todo - below is bad code because it rely on linux device tree which is fragile. eliminate ASAP! */
	if (sdram_size < 0x80000000) {
		printf("disable ethosu because its address is mapped above 1GB\n");
		disable_npu_nodes(blob);
	}
	if (sdram_size < 0x40000000) {
		printf("disable M33 because its shared address is mapped above 500MB, you can enable it after changing this address in M33 code and in Linux device tree\n");
		disable_m33_nodes(blob);
		printf("disable ELE because its shared address is mapped above 500MB\n");
		disable_ele_nodes(blob);
	}
	return 0;
}
#endif
