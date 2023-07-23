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

#define DEFAULT (('D' << 24) + ('E' << 16) + ('F' << 8) + 'A')
#define VALID 0xCAFECAFE
#define DDR_INIT_IN 0xCACACACA
#define DDR_INIT_OUT 0x0C0C0C0C

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

static void spl_tcm_init(struct lpddr4_tcm_desc* lpddr4_tcm_desc)
{
	if (lpddr4_tcm_desc->sign == DEFAULT)
		return;

	memset((char*)lpddr4_tcm_desc, 0x0, sizeof(struct lpddr4_tcm_desc));
	lpddr4_tcm_desc->sign = DEFAULT;
}

static void spl_tcm_fini(struct lpddr4_tcm_desc* lpddr4_tcm_desc)
{
	if (lpddr4_tcm_desc->sign == VALID)
		return;

	memset((char*)lpddr4_tcm_desc, 0x0, sizeof(struct lpddr4_tcm_desc));
	lpddr4_tcm_desc->sign = VALID;
}

static void spl_tcm_clr(struct lpddr4_tcm_desc* lpddr4_tcm_desc)
{
	memset((char*)lpddr4_tcm_desc, 0xFF, sizeof(struct lpddr4_tcm_desc));
}

static struct lpddr4_tcm_desc spl_tcm_data;
#define SPL_TCM_DATA &spl_tcm_data
#define SPL_TCM_CLR spl_tcm_clr(lpddr4_tcm_desc)
#define SPL_TCM_INIT spl_tcm_init(lpddr4_tcm_desc)
#define SPL_TCM_FINI spl_tcm_fini(lpddr4_tcm_desc)

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

/*
static int _spl_dram_init(void) {
	unsigned int ddr_info = 0xdeadbeef;
	u8 subind = 0xfF;
	unsigned int ddr_info_mrr = 0xdeadbeef;
	unsigned int ddr_found = 0;
	int i = 0;

	struct lpddr4_tcm_desc *lpddr4_tcm_desc = SPL_TCM_DATA;

	// get ddr type from the eeprom if not in tcm scan mode
	if (lpddr4_tcm_desc->sign == VALID) {
		ddr_info = cl_eeprom_get_ddrinfo();
		subind = cl_eeprom_get_subind();

		printf("DDRINFO: EEPROM VALID DATA [ %x ] = %x %x \n",
				lpddr4_tcm_desc->sign, ddr_info, subind);

		for ( i = 0; i < ARRAY_SIZE(lpddr4_array); i++ ) {
			if (lpddr4_array[i].id == ddr_info &&
			lpddr4_array[i].subind == subind) {
				ddr_found = 1;
				break;
			}
		}
	}

	// Walk trough available ddr ids and apply one by one. Save the l at the tcm memory that  persists after the reset.
	if (ddr_found == 0) {
		SPL_TCM_INIT;
		// check the last training status
		if (lpddr4_tcm_desc->ddr_init_status == DDR_INIT_IN) {
			printf("%s:%d Bad attempt reading ddr id at l %d. skipping\n",__FILE__,__LINE__,(lpddr4_tcm_desc->l+1));
			lpddr4_tcm_desc->l += 1;
		}
		if (lpddr4_tcm_desc->l < ARRAY_SIZE(lpddr4_array)) {
			printf("%s:%d DDRINFO: Cfg attempt: [ %d/%lu ]\n", __FILE__,__LINE__, lpddr4_tcm_desc->l+1, ARRAY_SIZE(lpddr4_array));
			i = lpddr4_tcm_desc->l;
			lpddr4_tcm_desc->l += 1;
		} else {
			SPL_TCM_CLR;
			printf("DDRINFO: tried all %lu available ddr configurations. Configuration not supported.\n", ARRAY_SIZE(lpddr4_array));
			return -1;
		}

		ddr_info = lpddr4_array[i].id;
	}

	printf("DDRINFO: %s: %s %dMB @ %d MHz\n", (ddr_found ? "current" : "to train" ), lpddr4_array[i].name,
			lpddr4_array[i].size, lpddr4_array[i].timing->fsp_table[0]);

	// prepare for training
	if (ddr_found == 0) {
		// This is a discovery case, save in ddr_init_status 'cause it can get stuck
		lpddr4_tcm_desc->ddr_init_status = DDR_INIT_IN;
		// Save the data before training
		lpddr4_data_set(SPL_TCM_DATA);
	}

	if (ddr_init(lpddr4_array[i].timing)) { // try to train
		SPL_TCM_INIT;
		return 1;
	}

	if (ddr_found == 0) {
		// This is a discovery case, save out ddr_init_status
		lpddr4_tcm_desc->ddr_init_status = DDR_INIT_OUT;
		// Save the data after training
		lpddr4_data_set(SPL_TCM_DATA);
	}

	ddr_info_mrr = lpddr4_get_mr();
	// check id
	if (ddr_info_mrr == 0xFFFFFFFF ) {
		printf("DDRINFO(M): mr5-8 [ 0x%x ] is invalid; reset\n", ddr_info_mrr);
		SPL_TCM_INIT;
		return 1;
	}
	if (ddr_info_mrr != ddr_info) {
		printf("%s:%d DDRINFO: id (mr5-8) mismatch: wanted 0x%x actual 0x%x\n", __FILE__,__LINE__, ddr_info, ddr_info_mrr);
		SPL_TCM_INIT;
		return 1;
	}
	printf("DDRINFO(%s found): wanted id (mr5-8) [ 0x%x ]\n", (ddr_found ? "" : "not" ), ddr_info);

	lpddr4_tcm_desc->size = lpddr4_array[i].size;
	if (ddr_found == 0) {
		// Update eeprom
		cl_eeprom_set_ddrinfo(ddr_info_mrr);
		mdelay(10);
		ddr_info = cl_eeprom_get_ddrinfo();
		mdelay(10);
		cl_eeprom_set_subind(lpddr4_array[i].subind);
		// make sure that the ddr_info has reached eeprom
		printf("DDRINFO(E): mr5-8 [ 0x%x ], read back\n", ddr_info);
		if (ddr_info_mrr != ddr_info || cl_eeprom_get_subind() != lpddr4_array[i].subind) {
			printf("DDRINFO(EEPROM): make sure that the eeprom is accessible\n");
			printf("DDRINFO(EEPROM): i2c dev 1; i2c md 0x51 0x40 0x50\n");
		}
		// Set the data valid
		SPL_TCM_FINI;
		// Return with 1 in order to make the caller save ddr discovery status
		return 1;
	}
	return 0;
}
*/

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
		if (lpddr4_array[i].id == ddr_info) // && lpddr4_array[i].subind == subind)
		{
			printf("DDRINFO: manufacturer id 0x%x matches known cfg: %s %dMB @ %d MHz\n", ddr_info, lpddr4_array[i].name, lpddr4_array[i].size, lpddr4_array[i].timing->fsp_table[0]);
			return i;
		}
	}
	return 0;
}

/*
size_t lppdr4_get_ramsize() {
	struct lpddr4_tcm_desc *desc = (void *) SHARED_DDR_INFO;
	if (desc)
		return desc->size;
	return 0;
}
*/
static inline void spl_dram_share_info(void)
{
#ifdef SHARED_DDR_INFO
	struct lpddr4_tcm_desc* lpddr4_tcm_desc = (void*)SHARED_DDR_INFO;
	memcpy(lpddr4_tcm_desc, SPL_TCM_DATA, sizeof(struct lpddr4_tcm_desc));
#endif
}

static int ddr_id_of_descriptor_and_of_board_info_match(unsigned int id, u8 subind)
{
	if (id == cl_eeprom_get_ddrinfo() && subind == cl_eeprom_get_subind())
	{
		return 1;
	}
	return 0;
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
	int status;
	unsigned int ddr_info = 0xdeadbeef;

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
			spl_dram_share_info();
		}
		else
		{
			printf("DDRINFO: ddr identification mismatch: %x\n", lpddr4_array[spl_tcm_data.index].id);
			find_another_timing();
		}
	}
	else // TIMING_IS_UNKNOWN:
	{
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