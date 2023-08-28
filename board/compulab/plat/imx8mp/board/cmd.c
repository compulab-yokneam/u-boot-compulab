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

#define SNVS_LP 0x30370038

static char pbb_help_text[] =
	"help            -- show this help\n"
	"pbb get         -- get SVNS_LP resgister value\n"
	"pbb set <value> -- set SVNS_LP resgister value\n"
	"pbb on_time <value> -- set power button on_time value\n"
	"\t0: 500msec off->on transition time (default)\n"
	"\t1: 50msec off->on transition time\n"
	"\t2: 100msec off->on transition time\n"
	"\t3: 0msec off->on transition time\n"
	"pbb btn_press_time <value> -- set power button btn_press_time value\n"
	"\t0: 5sec  on->off long press time (default)\n"
	"\t1: 10sec on->off long press time\n"
	"\t2: 15sec on->off long press time\n"
	"\t3: on->off long press disabled\n";

static void do_pbb_get(void) {
    unsigned int value;
    char *cvalue = NULL;
    value = readl(SNVS_LP);
    printf("%s = 0x%x\n","SNVS_LP Control Register LPCR",value);
    cvalue = env_get("on_time");
    if (cvalue) {
        printf("env: on_time=%s\n",cvalue);
    }
    cvalue = env_get("btn_press_time");
    if (cvalue) {
        printf("env: btn_press_time=%s\n",cvalue);
    }
    return;
}

static void do_pbb_set(const char *cvalue) {
    unsigned int value;
    value = simple_strtoul(cvalue, NULL, 16);
    writel(value, SNVS_LP);
    return;
}

static void do_pbb_function(char *name, const char *cvalue, unsigned int offset, int flag) {
    unsigned int value;
    unsigned int _value;
    if (cvalue == NULL) {
        value = readl(SNVS_LP);
        value &= (3 << offset);
        value = (value >> offset);
        if (flag)
            printf("%s[%d] = 0x%x\n","SNVS_LP Control Register LPCR",offset,value);
    } else {
        value = simple_strtoul(cvalue, NULL, 10);
        _value = readl(SNVS_LP);
        _value &= ~(3 << offset);
        _value |= (value << offset);
        writel(_value, SNVS_LP);
        if (flag)
            env_set(name, cvalue);
    }
}

int do_pbb(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    if (strcmp(argv[1], "on_time") == 0) {
        do_pbb_function(argv[1], argv[2], 20, 1);
    } else if (strcmp(argv[1], "btn_press_time") == 0 ) {
        do_pbb_function(argv[1], argv[2], 16, 1);
    } else if (strcmp(argv[1], "get") == 0 ) {
        do_pbb_get();
    } else if ((strcmp(argv[1], "set") == 0) && (argv[2])) {
        do_pbb_set(argv[2]);
    } else
        return CMD_RET_USAGE;

   return 0;
}

U_BOOT_CMD(
    pbb, 3,	1, do_pbb,
    "get/set power button settings",
    pbb_help_text
);

void do_pbb_restore(void) {
    char *cvalue = NULL;
    cvalue = env_get("on_time");
    do_pbb_function("on_time", cvalue, 20, 0);
    cvalue = env_get("btn_press_time");
    do_pbb_function("btn_press_time", cvalue, 16, 0);
}
