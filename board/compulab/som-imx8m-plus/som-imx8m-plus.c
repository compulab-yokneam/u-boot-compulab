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
#define FEC_RST_PAD IMX_GPIO_NR(4, 24)
static iomux_v3_cfg_t const fec1_rst_pads[] = {
	MX8MP_PAD_SAI2_TXFS__GPIO4_IO24 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_iomux_fec(void)
{
	imx_iomux_v3_setup_multiple_pads(fec1_rst_pads,
					 ARRAY_SIZE(fec1_rst_pads));

	gpio_request(FEC_RST_PAD, "fec1_rst");
	gpio_direction_output(FEC_RST_PAD, 0);
	mdelay(15);
	gpio_direction_output(FEC_RST_PAD, 1);
	mdelay(100);
}

static int setup_fec(void)
{
	struct iomuxc_gpr_base_regs *gpr =
		(struct iomuxc_gpr_base_regs *)IOMUXC_GPR_BASE_ADDR;

	setup_iomux_fec();

	/* Enable RGMII TX clk output */
	setbits_le32(&gpr->gpr[1], BIT(22));

	return 0;
}
#endif

void board_vendor_init(void) {
	setup_3v3_exp();

#ifdef CONFIG_FEC_MXC
	setup_fec();
#endif
	return;
}
