/*
 * Copyright 2020 CompuLab
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <hang.h>
#include <asm/io.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/clock.h>
#include <spl.h>
#include <led.h>
#include <asm/mach-imx/dma.h>
#include <power/pmic.h>
#include <power/bd71837.h>
#include <usb.h>
#include <asm/setup.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/video.h>
#include <imx_sip.h>
#include <linux/arm-smccc.h>
#include <linux/delay.h>
#include "ddr/ddr.h"
#include "common/eeprom.h"
#include "common/rtc.h"
#include "common/fdt.h"
#include <asm-generic/u-boot.h>
#include <fdt_support.h>

DECLARE_GLOBAL_DATA_PTR;

static int env_dev = -1;
static int env_part= -1;
static int fec_phyaddr = -1;

#ifdef CONFIG_BOARD_POSTCLK_INIT
int board_postclk_init(void)
{
	/* TODO */
	return 0;
}
#endif
int board_phys_sdram_size(phys_size_t *size)
{
	struct lpddr4_tcm_desc *lpddr4_tcm_desc = (struct lpddr4_tcm_desc *) TCM_DATA_CFG;

	switch (lpddr4_tcm_desc->size) {
	case 4096:
	case 2048:
	case 1024:
		*size = (1L << 20) * lpddr4_tcm_desc->size;
		break;
	default:
		printf("%s: DRAM size %uM is not supported \n", __func__,
				lpddr4_tcm_desc->size);
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

#ifdef CONFIG_OF_BOARD_SETUP
__weak int sub_ft_board_setup(void *blob, struct bd_info *bd)
{
	return 0;
}

#define FDT_PHYADDR "/soc@0/bus@30800000/ethernet@30be0000/mdio/ethernet-phy@0"
#define FLIP_32B(val) (((val>>24)&0xff) | ((val<<8)&0xff0000) | ((val>>8)&0xff00) | ((val<<24)&0xff000000))
static int fdt_set_fec_phy_addr(void *blob)
{
	if(0 > fec_phyaddr)
		return -EINVAL;

	u32 val = FLIP_32B(fec_phyaddr);
	return fdt_find_and_setprop
		(blob, FDT_PHYADDR, "reg", (const void*)&val, sizeof(val), 0);
}

static int fdt_set_ram_size(void *blob)
{
	char tmp[32];
	struct lpddr4_tcm_desc *lpddr4_tcm_desc = (struct lpddr4_tcm_desc *) TCM_DATA_CFG;
	int nodeoff = fdt_add_subnode(blob, 0, "dram");

	if(0 > nodeoff)
		return nodeoff;

	fdt_setprop(blob, nodeoff, "size", tmp, sprintf(tmp, "%llu", (1L << 20) * lpddr4_tcm_desc->size));
	fdt_setprop(blob, nodeoff, "id", tmp, sprintf(tmp, "0x%x", lpddr4_tcm_desc->sign));
	return 0;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	fdt_set_env_addr(blob);
	fdt_set_sn(blob);
	fdt_set_ram_size(blob);
	fdt_set_fec_phy_addr(blob);
	return sub_ft_board_setup(blob, bd);
}
#endif

#ifdef CONFIG_FEC_MXC
#define FEC_RST_PAD IMX_GPIO_NR(1, 10)
static iomux_v3_cfg_t const fec1_rst_pads[] = {
	IMX8MM_PAD_GPIO1_IO10_GPIO1_IO10 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_iomux_fec(void)
{
	imx_iomux_v3_setup_multiple_pads(fec1_rst_pads,
					 ARRAY_SIZE(fec1_rst_pads));

	gpio_request(FEC_RST_PAD, "fec1_rst");
	gpio_direction_output(FEC_RST_PAD, 0);
	udelay(500);
	gpio_direction_output(FEC_RST_PAD, 1);
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

/* These are specifc ID, purposed to distiguish between PHY vendors.
These values are not equal to real vendors' OUI (half of MAC address) */
#define OUI_PHY_ATHEROS 0x1374
#define OUI_PHY_REALTEK 0x0732

int board_phy_config(struct phy_device *phydev)
{
	int phyid1, phyid2;
	unsigned int model, rev, oui;
	unsigned int reg;

	phyid1 = phy_read(phydev, MDIO_DEVAD_NONE, MII_PHYSID1);
	if(0 > phyid1) {
		printf("%s: PHYID1 registry read fail %i\n", __func__, phyid1);
		return phyid1;
	}

	phyid2 = phy_read(phydev, MDIO_DEVAD_NONE, MII_PHYSID2);
	if(0 > phyid2) {
		printf("%s: PHYID2 registry read fail %i\n", __func__, phyid2);
		return phyid2;
	}

	reg = phyid2 | phyid1 << 16;
	if(0xffff == reg) {
		printf("%s: There is no device @%i\n", __func__, phydev->addr);
		return -ENODEV;
	}

	rev = reg & 0xf;
	reg >>= 4;
	model = reg & 0x3f;
	reg >>=6;
	oui = reg;
	debug("%s: PHY @0x%x OUI 0x%06x model 0x%x rev 0x%x\n",
		__func__, phydev->addr, oui, model, rev);

	switch (oui) {
	case OUI_PHY_ATHEROS:
		/* enable rgmii rxc skew and phy mode select to RGMII copper */
		printf("phy: AR803x@%x\t", phydev->addr);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x1f);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x8);

		phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x00);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x82ee);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x05);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x100);
		break;
	case OUI_PHY_REALTEK:
		printf("phy: RTL8211E@%x\t", phydev->addr);
		/** RTL8211E-VB-CG - add TX and RX delay */
		unsigned short val;

		phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x07);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0xa4);
		val = phy_read(phydev, MDIO_DEVAD_NONE, 0x1c);
		val |= (0x1 << 13) | (0x1 << 12) | (0x1 << 11);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1c, val);
		/*LEDs:*/
		/* set to extension page */
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x0007);
		/* extension Page44 */
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x002c);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1c, 0x0430);//LCR
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1a, 0x0010);//LACR
		/* To disable EEE LED mode (blinking .4s/2s) */
		/* extension Page5 */
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x0005);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x05, 0x8b82);//magic const
		phy_write(phydev, MDIO_DEVAD_NONE, 0x06, 0x052b);//magic const

		phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x00);// Back to Page0

		val = phy_read(phydev, MDIO_DEVAD_NONE, 0x10);
		val |= 0x10;
		phy_write(phydev, MDIO_DEVAD_NONE, 0x10, val);// Turn off the PHY CLK 125 MHz

		break;
	default:
		printf("%s: ERROR: unknown PHY @0x%x OUI 0x%06x model 0x%x rev 0x%x\n",
			__func__, phydev->addr, oui, model, rev);
		return -ENOSYS;
	}

	fec_phyaddr = phydev->addr;

	if (phydev->drv->config)
		phydev->drv->config(phydev);
	return 0;
}
#endif

int board_usb_init(int index, enum usb_init_type init)
{
	debug("board_usb_init %d, type %d\n", index, init);

	imx8m_usb_power(index, true);

	return 0;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	debug("board_usb_cleanup %d, type %d\n", index, init);

	imx8m_usb_power(index, false);

	return 0;
}

__weak int uboot_board_private_init(void) {
	return 0;
}

static void show_suite_info(void)
{
	char *buf = malloc(200); //More than necessary

	if(!buf) {
		printf("%s: Not enough memory\n", __func__);
		return;
	}

	cl_eeprom_get_suite(buf);
	printf("Suite:\t%s\n", buf);

	free(buf);
	return;
}

static void disable_rtc_bus_on_battery(void)
{
	struct udevice *bus, *dev;
	int ret;

	ret = uclass_get_device_by_seq(UCLASS_I2C, CONFIG_SYS_I2C_RTC_BUS, &bus);
	if (ret) {
		printf("%s: No bus %d\n", __func__, CONFIG_SYS_I2C_RTC_BUS);
		return;
	}

	ret = dm_i2c_probe(bus, CONFIG_SYS_I2C_RTC_ADDR, 0, &dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
			__func__, CONFIG_SYS_I2C_RTC_BUS, CONFIG_SYS_I2C_RTC_ADDR);
		return;
	}

	if((ret = dm_i2c_reg_write(dev, ABX8XX_REG_CFG_KEY, ABX8XX_CFG_KEY_MISC)) ||
	    (ret = dm_i2c_reg_write(dev, ABX8XX_REG_BATMODE, ABX8XX_BATMODE_IOBM_NOT)))
		printf("%s: i2c write error %d\n", __func__, ret);

	return;
}

#define FSL_SIP_GPC			0xC2000000
#define FSL_SIP_CONFIG_GPC_PM_DOMAIN	0x3
#define DISPMIX				9
#define MIPI				10

int board_init(void)
{

	struct arm_smccc_res res;

	disable_rtc_bus_on_battery();

#ifdef CONFIG_FEC_MXC
	setup_fec();
#endif
	if (uboot_board_private_init()) {
		printf("uboot_board_private_init() failed\n");
		hang();
	}

	arm_smccc_smc(IMX_SIP_GPC, IMX_SIP_GPC_PM_DOMAIN,
		      DISPMIX, true, 0, 0, 0, 0, &res);
	arm_smccc_smc(IMX_SIP_GPC, IMX_SIP_GPC_PM_DOMAIN,
		      MIPI, true, 0, 0, 0, 0, &res);

	show_suite_info();
	return 0;
}

static void board_bootdev_init(void)
{
	u32 bootdev = get_boot_device();
	struct mmc *mmc;

	switch (bootdev) {
	case MMC3_BOOT:
		bootdev = 2;
		break;
	case SD2_BOOT:
		bootdev = 1;
		break;
	default:
		env_set("bootdev", NULL);
		return;
	}

	env_dev = bootdev;

	mmc = find_mmc_device(bootdev);
	if (mmc && mmc->part_support && mmc->part_config != MMCPART_NOAVAILABLE)
		env_part = EXT_CSD_EXTRACT_BOOT_PART(mmc->part_config);

	env_set_ulong("bootdev", bootdev);
}

__weak int sub_board_late_init(void)
{
	return 0;
}

int board_late_init(void)
{
	int ret;

#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
	board_bootdev_init();
#endif

	ret = setup_mac_address();
	if (ret < 0)
		printf("%s: Can't set MAC address\n", __func__);

	ret = sub_board_late_init();
	return ret;
}

#ifdef CONFIG_FSL_FASTBOOT
#ifdef CONFIG_ANDROID_RECOVERY
int is_recovery_key_pressing(void)
{
	return 0; /*TODO*/
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/
