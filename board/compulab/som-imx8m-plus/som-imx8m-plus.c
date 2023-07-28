/*
 * Copyright 2020 CompuLab
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <asm/io.h>
#include <miiphy.h>
#include <netdev.h>
#include <linux/delay.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8mp_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/clock.h>
#include <spl.h>
#include <asm/mach-imx/dma.h>
#include <power/pmic.h>
#include <power/bd71837.h>
#include <usb.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/video.h>

DECLARE_GLOBAL_DATA_PTR;

#define EXP_3V3_PAD IMX_GPIO_NR(2, 11)
static iomux_v3_cfg_t const exp_3v3_pads[] = {
	MX8MP_PAD_SD1_STROBE__GPIO2_IO11 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_3v3_exp(void)
{
	imx_iomux_v3_setup_multiple_pads(exp_3v3_pads,
					 ARRAY_SIZE(exp_3v3_pads));

	gpio_request(EXP_3V3_PAD, "3v3_exp");
	gpio_direction_output(EXP_3V3_PAD, 0);
	mdelay(50);
	gpio_direction_output(EXP_3V3_PAD, 1);
	mdelay(50);
}

#ifdef CONFIG_FEC_MXC
static int setup_fec(void)
{
	struct iomuxc_gpr_base_regs *gpr =
		(struct iomuxc_gpr_base_regs *)IOMUXC_GPR_BASE_ADDR;

	/* Enable RGMII TX clk output */
	setbits_le32(&gpr->gpr[1], BIT(22) | BIT(13));

	return set_clk_enet(ENET_125MHZ);
}
#endif

#if defined(CONFIG_FEC_MXC) || defined(CONFIG_DWC_ETH_QOS)
#include "../common/eeprom.h"
void imx_get_mac_from_fuse(int dev_id, unsigned char *mac)
{
	cl_eeprom_read_n_mac_addr(mac, dev_id, CONFIG_SYS_I2C_EEPROM_BUS);
	debug("%s: MAC%d: %02x.%02x.%02x.%02x.%02x.%02x\n",
	      __func__, dev_id, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return;
}
#endif

#define WIFI_PD IMX_GPIO_NR(1, 0)
static iomux_v3_cfg_t const wifi_pd_pads[] = {
	MX8MP_PAD_GPIO1_IO00__GPIO1_IO00 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_wifi_pd(void)
{
	imx_iomux_v3_setup_multiple_pads(wifi_pd_pads,
					 ARRAY_SIZE(wifi_pd_pads));

	gpio_request(WIFI_PD, "wifi_pd");
	gpio_direction_output(WIFI_PD, 0);
}

void board_vendor_init(void) {
	setup_3v3_exp();

	setup_wifi_pd();
#ifdef CONFIG_FEC_MXC
	setup_fec();
#endif
	return;
}

#define ETHPHY0 "/soc@0/bus@30800000/ethernet@30be0000/mdio/ethernet-phy@0"
#define ETHPHY1 "/soc@0/bus@30800000/ethernet@30bf0000/mdio/ethernet-phy@1"

struct phy_node {
    char *node;
    unsigned int addr;
};

static struct phy_node phy_nodes[] = {
    { .node = ETHPHY0, .addr = -1 },
    { .node = ETHPHY1, .addr = -1 },
};

void board_save_phyaddr(int phy_addr) {
    debug("[*] %s:%d addr [ %d ]\n",__func__,__LINE__,phy_addr);
    if ((phy_addr == 4) || (phy_addr == 5)) {
        /* rev1.1 */
        phy_nodes[0].addr = 5;
        phy_nodes[1].addr = 4;
    } else {
        phy_nodes[0].addr = 1;
        phy_nodes[1].addr = 0;
    }
    return;
}

static int fdt_update_phy_address(void *blob, const char *node, unsigned int phy_address) {
    debug("[*] %s:%d [ %s = %d ]\n",__func__,__LINE__,node,phy_address);
    int offs = fdt_path_offset(blob, node);
    if (offs < 0) {
        printf("Node %s not found.\n", node);
        return -EINVAL;
    }
    fdt_delprop(blob, offs, "reg");
    phy_address = cpu_to_fdt32(phy_address);
    return fdt_setprop(blob, offs , "reg", &phy_address, sizeof(u32));
}

int fdt_board_vendor_setup(void *blob) {
    int rc, i;
    for ( i = 0; i < ARRAY_SIZE(phy_nodes); i++ ) {
        rc = fdt_update_phy_address(blob, phy_nodes[i].node, phy_nodes[i].addr);
        if (rc) {
            printf("Node %s update failed rc %d\n", phy_nodes[i].node, rc);
            return rc;
        }
    }
    return 0;
}
