/*
 * (C) Copyright 2023 CompuLab, Ltd. <www.compulab.co.il>
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <command.h>
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
#include <asm/arch/imx-regs.h>
#include <asm/mach-imx/gpio.h>
#include <linux/delay.h>

#define REG_DDR_SDRAM_MD_CNTL_2 (DDR_CTL_BASE + 0x270)
#define REG_DDR_SDRAM_MD_CNTL (DDR_CTL_BASE + 0x120)
#define REG_DDR_SDRAM_MPR5 (DDR_CTL_BASE + 0x290)
#define REG_DDR_SDRAM_MPR4 (DDR_CTL_BASE + 0x28C)

#define DDRINFO_SRC_GPR_DDRINFO	1
#define DDRINFO_SRC_GPR_SUBIND	2

static u32 src_gpr_get(u32 index) {
	struct src_general_regs *src = (struct src_general_regs *)SRC_GLOBAL_RBASE;

	return readl(&src->gpr[index]);
}

static void src_gpr_set(u32 index, u32 value) {
	struct src_general_regs *src = (struct src_general_regs *)SRC_GLOBAL_RBASE;

	writel(value, &src->gpr[index]);
}

static inline void lpddr4_data_get(struct lpddr4_tcm_desc* lpddr4_tcm_desc) {
	memset(lpddr4_tcm_desc, 0, sizeof(struct lpddr4_tcm_desc));
}

static inline void lpddr4_data_set(struct lpddr4_tcm_desc* lpddr4_tcm_desc) {
	memset(lpddr4_tcm_desc, 0, sizeof(struct lpddr4_tcm_desc));
}

static struct lpddr4_tcm_desc spl_tcm_data;
#define SPL_TCM_DATA &spl_tcm_data

static u32 ddrc_mrr(u32 chip_select, u32 mode_reg_num, u32* mode_reg_val) {
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

static u32 lpddr4_mr_read(u32 mr_rank, u32 mr_addr) {
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

static void write_ddr_info_to_src_gpr(u32 id_in) {
	u32 id_out = 0xdeadbeef;

	src_gpr_set(DDRINFO_SRC_GPR_DDRINFO, id_in);
	mdelay(10);
	id_out = src_gpr_get(DDRINFO_SRC_GPR_DDRINFO);
	mdelay(10);
	if (id_in != id_out)
	{
		printf("DDRINFO: info mismatch expected 0x%x, actual 0x%x\n", id_in, id_out);
	}
}

static void write_subind_to_src_gpr(unsigned char subind_in) {
	unsigned char subind_out = 0;

	src_gpr_set(DDRINFO_SRC_GPR_SUBIND, subind_in);
	mdelay(10);
	subind_out = src_gpr_get(DDRINFO_SRC_GPR_SUBIND);
	if (subind_in != subind_out)
	{
		printf("DDRINFO: %s subind mismatch wrote 0x%x, read back 0x%x\n", __func__, subind_in, subind_out);
	}
}

static unsigned char get_valid_ddr_timing_index(u32 ddr_info, unsigned char subind) {
	for (unsigned char i = 1; i < ARRAY_SIZE(lpddr4_array); i++)
	{
		if ((lpddr4_array[i].id == ddr_info) && (lpddr4_array[i].subind == subind))
		{
			return i;
		}
	}
	printf("DDRINFO: cfg not found for ddr_info 0x%x and subind 0x%x\n", ddr_info, subind);
	return 0;
}

static unsigned char get_next_subind(u32 ddr_info, unsigned char subind) {//test this by trying to init with non working timing first
	unsigned char i = 1;
	while (i < ARRAY_SIZE(lpddr4_array)) {
		if ((lpddr4_array[i].id == ddr_info) && (lpddr4_array[i].subind == subind)) {
			break;
		}
		i++;
	}
	i++;
	while (i < ARRAY_SIZE(lpddr4_array)) {
		if (lpddr4_array[i].id == ddr_info) {
			return lpddr4_array[i].subind;
		}
		i++;
	}
	printf("DDRINFO: didn't find another cfg for ddr info 0x%x\n", ddr_info);
	return 0;
}

static unsigned char get_ddr_timing_index(u32 ddr_info) {
	for (unsigned char i = 1; i < ARRAY_SIZE(lpddr4_array); i++)
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

static bool initialize_ddr(const struct lpddr4_desc* ddr_desc) {
	if (ddr_init(ddr_desc->timing))
	{
		printf("DDRINFO: failed applying cfg: %s %dMB @ %d MHz\n", ddr_desc->name, ddr_desc->size, ddr_desc->timing->fsp_table[0]);
		return false;
	}
	else
	{
		printf("DDRINFO: applied cfg: %s %dMB @ %d MHz\n", ddr_desc->name, ddr_desc->size, ddr_desc->timing->fsp_table[0]);
		return true;
	}
}

static const struct lpddr4_desc* get_ddr_desc(unsigned char i) {
	return &lpddr4_array[i];
}

/*
testing:
after ddr came up check current SRC GPR state:
inspect the SRC GPR registers used for DDR info and subind

sabotage subind to check recovery after reset:
set the SRC GPR used for subind to an invalid value

sabotage ddrinfo to check recovery after reset:
set the SRC GPR used for ddrinfo to an invalid value
*/

void initialize_ddr_info(void) {
	u32 ddr_info = lpddr4_get_mr();
	const struct lpddr4_desc* ddr_desc = get_ddr_desc(get_ddr_timing_index(ddr_info));
	if (ddr_desc->id == 0xdeadbeef) {
		do_reset(NULL, 0, 0, NULL);
	}
	write_ddr_info_to_src_gpr(ddr_info);
	write_subind_to_src_gpr(ddr_desc->subind);
}

void spl_dram_init(void) {
	const struct lpddr4_desc* ddr_desc;
	u32 ddr_info = src_gpr_get(DDRINFO_SRC_GPR_DDRINFO);
	unsigned char subind = src_gpr_get(DDRINFO_SRC_GPR_SUBIND);
	int i = get_valid_ddr_timing_index(ddr_info, subind);
	if (i == 0) {
		printf("DDRINFO: set dummy cfg to enable reading mr[5-8]\n");
	}
	lpddr4_data_get(SPL_TCM_DATA);
	ddr_desc = get_ddr_desc(i);
	if (initialize_ddr(ddr_desc) == false) {
		write_subind_to_src_gpr(get_next_subind(ddr_info, subind));
		do_reset(NULL, 0, 0, NULL);
	}
	if (i == 0) {
		initialize_ddr_info();
		do_reset(NULL, 0, 0, NULL);
	}
	spl_tcm_data.size = ddr_desc->size;
	share_ddr_info_on_ocram();
}
