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

#include <common.h>
#include <command.h>

#ifdef CONFIG_TARGET_UCM_IMX8M_PLUS
static char ldo4_help_text[] =
	"value[8-33] - set 0x24 register value; voltage range: [0.80-3.30]\n"
	"ldo4 value[0] - disable ldo4\n";

/* Forward declaration */
u8 cl_eeprom_get_ldo4(void);
u8 cl_eeprom_set_ldo4(u8 ldo4);

static void do_pmic_ldo4(u8 ldo4) {
    const char *name = "pca9450@25";
    static struct udevice *currdev = NULL;
    int ret;
    if (currdev == NULL) {
        ret = pmic_get(name, &currdev);
        if (ret) {
            printf("Can't get PMIC: %s!\n", name);
            return;
        }
    }
    ret = pmic_reg_write(currdev, 0x24, ldo4);
    if (ret) {
        printf("Can't set PMIC: %s; register 0x%x\n", name, 0x24);
        return;
    }
}

int do_ldo4(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    u8 ldo4 = 0xDA;
    if (argc == 2) {
        ldo4 = (u8) simple_strtoul(argv[1], NULL, 10);
        if (( ldo4 >= 0x8 ) && ( ldo4 <= 0x21 )) {
            ldo4 -= 0x8; ldo4 |= 0x80;
	    } else if ( ldo4 == 0 )
            ldo4 = 0;
        else
            return CMD_RET_USAGE;
        ldo4 = cl_eeprom_set_ldo4(ldo4);
        do_pmic_ldo4(ldo4);
        return 0;
    }

    ldo4 = cl_eeprom_get_ldo4();
    if (( ldo4 >= 0x80 ) && ( ldo4 <= 0x9F )) {
        ldo4 &= ~0x80; ldo4 += 8;
        ldo4 = (( ldo4 > 33 ) ? 33 : ldo4);
        printf("pca9450@25 [ldo4] = %dv%d\n", (ldo4/10) , (ldo4%10));
    } else
        printf("pca9450@25 [ldo4] = 0x%x\n", ldo4);

    return 0;
}

U_BOOT_CMD(
	ldo4,	2,	1,	do_ldo4,
	"get/set ldo4 value",
	ldo4_help_text
);
#endif

static char ddr_help_text[] =
	"rdmr -- read mr[5-8] registers\n"
	"ddr read -- read eeprom values [ mrs, subid, size ]\n"
	"ddr clear -- clean up eeprom\n";

unsigned int lpddr4_get_mr(void);
void do_ddr_rdmr(void) {
   unsigned int data = lpddr4_get_mr();
   printf("mr[5-8]: [0x%x]\n", data);
}

u32 cl_eeprom_get_ddrinfo(void);
u8 cl_eeprom_get_subind(void);
void do_ddr_read(void) {
   u32 ddrinfo = cl_eeprom_get_ddrinfo();
   u8 subind = cl_eeprom_get_subind();
   printf("eeprom: [0x%x][0x%x]\n", ddrinfo, subind);
}

void cl_eeprom_clr_ddrinfo(void);
void do_ddr_clear(void) {
   cl_eeprom_clr_ddrinfo();
}

int do_ddr(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    if (argc != 2) {
        return CMD_RET_USAGE;
    }

	if (strcmp(argv[1], "rdmr") == 0) {
		do_ddr_rdmr();
	} else if (strcmp(argv[1], "read") == 0 ) {
		do_ddr_read();
	} else if (strcmp(argv[1], "clear") == 0 ) {
		do_ddr_clear();
    } else
        return CMD_RET_USAGE;

   return 0;
}

#ifdef CONFIG_DRAM_D2D4
#define SUPPORTED_CONF "D2,D4"
#else
#define SUPPORTED_CONF "D1,D8"
#endif

U_BOOT_CMD(
	ddr,	2,	1,	do_ddr,
	"rdmr/read/clear\nSupported configurations : [ "SUPPORTED_CONF" ]" ,
	ddr_help_text
);
