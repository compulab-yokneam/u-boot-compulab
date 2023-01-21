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
#include "ddr/ddr.h"
#include "common/eeprom.h"
#include "common/rtc.h"

DECLARE_GLOBAL_DATA_PTR;

static int env_dev = -1;
static int env_part= -1;

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

int dram_init(void)
{
	phys_size_t sdram_size;
	int ret;
	ret = board_phys_sdram_size(&sdram_size);
	if (ret)
		return ret;

	/* rom_pointer[1] contains the size of TEE occupies */
	gd->ram_size = sdram_size - rom_pointer[1];

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

int ft_board_setup(void *blob, bd_t *bd)
{
	fdt_set_env_addr(blob);
	fdt_set_sn(blob);
	return 0;
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

int board_phy_config(struct phy_device *phydev)
{
	/* enable rgmii rxc skew and phy mode select to RGMII copper */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x1f);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x8);

	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x00);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x82ee);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x05);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x100);

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

int board_init(void)
{

	disable_rtc_bus_on_battery();

#ifdef CONFIG_FEC_MXC
	setup_fec();
#endif
	if (uboot_board_private_init()) {
		printf("uboot_board_private_init() failed\n");
		hang();
	}
	if (IS_ENABLED(CONFIG_LED))
		led_default_state();

	show_suite_info();
	return 0;
}

int board_mmc_get_env_dev(int devno)
{
	const ulong user_env_devno = env_get_hex("env_dev", ULONG_MAX);
	if (user_env_devno != ULONG_MAX) {
		printf("User Environment dev# is (%lu)\n", user_env_devno);
		return (int)user_env_devno;
	}
	return devno;
}

static int _mmc_get_env_part(struct mmc *mmc)
{
	const ulong user_env_part = env_get_hex("env_part", ULONG_MAX);
	if (user_env_part != ULONG_MAX) {
		printf("User Environment part# is (%lu)\n", user_env_part);
		return (int)user_env_part;
	}

	return EXT_CSD_EXTRACT_BOOT_PART(mmc->part_config);
}

uint mmc_get_env_part(struct mmc *mmc)
{
	if (mmc->part_support && mmc->part_config != MMCPART_NOAVAILABLE) {
		uint partno = _mmc_get_env_part(mmc);
		env_part = partno;
		return partno;
	}
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
