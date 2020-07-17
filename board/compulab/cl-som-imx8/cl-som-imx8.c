/*
 * Copyright 2018 NXP
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
#include <asm/arch/imx8mq_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/boot_mode.h>
#include <spl.h>
#include <usb.h>
#include <dwc3-uboot.h>
#include "../common/eeprom.h"

#include <usb.h>
#include <dwc3-uboot.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_BOARD_POSTCLK_INIT
int board_postclk_init(void)
{
	/* TODO */
	return 0;
}
#endif

#define TCM_DATA_CFG 0x7e0000
int board_phys_sdram_size(phys_size_t *size)
{
    unsigned long value = readl(TCM_DATA_CFG);

    switch (value) {
    case 4096:
    case 2048:
    case 1024:
        *size = ( value << 20 );
        break;
    default:
	printf("%s: DRAM size %luM is not supported \n", __func__,
		value);
	while ( 1 ) {};
	break;
    };
    return 0;
}

/* Get the top of usable RAM */
ulong board_get_usable_ram_top(ulong total_size)
{

        if(gd->ram_top > 0x100000000)
            gd->ram_top = 0x100000000;

        return gd->ram_top;
}

int dram_init(void)
{
	phys_size_t sdram_size;
	int ret;
	ret = board_phys_sdram_size(&sdram_size);
	printf("%s: size %llu\n", __func__, sdram_size);
	if (ret)
		return ret;

	/* rom_pointer[1] contains the size of TEE occupies */
	gd->ram_size = sdram_size - rom_pointer[1];

	return 0;
}

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, bd_t *bd)
{
	return 0;
}
#endif

#ifdef CONFIG_FEC_MXC
#define FEC_RST_PAD IMX_GPIO_NR(1, 9)
static iomux_v3_cfg_t const fec1_rst_pads[] = {
	IMX8MQ_PAD_GPIO1_IO09__GPIO1_IO9 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#ifdef CPL_NET_DISC_LOGICS
static iomux_v3_cfg_t const fec1_default_pads[] = {
	IMX8MQ_PAD_ENET_MDC__ENET_MDC | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MQ_PAD_ENET_MDIO__ENET_MDIO | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const fec1_gpio_pads[] = {
	IMX8MQ_PAD_ENET_MDC__GPIO1_IO16 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MQ_PAD_ENET_MDIO__GPIO1_IO17 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static int get_fec_status(int mode)
{
	int rc = 0, i = 0;
	int gpios[] = { 16, 17 };
	static int fec_status = 0;
	if (mode) {
		fec_status = 0;
		/* Configure straps pins as GPIOs */
		imx_iomux_v3_setup_multiple_pads(fec1_gpio_pads, ARRAY_SIZE(fec1_gpio_pads));

		for ( i = 0 ; i < ARRAY_SIZE(gpios) ; i++ ) {
			char label[10];
			sprintf(label, "gpio_%d" , gpios[i]);
			gpio_request(IMX_GPIO_NR(1, gpios[i]), label);
			gpio_set_value(IMX_GPIO_NR(1, gpios[i]),1);
			rc = gpio_get_value(IMX_GPIO_NR(1, gpios[i]));
			fec_status += rc;
		}
		/* Restore the straps pins' configurations */
		imx_iomux_v3_setup_multiple_pads(fec1_default_pads, ARRAY_SIZE(fec1_default_pads));
	}
	return fec_status;
}
#endif

static void setup_iomux_fec(void)
{
	imx_iomux_v3_setup_multiple_pads(fec1_rst_pads, ARRAY_SIZE(fec1_rst_pads));

	gpio_request(IMX_GPIO_NR(1, 9), "fec1_rst");
	gpio_direction_output(IMX_GPIO_NR(1, 9), 0);
	udelay(500);
	gpio_direction_output(IMX_GPIO_NR(1, 9), 1);
}

/*
 * setup_mac_address() - set Ethernet MAC address environment.
 *
 * @return: 0 on success, -1 on failure
 */
static int setup_mac_address(void)
{
        int ret;
        unsigned char enetaddr[6];

        ret = eth_env_get_enetaddr("ethaddr", enetaddr);
        if (ret)
                return 0;

        ret = cl_eeprom_read_mac_addr(enetaddr, CONFIG_SYS_I2C_EEPROM_BUS);
        if (ret)
                return ret;

        ret = is_valid_ethaddr(enetaddr);
        if (!ret)
                return -1;

	ret = eth_env_set_enetaddr("ethaddr", enetaddr);
	if (ret)
		return -1;

        return 0;
}

static int setup_fec(void)
{
	struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs
		= (struct iomuxc_gpr_base_regs *) IOMUXC_GPR_BASE_ADDR;

	setup_iomux_fec();

	/* Use 125M anatop REF_CLK1 for ENET1, not from external */
	clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
			IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_SHIFT, 0);
	return set_clk_enet(ENET_125MHZ);
}

int board_phy_config(struct phy_device *phydev)
{

#ifdef CPL_NET_DISC_LOGICS
	if (!get_fec_status(1))
		env_set("ethprime" , "");
#endif

	/* enable rgmii rxc skew and phy mode select to RGMII copper */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x1f);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x8);

	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x05);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x100);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}
#endif

#ifdef CONFIG_USB_DWC3

#define USB_PHY_CTRL0			0xF0040
#define USB_PHY_CTRL0_REF_SSP_EN	BIT(2)

#define USB_PHY_CTRL1			0xF0044
#define USB_PHY_CTRL1_RESET		BIT(0)
#define USB_PHY_CTRL1_COMMONONN		BIT(1)
#define USB_PHY_CTRL1_ATERESET		BIT(3)
#define USB_PHY_CTRL1_VDATSRCENB0	BIT(19)
#define USB_PHY_CTRL1_VDATDETENB0	BIT(20)

#define USB_PHY_CTRL2			0xF0048
#define USB_PHY_CTRL2_TXENABLEN0	BIT(8)

static struct dwc3_device dwc3_device_data = {
#ifdef CONFIG_SPL_BUILD
	.maximum_speed = USB_SPEED_HIGH,
#else
	.maximum_speed = USB_SPEED_SUPER,
#endif
	.base = USB1_BASE_ADDR,
	.dr_mode = USB_DR_MODE_HOST,
	.index = 0,
	.power_down_scale = 2,
};

int usb_gadget_handle_interrupts(void)
{
	dwc3_uboot_handle_interrupt(0);
	return 0;
}

static void dwc3_nxp_usb_phy_init(struct dwc3_device *dwc3)
{
	u32 RegData;

	RegData = readl(dwc3->base + USB_PHY_CTRL1);
	RegData &= ~(USB_PHY_CTRL1_VDATSRCENB0 | USB_PHY_CTRL1_VDATDETENB0 |
			USB_PHY_CTRL1_COMMONONN);
	RegData |= USB_PHY_CTRL1_RESET | USB_PHY_CTRL1_ATERESET;
	writel(RegData, dwc3->base + USB_PHY_CTRL1);

	RegData = readl(dwc3->base + USB_PHY_CTRL0);
	RegData |= USB_PHY_CTRL0_REF_SSP_EN;
	writel(RegData, dwc3->base + USB_PHY_CTRL0);

	RegData = readl(dwc3->base + USB_PHY_CTRL2);
	RegData |= USB_PHY_CTRL2_TXENABLEN0;
	writel(RegData, dwc3->base + USB_PHY_CTRL2);

	RegData = readl(dwc3->base + USB_PHY_CTRL1);
	RegData &= ~(USB_PHY_CTRL1_RESET | USB_PHY_CTRL1_ATERESET);
	writel(RegData, dwc3->base + USB_PHY_CTRL1);
}
#endif

#if defined(CONFIG_USB_DWC3) || defined(CONFIG_USB_XHCI_IMX8M)
int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;
	imx8m_usb_power(index, true);

	if (index == 0 && init == USB_INIT_DEVICE) {
		dwc3_nxp_usb_phy_init(&dwc3_device_data);
		return dwc3_uboot_init(&dwc3_device_data);
	} else if (index == 0 && init == USB_INIT_HOST) {
		return ret;
	}

	return 0;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;
	if (index == 0 && init == USB_INIT_DEVICE)
		dwc3_uboot_exit(index);

	imx8m_usb_power(index, false);

	return ret;
}
#endif

static iomux_v3_cfg_t const usbmux_pads[] = {
	IMX8MQ_PAD_GPIO1_IO04__GPIO1_IO4 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_iomux_usbmux(void)
{
	imx_iomux_v3_setup_multiple_pads(usbmux_pads, ARRAY_SIZE(usbmux_pads));

	gpio_request(IMX_GPIO_NR(1, 4), "usb_mux");
	gpio_direction_output(IMX_GPIO_NR(1, 4), 0);
}

static void setup_usbmux(void)
{
	setup_iomux_usbmux();
}

int board_init(void)
{

#ifdef CONFIG_MXC_SPI
	board_ecspi_init();
#endif

#ifdef CONFIG_FEC_MXC
	setup_fec();
#endif
	setup_usbmux();

#if defined(CONFIG_USB_DWC3) || defined(CONFIG_USB_XHCI_IMX8M)
	init_usb_clk();
#endif
	return 0;
}

int board_mmc_get_env_dev(int devno)
{
	const char *s = env_get("atp");
	if (s != NULL) {
		printf("ATP Mode: Save environmet on eMMC\n");
		return CONFIG_SYS_MMC_ENV_DEV;
	}
	return devno;
}

uint mmc_get_env_part(struct mmc *mmc)
{
	if (get_boot_device() == MMC1_BOOT)
		return CONFIG_SYS_MMC_ENV_PART;

	return 0;
}

static void board_bootdev_init(void)
{
	u32 bootdev = get_boot_device();
	switch (bootdev) {
	case MMC1_BOOT:
		bootdev = 0;
		break;
	case SD2_BOOT:
		bootdev = 1;
		break;
	default:
		env_set("bootdev", NULL);
		return;
	}
	env_set_ulong("bootdev", bootdev);
}

int board_late_init(void)
{
	int ret;

#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif
	board_bootdev_init();

	ret = setup_mac_address();
	if (ret < 0)
		printf("%s: Can't set MAC address\n", __func__);

	return 0;
}

#ifdef CONFIG_FSL_FASTBOOT
#ifdef CONFIG_ANDROID_RECOVERY
int is_recovery_key_pressing(void)
{
	return 0; /*TODO*/
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/

#ifdef CONFIG_ANDROID_SUPPORT
bool is_power_key_pressed(void) {
	return (bool)(!!(readl(SNVS_HPSR) & (0x1 << 6)));
}
#endif

