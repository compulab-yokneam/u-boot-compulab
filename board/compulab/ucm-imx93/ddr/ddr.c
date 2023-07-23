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

static inline void lpddr4_data_get(struct lpddr4_tcm_desc* lpddr4_tcm_desc)
{
	cl_eeprom_read(0, (uchar*)lpddr4_tcm_desc, sizeof(struct lpddr4_tcm_desc));
}

static inline void lpddr4_data_set(struct lpddr4_tcm_desc* lpddr4_tcm_desc)
{
	cl_eeprom_write(0, (uchar*)lpddr4_tcm_desc, sizeof(struct lpddr4_tcm_desc));
}

static struct lpddr4_tcm_desc spl_tcm_data;
#define SPL_TCM_DATA &spl_tcm_data

u32 ddrc_mrr(u32 chip_select, u32 mode_reg_num, u32* mode_reg_val)
{
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

u32 lpddr4_mr_read(u32 mr_rank, u32 mr_addr)
{
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

unsigned int lpddr4_get_mr(void)
{
	int i = 0, attempts = 5;
	unsigned int ddr_info = 0;
	unsigned int regs[] = { 5, 6, 7, 8 };

	do
	{
		for (i = 0; i < ARRAY_SIZE(regs); i++)
		{
			unsigned int data = 0;
			data = lpddr4_mr_read(0xF, regs[i]);
			ddr_info <<= 8;
			ddr_info += (data & 0xFF);
		}
		if ((ddr_info != 0xFFFFFFFF) && (ddr_info != 0))
			break; // The attempt was successfull
	} while (--attempts);
	printf("DDRINFO: manufacturer id 0x%x detected\n", ddr_info);
	return ddr_info;
}

static int write_ddr_id_to_eeprom(unsigned int ddr_info_mrr)
{
	unsigned int ddr_info = 0xdeadbeef;
	cl_eeprom_set_ddrinfo(ddr_info_mrr);
	mdelay(10);
	ddr_info = cl_eeprom_get_ddrinfo();
	mdelay(10);
	if (ddr_info_mrr != ddr_info)
	{
		printf("DDRINFO: %s failed. written to mr5-8 %x, read back %x\n", __func__, ddr_info_mrr, ddr_info);
		return 1;
	}
	return 0;
}

static int get_ddr_timing_index(unsigned int ddr_info)
{
	for (int i = 0; i < ARRAY_SIZE(lpddr4_array); i++)
	{
		printf("DDRINFO: checking cfg: %s %dMB @ %d MHz\n", lpddr4_array[i].name, lpddr4_array[i].size, lpddr4_array[i].timing->fsp_table[0]);
		if (lpddr4_array[i].id == ddr_info)
		{
			printf("DDRINFO: manufacturer id 0x%x matches known cfg: %s %dMB @ %d MHz\n", ddr_info, lpddr4_array[i].name, lpddr4_array[i].size, lpddr4_array[i].timing->fsp_table[0]);
			return i;
		}
	}
	return 0;
}

static inline void share_ddr_info_on_ocram(void)
{
#ifdef SHARED_DDR_INFO
	struct lpddr4_tcm_desc* lpddr4_tcm_desc = (void*)SHARED_DDR_INFO;
	memcpy(lpddr4_tcm_desc, SPL_TCM_DATA, sizeof(struct lpddr4_tcm_desc));
#endif
}

static int initialize_ddr(const struct lpddr4_desc ddr_desc)
{
	if (ddr_init(ddr_desc.timing))
	{
		printf("DDRINFO: applying cfg fail: %s %dMB @ %d MHz\n", ddr_desc.name, ddr_desc.size, ddr_desc.timing->fsp_table[0]);
		return 1;
	}
	else
	{
		printf("DDRINFO: applying cfg success: %s %dMB @ %d MHz\n", ddr_desc.name, ddr_desc.size, ddr_desc.timing->fsp_table[0]);
		return 0;
	}
}

void die(void)
{
	while (1)
	{
	};
}

#define TIMING_WAS_FOUND 0x55555555
#define TIMING_IS_UNKNOWN 0x11111111

static void find_another_timing()
{
	spl_tcm_data.ddr_init_status = TIMING_IS_UNKNOWN;
	lpddr4_data_set(SPL_TCM_DATA);
	do_reset(NULL, 0, 0, NULL);
}

void spl_dram_init(void)
{
	lpddr4_data_get(SPL_TCM_DATA);
	if (spl_tcm_data.ddr_init_status == TIMING_WAS_FOUND)
	{
		if (lpddr4_array[spl_tcm_data.index].id == cl_eeprom_get_ddrinfo())
		{
			if (initialize_ddr(lpddr4_array[spl_tcm_data.index]))
			{
				find_another_timing();
			}
			spl_tcm_data.size = lpddr4_array[spl_tcm_data.index].size;
			share_ddr_info_on_ocram();
		}
		else
		{
			printf("DDRINFO: ddr identification mismatch: %x\n", lpddr4_array[spl_tcm_data.index].id);
			find_another_timing();
		}
	}
	else // TIMING_IS_UNKNOWN:
	{
		unsigned int ddr_info = 0xdeadbeef;
		if (initialize_ddr(lpddr4_array[0])) // enable reading from mr
		{
			printf("DDRINFO: ddr init fail\n");
			die();
		}
		ddr_info = lpddr4_get_mr();
		assert((ddr_info == 0xFFFFFFFF) || (ddr_info == 0));
		spl_tcm_data.index = get_ddr_timing_index(ddr_info);
		assert((spl_tcm_data.index == 0) || (spl_tcm_data.index >= ARRAY_SIZE(lpddr4_array)));
		if (write_ddr_id_to_eeprom(ddr_info))
		{
			die();
		}
		spl_tcm_data.ddr_init_status = TIMING_WAS_FOUND;
		lpddr4_data_set(SPL_TCM_DATA);
		do_reset(NULL, 0, 0, NULL);
	}
}