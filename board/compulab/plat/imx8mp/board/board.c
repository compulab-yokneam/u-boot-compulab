// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 NXP
 */

#include <common.h>
#include <env.h>
#include <errno.h>
#include <init.h>
#include <miiphy.h>
#include <netdev.h>
#include <linux/delay.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <asm/arch/imx8mp_pins.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <spl.h>
#include <led.h>
#include <pwm.h>
#include <asm/mach-imx/dma.h>
#include <power/pmic.h>
#include "common/tcpc.h"
#include "common/fdt.h"
#include <usb.h>
#include <dwc3-uboot.h>
#include <imx_sip.h>
#include <linux/arm-smccc.h>
#include "ddr/ddr.h"
#include "common/eeprom.h"
#include "cmd.h"

DECLARE_GLOBAL_DATA_PTR;

int board_phys_sdram_size(phys_size_t *size)
{
	size_t dramsize;
	if (!size)
		return -EINVAL;

	dramsize = lppdr4_get_ramsize();

	*size = ((1L << 20) * dramsize );

	return 0;
}


#ifdef CONFIG_OF_BOARD_SETUP
__weak int fdt_board_vendor_setup(void *blob) {
	return 0;
}

__weak void board_save_phyaddr(int phy_addr) {
	return;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{

	fdt_set_env_addr(blob);
	fdt_set_sn(blob);
	fdt_board_vendor_setup(blob);
	return 0;
}
#endif

#ifdef CONFIG_DWC_ETH_QOS
static int setup_eqos(void)
{
	struct iomuxc_gpr_base_regs *gpr =
		(struct iomuxc_gpr_base_regs *)IOMUXC_GPR_BASE_ADDR;

	/* set INTF as RGMII, enable RGMII TXC clock */
	clrsetbits_le32(&gpr->gpr[1],
			IOMUXC_GPR_GPR1_GPR_ENET_QOS_INTF_SEL_MASK, BIT(16));
	setbits_le32(&gpr->gpr[1], BIT(19) | BIT(21));

	return set_clk_eqos(ENET_125MHZ);
}
#endif

#if defined(CONFIG_FEC_MXC) || defined(CONFIG_DWC_ETH_QOS)
static int mx8_rgmii_rework(struct phy_device *phydev);
int board_phy_config(struct phy_device *phydev)
{
	mx8_rgmii_rework(phydev);

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

#define USB_PHY_CTRL6			0xF0058

#define HSIO_GPR_BASE                               (0x32F10000U)
#define HSIO_GPR_REG_0                              (HSIO_GPR_BASE)
#define HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN_SHIFT    (1)
#define HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN          (0x1U << HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN_SHIFT)


static struct dwc3_device dwc3_device_data = {
#ifdef CONFIG_SPL_BUILD
	.maximum_speed = USB_SPEED_HIGH,
#else
	.maximum_speed = USB_SPEED_SUPER,
#endif
	.base = USB1_BASE_ADDR,
	.dr_mode = USB_DR_MODE_PERIPHERAL,
	.index = 0,
	.power_down_scale = 2,
};

int usb_gadget_handle_interrupts(int index)
{
	dwc3_uboot_handle_interrupt(index);
	return 0;
}

static void dwc3_nxp_usb_phy_init(struct dwc3_device *dwc3)
{
	u32 RegData;

	/* enable usb clock via hsio gpr */
	RegData = readl(HSIO_GPR_REG_0);
	RegData |= HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN;
	writel(RegData, HSIO_GPR_REG_0);

	/* USB3.0 PHY signal fsel for 100M ref */
	RegData = readl(dwc3->base + USB_PHY_CTRL0);
	RegData = (RegData & 0xfffff81f) | (0x2a<<5);
	writel(RegData, dwc3->base + USB_PHY_CTRL0);

	RegData = readl(dwc3->base + USB_PHY_CTRL6);
	RegData &=~0x1;
	writel(RegData, dwc3->base + USB_PHY_CTRL6);

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
#define USB2_PWR_EN IMX_GPIO_NR(1, 14)
int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;
	imx8m_usb_power(index, true);

	if (index == 0 && init == USB_INIT_DEVICE) {
		dwc3_nxp_usb_phy_init(&dwc3_device_data);
		return dwc3_uboot_init(&dwc3_device_data);
	} else if (index == 0 && init == USB_INIT_HOST) {
		return ret;
	} else if (index == 1 && init == USB_INIT_HOST) {
		return ret;
	}

	return 0;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;
	if (index == 0 && init == USB_INIT_DEVICE) {
		dwc3_uboot_exit(index);
	}

	imx8m_usb_power(index, false);

	return ret;
}
#endif

#define FSL_SIP_GPC			0xC2000000
#define FSL_SIP_CONFIG_GPC_PM_DOMAIN	0x3
#define DISPMIX				13
#define MIPI				15

__weak void board_vendor_init(void) {
	return;
}

__weak void board_vendor_late_init(void) {
	return;
}

void static board_pm_init(void)
{
	struct arm_smccc_res res;
	/* enable the dispmix & mipi phy power domain */
	arm_smccc_smc(IMX_SIP_GPC, IMX_SIP_GPC_PM_DOMAIN,
		      DISPMIX, true, 0, 0, 0, 0, &res);
	arm_smccc_smc(IMX_SIP_GPC, IMX_SIP_GPC_PM_DOMAIN,
		      MIPI, true, 0, 0, 0, 0, &res);
}

int board_init(void)
{
	board_pm_init();

	board_vendor_init();

#ifdef CONFIG_DWC_ETH_QOS
	setup_eqos();
#endif

#if defined(CONFIG_USB_DWC3) || defined(CONFIG_USB_XHCI_IMX8M)
	init_usb_clk();
#endif

	return 0;
}

int board_late_init(void)
{
#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", CONFIG_SYS_BOARD);
	env_set("board_rev", "iMX8MP");
#endif

#ifdef CONFIG_DRAM_D2D4
	env_set("dram_subset", "d2d4");
#else
	env_set("dram_subset", "d1d8");
#endif
	board_vendor_late_init();

	do_pbb_restore();
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

#ifdef CONFIG_SPL_MMC_SUPPORT

#define UBOOT_RAW_SECTOR_OFFSET 0x40
unsigned long spl_mmc_get_uboot_raw_sector(struct mmc *mmc)
{
	u32 boot_dev = spl_boot_device();
	switch (boot_dev) {
		case BOOT_DEVICE_MMC2:
			return CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR - UBOOT_RAW_SECTOR_OFFSET;
		default:
			return CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR;
	}
}
#endif

#define PHY_VENDOR_ID_MASK (( 1<<5 ) - 1 )
#define PHY_ATEROS_ID  0x7
#define PHY_REALTEK_ID 0x11

int get_phy_id(struct mii_dev *bus, int addr, int devad, u32 *phy_id)
{
	int phy_reg;

	/*
	 * Grab the bits from PHYIR1, and put them
	 * in the upper half
	 */
	phy_reg = bus->read(bus, addr, devad, MII_PHYSID1);

	if (phy_reg < 0)
		return -EIO;

	*phy_id = (phy_reg & 0xffff) << 16;

	/* Grab the bits from PHYIR2, and put them in the lower half */
	phy_reg = bus->read(bus, addr, devad, MII_PHYSID2);

	if (phy_reg < 0)
		return -EIO;

	*phy_id |= (phy_reg & 0xffff);

#ifdef CONFIG_TARGET_SOM_IMX8M_PLUS
	/* Specical case for REALTEK */
	phy_reg = (( phy_reg >> 4 ) & PHY_VENDOR_ID_MASK);
	if ((addr ==  0) &&  (phy_reg == PHY_REALTEK_ID)) {
		return -ENODEV;
	}
#endif
	return 0;
}

static int mx8_rgmii_rework_realtek(struct phy_device *phydev)
{
#define TXDLY_MASK ((1 << 13) | (1 << 12))
#define RXDLY_MASK ((1 << 13) | (1 << 11))

	unsigned short val;

	/* introduce tx clock delay */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x7);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0xa4);

	val = phy_read(phydev, MDIO_DEVAD_NONE, 0x1c);
	val |= TXDLY_MASK;
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1c, val);

	/* introduce rx clock delay */
	val = phy_read(phydev, MDIO_DEVAD_NONE, 0x1c);
	val |= RXDLY_MASK;
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1c, val);

	phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x0);

	/*LEDs:*/
	/* set to extension page */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x0007);
	/* extension Page44 */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x002c);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1c, 0x0430);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1a, 0x0010);
	/* To disable EEE LED mode (blinking .4s/2s) */
	/* extension Page5 */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x0005);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x05, 0x8b82);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x06, 0x052b);

	phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x00);
	return 0;
}

static int mx8_rgmii_rework(struct phy_device *phydev)
{
	unsigned short val = phy_read(phydev, MDIO_DEVAD_NONE, 0x3);

	val = (( val >> 4 ) & PHY_VENDOR_ID_MASK);

	switch (val) {
	case PHY_ATEROS_ID:
		break;
	case PHY_REALTEK_ID:
		mx8_rgmii_rework_realtek(phydev);
		break;
	default:
		break;
	}

	board_save_phyaddr(phydev->addr);
	return 0;
}
