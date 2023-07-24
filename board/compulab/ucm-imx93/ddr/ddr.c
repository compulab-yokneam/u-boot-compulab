#include <common.h>
#include <command.h>
#include "../../common/eeprom.h"
#include <eeprom.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include "ddr.h"
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/gpio.h>
#include <asm-generic/gpio.h>
#include <asm/arch/ddr.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/gpio.h>
#include <linux/delay.h>

#define REG_DDR_SDRAM_MD_CNTL_2 (DDR_CTL_BASE + 0x270)
#define REG_DDR_SDRAM_MD_CNTL (DDR_CTL_BASE + 0x120)
#define REG_DDR_SDRAM_MPR5 (DDR_CTL_BASE + 0x290)
#define REG_DDR_SDRAM_MPR4 (DDR_CTL_BASE + 0x28C)

/* Forward declarations */
u32 cl_eeprom_get_ddrinfo(void);
u32 cl_eeprom_set_ddrinfo(u32 ddrinfo);
u32 cl_eeprom_get_subind(void);
u32 cl_eeprom_set_subind(u32 subind);
int cl_eeprom_write(uint offset, uchar* buf, int len);
int cl_eeprom_read(uint offset, uchar* buf, int len);

static inline void lpddr4_data_get(struct lpddr4_tcm_desc* lpddr4_tcm_desc) {
	cl_eeprom_read(0, (uchar*)lpddr4_tcm_desc, sizeof(struct lpddr4_tcm_desc));
}

static inline void lpddr4_data_set(struct lpddr4_tcm_desc* lpddr4_tcm_desc) {
	cl_eeprom_write(0, (uchar*)lpddr4_tcm_desc, sizeof(struct lpddr4_tcm_desc));
}

static struct lpddr4_tcm_desc spl_tcm_data;
#define SPL_TCM_DATA &spl_tcm_data

u32 ddrc_mrr(u32 chip_select, u32 mode_reg_num, u32* mode_reg_val) {
	u32 temp;

	writel(0x80000000, REG_DDR_SDRAM_MD_CNTL_2);
	temp = 0x80000000 | (chip_select << 28) | (mode_reg_num << 0);
	writel(temp, REG_DDR_SDRAM_MD_CNTL);
	while ((readl(REG_DDR_SDRAM_MD_CNTL) & 0x80000000) == 0x80000000)
		;
	while (!(readl(REG_DDR_SDRAM_MPR5)))
		;
	*mode_reg_val = (readl(REG_DDR_SDRAM_MPR4) & 0xFF0000) >> 16;
	writel(0x0, REG_DDR_SDRAM_MPR5);
	while ((readl(REG_DDR_SDRAM_MPR5)))
		;
	writel(0x0, REG_DDR_SDRAM_MPR4);
	writel(0x0, REG_DDR_SDRAM_MD_CNTL_2);

	return 0;
}

u32 lpddr4_mr_read(u32 mr_rank, u32 mr_addr) {
	u32 chip_select, regval;

	if (mr_rank == 1)
	{
		chip_select = 0; /* CS0 */
	}
	else if (mr_rank == 2)
	{
		chip_select = 1; /* CS1 */
	}
	else
	{
		chip_select = 4; /* CS0 & CS1 */
	}

	ddrc_mrr(chip_select, mr_addr, &regval);

	return regval;
}

u32 lpddr4_get_mr(void) {
	int i = 0, attempts = 5;
	u32 ddr_info = 0;
	u32 regs[] = { 5, 6, 7, 8 };

	do
	{
		for (i = 0; i < ARRAY_SIZE(regs); i++)
		{
			u32 data = 0;
			data = lpddr4_mr_read(0xF, regs[i]);
			ddr_info <<= 8;
			ddr_info += (data & 0xFF);
		}
		if ((ddr_info != 0xFFFFFFFF) && (ddr_info != 0))
			break; // The attempt was successfull
	} while (--attempts);
	assert_noisy((ddr_info != 0xFFFFFFFF) && (ddr_info != 0));
	return ddr_info;
}

static void write_ddr_info_to_eeprom(u32 id_in) {
	u32 id_out = 0xdeadbeef;
	cl_eeprom_set_ddrinfo(id_in);
	mdelay(10);
	id_out = cl_eeprom_get_ddrinfo();
	mdelay(10);
	if (id_in != id_out)
	{
		printf("DDRINFO: info mismatch expected 0x%x, actual 0x%x\n", id_in, id_out);
	}
}

static void write_subind_to_eeprom(u8 subind_in) {
	u8 subind_out = 0xff;
	cl_eeprom_set_subind(subind_in);
	mdelay(20);
	subind_out = cl_eeprom_get_subind();
	if (subind_in != subind_out)
	{
		printf("DDRINFO: %s subind mismatch wrote 0x%x, read back 0x%x\n", __func__, subind_in, subind_out);
		printf("DDRINFO: make sure that eeprom is accessible\n");
		printf("DDRINFO: i2c dev 0; i2c md 0x51 0x40 0x50\n");
	}
}

static u8 get_ddr_timing_index_with_next_subind(u32 ddr_info, u8 invalid_subind) {
	u8 i = 2;
	while (i < ARRAY_SIZE(lpddr4_array)) {
		if ((lpddr4_array[i].id == ddr_info) && (lpddr4_array[i].subind == invalid_subind)) {
			i++;
			break;
		}
		i++;
	}
	while (i < ARRAY_SIZE(lpddr4_array)) {
		if (lpddr4_array[i].id == ddr_info) {
			return i;
		}
		i++;
	}
	printf("DDRINFO: didn't find cfg for ddr info 0x%x\n", ddr_info);
	return 0;
}

static u8 get_valid_ddr_timing_index(u32 ddr_info, u8 subind) {
	for (u8 i = 1; i < ARRAY_SIZE(lpddr4_array); i++)
	{
		if ((lpddr4_array[i].id == ddr_info) && (lpddr4_array[i].subind == subind))
		{
			return i;
		}
	}
	printf("DDRINFO: cfg not found for ddr_info 0x%x and subind 0x%x\n", ddr_info, subind);
	return 0;
}

static u8 get_ddr_timing_index(u32 ddr_info) {
	for (u8 i = 1; i < ARRAY_SIZE(lpddr4_array); i++)
	{
		if (lpddr4_array[i].id == ddr_info)
		{
			return i;
		}
	}
	printf("DDRINFO: no cfg found for ddr_info 0x%x\n", ddr_info);
	return 0;
}

static inline void share_ddr_info_on_ocram(void) {
#ifdef SHARED_DDR_INFO
	struct lpddr4_tcm_desc* lpddr4_tcm_desc = (void*)SHARED_DDR_INFO;
	memcpy(lpddr4_tcm_desc, SPL_TCM_DATA, sizeof(struct lpddr4_tcm_desc));
#endif
}

static int initialize_ddr(const struct lpddr4_desc* ddr_desc) {
	if (ddr_init(ddr_desc->timing))
	{
		printf("DDRINFO: failed applying cfg: %s %dMB @ %d MHz\n", ddr_desc->name, ddr_desc->size, ddr_desc->timing->fsp_table[0]);
		return 1;
	}
	else
	{
		printf("DDRINFO: applied cfg: %s %dMB @ %d MHz\n", ddr_desc->name, ddr_desc->size, ddr_desc->timing->fsp_table[0]);
		return 0;
	}
}

static void set_state(u32 state) {
	spl_tcm_data.ddr_init_status = state;
	lpddr4_data_set(SPL_TCM_DATA);
}

#define DDR_CFG_VALID 							0x11111111
#define NAIVE_DDR_INIT 							0x22222222
#define DDR_INIT_FAILED_BUT_ANOTHER_CFG_EXISTS 	0x33333333
#define CFG_UNKNOWN 							0x44444444

void die(void) {
	set_state(CFG_UNKNOWN);
	while (1)
	{
	};
}

static void go_to_state(u32 state) {
	set_state(state);
	do_reset(NULL, 0, 0, NULL);
}

static int get_occurences_of_ddr_info(u32 ddr_info) {
	int count = 0;
	for (u8 i = 1; i < ARRAY_SIZE(lpddr4_array); i++)
	{
		if (lpddr4_array[i].id == ddr_info)
		{
			count++;
		}
	}
	return count;
}

static bool there_is_another_cfg_for_this_ddr_info(u32 ddr_info) {
	return (1 < get_occurences_of_ddr_info(ddr_info));
}

static const struct lpddr4_desc* get_ddr_desc(u8 i) {
	if (i == 0) {
		die();
	}
	return &lpddr4_array[i];
}

/*
testing:
after ddr came up:
check current eeprom state:						i2c dev 0; i2c md 0x51 0x0 0x50
sabotage subind to check recovery after reset: 	i2c mw 0x51 0x44 0xff
sabotage ddrinfo to check recovery after reset: i2c mw 0x51 0x40 0xff; i2c mw 0x51 0x41 0xff; i2c mw 0x51 0x42 0xff; i2c mw 0x51 0x43 0xff
sabotage state to check recovery after reset: 	i2c mw 0x51 0xe 0xff
*/

void spl_dram_init(void) {
	const struct lpddr4_desc* ddr_desc;
	u32 ddr_info = cl_eeprom_get_ddrinfo();
	u8 subind = cl_eeprom_get_subind();
	lpddr4_data_get(SPL_TCM_DATA);
	switch (spl_tcm_data.ddr_init_status) {
	case DDR_CFG_VALID:
	{
		int i = get_valid_ddr_timing_index(ddr_info, subind);
		if (i == 0) {
			go_to_state(CFG_UNKNOWN);
		}
		ddr_desc = get_ddr_desc(i);
		if (initialize_ddr(ddr_desc)) {
			go_to_state(CFG_UNKNOWN);
		}
		break;
	}
	case NAIVE_DDR_INIT:
	{
		printf("DDRINFO: NAIVE_DDR_INIT\n");
		ddr_desc = get_ddr_desc(get_ddr_timing_index(ddr_info));
		write_subind_to_eeprom(ddr_desc->subind);
		if (initialize_ddr(ddr_desc)) {
			if (there_is_another_cfg_for_this_ddr_info(ddr_info)) {
				go_to_state(DDR_INIT_FAILED_BUT_ANOTHER_CFG_EXISTS);
			}
			else {
				die();
			}
		}
		break;
	}
	case DDR_INIT_FAILED_BUT_ANOTHER_CFG_EXISTS:
	{
		printf("DDRINFO: DDR_INIT_FAILED_BUT_ANOTHER_CFG_EXISTS\n");
		ddr_desc = get_ddr_desc(get_ddr_timing_index_with_next_subind(ddr_info, subind));
		write_subind_to_eeprom(ddr_desc->subind);
		if (initialize_ddr(ddr_desc)) {
			go_to_state(DDR_INIT_FAILED_BUT_ANOTHER_CFG_EXISTS);
		}
		else {
			die();
		}
		break;
	}
	default:
	{
		u32 ddr_info = 0xdeadbeef;
		printf("DDRINFO: CFG_UNKNOWN\n");
		printf("DDRINFO: set dummy cfg to enable reading mr[5-8]\n");
		if (initialize_ddr(&lpddr4_array[0]))
		{
			die();
		}
		ddr_info = lpddr4_get_mr();
		if (get_ddr_timing_index(ddr_info) == 0) {
			die();
		}
		write_ddr_info_to_eeprom(ddr_info);
		set_state(NAIVE_DDR_INIT);
		do_reset(NULL, 0, 0, NULL);
		break;
	}
	}
	set_state(DDR_CFG_VALID);
	spl_tcm_data.size = ddr_desc->size;
	share_ddr_info_on_ocram();
}