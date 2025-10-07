#include <common.h>
#include <command.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch-imx8m/imx8m_ddr.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/gpio.h>
#include <asm-generic/gpio.h>
#include <asm/arch/ddr.h>
#include <asm/arch/imx8mq_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/gpio.h>
#include <linux/delay.h>
#include <mmc.h>
#include "ddr.h"
#include "lpddr_timing_block.h"
#include "ddr_ddrphy_trained_csr.h"

/* Forward declarations */
u32 cl_eeprom_get_ddrinfo(void);
u32 cl_eeprom_set_ddrinfo(u32 ddrinfo);
u8 cl_eeprom_get_subind(void);
u8 cl_eeprom_set_subind(u8 subind);

/* Placeholder to be filled with a real timing table read from MMC */
struct lpddr4_timing_block timing_block __attribute__((section (".data")));

int spl_mmc_find_device(struct mmc **mmcp, u32 boot_device);
int mmc_init(struct mmc *mmc);
struct blk_desc *mmc_get_blk_desc(struct mmc *mmc);


#ifdef CONFIG_SPL_REPORT_FAKE_MEMSIZE
u32 cl_eeprom_get_osize(void);
#endif
static unsigned int lpddr4_mr_read_and_refine(unsigned int mr_rank, unsigned int mr_addr)
{
	unsigned int tmp;
	reg32_write(DRC_PERF_MON_MRR0_DAT(0), 0x1);
	do {
		tmp = reg32_read(DDRC_MRSTAT(0));
	} while (tmp & 0x1);

	reg32_write(DDRC_MRCTRL0(0), (mr_rank << 4) | 0x1);
	reg32_write(DDRC_MRCTRL1(0), (mr_addr << 8));
	reg32setbit(DDRC_MRCTRL0(0), 31);
	do {
		tmp = reg32_read(DRC_PERF_MON_MRR0_DAT(0));
	} while ((tmp & 0x8) == 0);
	tmp = reg32_read(DRC_PERF_MON_MRR1_DAT(0));
	reg32_write(DRC_PERF_MON_MRR0_DAT(0), 0x4);
	while(tmp) { //try to find a significant byte in the word
		if(tmp & 0xff) {
			tmp &= 0xff;
			break;
		}
		tmp >>= 8;
	}
	return tmp;
}
#define DEFAULT (('D' << 24) + ('E' << 16 ) + ( 'F' << 8 ) + 'A')
#if 0
struct lpddr4_desc {
	char name[16];
	unsigned int id;
	unsigned int size;
	unsigned int count;
	/* an optional field
	 * use it if default is not the
	 * 1-st array entry */
	unsigned int _default;
	/* An optional field to distiguish DRAM chips that
	 * have different geometry, though return the same MRR.
	 * Default value 0xff
	 */
	u8	subind;
	struct dram_timing_info *timing;
	char *desc[4];
};

static const struct lpddr4_desc lpddr4_array[] = {
	{ .name = "Etron",	.id = 0xff070018, .subind = 0xff, .size = 4096, .count = 1, .timing = &dram_timing_ff070018},
#if 0
	{ .name = "Etron",	.id = 0x1a000008, .subind = 0xff, .size = 1024, .count = 1, .timing = &ucm_dram_timing_1a000008},
	{ .name = "ISSI",	.id = 0x13000210, .subind = 0xff, .size = 4096, .count = 1, .timing = &ucm_dram_timing_13000210}, // TBD -- fake assignement!
	{ .name = "ISSI",	.id = 0x13000210, .subind = 0xff, .size = 2048, .count = 1, .timing = &ucm_dram_timing_13000210},
	{ .name = "ISSI",	.id = 0x1b000008, .subind = 0xff, .size = 1024, .count = 1, .timing = &ucm_dram_timing_1b000008},
#ifdef CONFIG_TARGET_MCM_IMX8M_MINI
	{ .name = "Nanya",	.id = 0x05000010, .subind = 0xff, .size = 2048, .count = 1, .timing = &ucm_dram_timing_01061010},
#else
	{ .name = "Nanya",	.id = 0x05000010, .subind = 0xff, .size = 2048, .count = 1, .timing = &ucm_dram_timing_05000010},
#endif
#endif
	{ .name = "Samsung",	.id = 0x01061010, .subind = 0x04, .size = 4096, .count = 1, .timing = &ucm_dram_timing_ff000110},
	{ .name = "Samsung",	.id = 0x01061010, .subind = 0x02, .size = 2048, .count = 1, .timing = &ucm_dram_timing_01061010},
	{ .name = "Samsung",	.id = 0x01080010, .subind = 0x04, .size = 4096, .count = 1, .timing = &ucm_dram_timing_ff000110},
	{ .name = "Samsung",	.id = 0x01080010, .subind = 0x02, .size = 2048, .count = 1, .timing = &ucm_dram_timing_01080010},
	{ .name = "Samsung",	.id = 0x01050008, .subind = 0xff, .size = 1024, .count = 1, .timing = &ucm_dram_timing_01050008},
	{ .name = "Samsung",	.id = 0x01060008, .subind = 0xff, .size = 1024, .count = 1, .timing = &ucm_dram_timing_01050008},
	{ .name = "Alliance",	.id = 0x52000008, .subind = 0xff, .size = 1024, .count = 1, .timing = &ucm_dram_timing_01050008},
	{ .name = "Kingston",	.id = 0xff050010, .subind = 0xff, .size = 2048, .count = 1, .timing = &ucm_dram_timing_01061010},
	{ .name = "Kingston",	.id = 0xff000010, .subind = 0x04, .size = 4096, .count = 1, .timing = &ucm_dram_timing_ff000110},
	{ .name = "Kingston",	.id = 0xff000010, .subind = 0x02, .size = 2048, .count = 1, .timing = &ucm_dram_timing_01061010},
	{ .name = "Micron",	.id = 0xff020008, .subind = 0xff, .size = 2048, .count = 1, .timing = &ucm_dram_timing_ff020008},
	{ .name = "Micron",	.id = 0xff000110, .subind = 0xff, .size = 4096, .count = 1, .timing = &ucm_dram_timing_ff000110},
};
#endif
static unsigned int lpddr4_get_mr(void)
{
	int i = 0, attempts = 5;
	unsigned int ddr_info = 0;
	unsigned int regs[] = { 5, 6, 7, 8 };

	do {
		for ( i = 0 ; i < ARRAY_SIZE(regs) ; i++ ) {
			unsigned int data = 0;
			data = lpddr4_mr_read_and_refine(0xF, regs[i]);
			ddr_info <<= 8;
			ddr_info += (data & 0xFF);
		}
		if ((ddr_info != 0xFFFFFFFF) && (ddr_info != 0))
			break; // The attempt was successfull
	} while ( --attempts );
	return	ddr_info;
}

static void spl_tcm_init(struct lpddr4_tcm_desc *lpddr4_tcm_desc) {
    if (lpddr4_tcm_desc->sign == DEFAULT)
	    return;

    lpddr4_tcm_desc->sign = DEFAULT;
    lpddr4_tcm_desc->index = 0;
}

static void spl_tcm_fini(struct lpddr4_tcm_desc *lpddr4_tcm_desc) {
    if (lpddr4_tcm_desc->sign != DEFAULT)
	    return;

    lpddr4_tcm_desc->sign = ~DEFAULT;
    lpddr4_tcm_desc->index = 0;
}

#define UBOOT_START_SECTOR 33
//#define UBOOT_START_SECTOR (CONFIG_IMX_BOOT_SEEK * 2) /*Convert kB to secs*/
static void read_timing_from_mmc(int idx)
{
	u32 bootdev;
	struct spl_image_info image;
	struct mmc *mmc;
	int err, count;
	struct blk_desc *bd;
	unsigned int sec_cnt;

	bootdev = spl_boot_device();

	image.load_addr = (long unsigned int)&timing_block,
	image.boot_device = bootdev,
	image.size = sizeof(timing_block),

	err = spl_mmc_find_device(&mmc, bootdev);
	if(err) {
		printf("%s: Can`t find mmc for boot device %d errno %d\n",
			__func__, err, bootdev);
		while(42);
	}

	err = mmc_init(mmc);
	if(err) {
		printf("%s: Can`t initialize mmc for boot device %d errno %d\n",
			__func__, err, bootdev);
		while(42);
	}

	bd = mmc_get_blk_desc(mmc);
	sec_cnt = (sizeof(timing_block) + bd->blksz - 1) / bd->blksz;

	count = blk_dread(bd,
		CONFIG_LPDDR4_TIMINGS_BIN_SECTOR + UBOOT_START_SECTOR + idx * sec_cnt,
		sec_cnt, &timing_block);

	if(sec_cnt != count) {
		printf("%s: %d sector read %d sector necessary\n",
				__func__, count, sec_cnt);
		while(42);
	}
}

/* Update pointers to get an operable timing structure, basing on the block read from eMMC*/
static void relocate_timing_block(void)
{
	timing_block.ddr_dram_fsp_msg[0].fsp_cfg = timing_block.ddr_fsp0_cfg;
	timing_block.ddr_dram_fsp_msg[1].fsp_cfg = timing_block.ddr_fsp1_cfg;
	timing_block.ddr_dram_fsp_msg[2].fsp_cfg = timing_block.ddr_fsp2_cfg;
	timing_block.ddr_dram_fsp_msg[3].fsp_cfg = timing_block.ddr_fsp0_2d_cfg;
	timing_block.dram_timing.fsp_msg = timing_block.ddr_dram_fsp_msg;

	timing_block.dram_timing.ddrc_cfg = timing_block.ddr_ddrc_cfg;
	timing_block.dram_timing.ddrphy_cfg = timing_block.ddr_ddrphy_cfg;
	timing_block.dram_timing.ddrphy_trained_csr = timing_block.ddr_ddrphy_trained_csr;
	timing_block.dram_timing.ddrphy_pie = timing_block.ddr_phy_pie;
}

static int find_timing_block(unsigned long long id, u8 subind)
{
	read_timing_from_mmc(0);
	for(int i=1; timing_block.id; ++i){
		if(id == timing_block.id && subind == timing_block.subind) {
			relocate_timing_block();
			return 0;
		}
		read_timing_from_mmc(i);
	}

	printf("LPDDR timing 0x%x (0x%x) not found\n", id, subind);
	printf("Supported LPDDR timings:\n");

	read_timing_from_mmc(0);
	for(int i=1; timing_block.id; ++i){
		printf("\t0x%x (0x%x)\n", timing_block.id, timing_block.subind);
		read_timing_from_mmc(i);
	}
	return -ENOENT;
}
#define SPL_TCM_DATA 0x7e0000
#define SPL_TCM_INIT spl_tcm_init(lpddr4_tcm_desc)
#define SPL_TCM_FINI spl_tcm_fini(lpddr4_tcm_desc)

void spl_dram_init(void)
{
	unsigned int ddr_info = 0xdeadbeef;
	unsigned int ddr_info_mrr = 0xdeadbeef;
	unsigned int ddr_found = 0;
	int i = 0;

	struct lpddr4_tcm_desc *lpddr4_tcm_desc = (struct lpddr4_tcm_desc *) SPL_TCM_DATA;

	if (lpddr4_tcm_desc->sign != DEFAULT) {
		/* get ddr type from the eeprom if not in tcm scan mode */
		ddr_info = cl_eeprom_get_ddrinfo();
		if (0 != ddr_info && 0xffffffff != ddr_info) {
			if(find_timing_block(timing_block.id, cl_eeprom_get_subind())) {
				ddr_found = 1;
			}
		}
	}

	/* Walk trought all available ddr ids and apply
	 * one by one. Save the index at the tcm memory that
	 * persists after the reset.
	 */
	if (ddr_found == 0) {

		SPL_TCM_INIT;

		read_timing_from_mmc(lpddr4_tcm_desc->index);
		lpddr4_tcm_desc->index += 1;
		if (0 != timing_block.id && 0xffffffff != timing_block.id) { 
			relocate_timing_block();
			ddr_info = timing_block.id;
			printf("DDRINFO: Cfg attempt: [ %d ] ID 0x%x(0x%x)\n", lpddr4_tcm_desc->index, timing_block.id, timing_block.subind);
		} else {
			/* Ran out all available ddr setings */
			printf("DDRINFO: Ran out all [ %lu ] cfg attempts. A non-supported configuration.\n", lpddr4_tcm_desc->index + 1);
			while ( 1 ) {};
		}
	}

	printf("DDRINFO(%s): %s %dG @ %d MHz\n", (ddr_found ? "eeprom" : "try" ), timing_block.name, timing_block.size, timing_block.dram_timing.fsp_table[0]);

	//Initialize the common part of all trainigs
	//lpddr4_array[i].timing->ddrphy_trained_csr = ddr_ddrphy_trained_csr;
	//lpddr4_array[i].timing->ddrphy_trained_csr_num = ARRAY_SIZE(ddr_ddrphy_trained_csr);

	if (ddr_init(&timing_block.dram_timing)) {
		SPL_TCM_INIT;
		do_reset(NULL,0,0,NULL);
	}

	ddr_info_mrr = lpddr4_get_mr();
	if (ddr_info_mrr == 0xFFFFFFFF ) {
		printf("DDRINFO(M): mr5-8 [ 0x%x ] is invalid; reset\n", ddr_info_mrr);
		SPL_TCM_INIT;
		do_reset(NULL,0,0,NULL);
	}

	printf("DDRINFO(mrr): mr5-8 [ 0x%x ]\n", ddr_info_mrr);
	printf("DDRINFO(%s): mr5-8 [ 0x%x ]\n", (ddr_found ? "eeprom" : "try" ), ddr_info);

	if (ddr_info_mrr != ddr_info) {
		SPL_TCM_INIT;
		do_reset(NULL,0,0,NULL);
	}

	SPL_TCM_FINI;

	if (ddr_found == 0) {
		/* Update eeprom */
		cl_eeprom_set_ddrinfo(ddr_info_mrr);
		cl_eeprom_set_subind(timing_block.subind);
		mdelay(10);
		ddr_info = cl_eeprom_get_ddrinfo();
		mdelay(10);
		/* make sure that the ddr_info has reached the eeprom */
		printf("DDRINFO(eeprom): mr5-8 [ 0x%x ], read back\n", ddr_info);
		if (ddr_info_mrr != ddr_info || cl_eeprom_get_subind() != timing_block.subind) {
			printf("DDRINFO(EEPROM): make sure that the eeprom is accessible\n");
			printf("DDRINFO(EEPROM): i2c dev 1; i2c md 0x51 0x40 0x50\n");
		}
	}
#ifdef CONFIG_SPL_REPORT_FAKE_MEMSIZE
	/* Pass the dram size to th U-Boot through the tcm memory */
	{ /* To figure out what to store into the TCM buffer */
	  /* For debug purpouse only. To override the real memsize */
		unsigned int ddr_tcm_size = cl_eeprom_get_osize();
		if ((ddr_tcm_size == 0) || (ddr_tcm_size == -1))
			ddr_tcm_size = timing_block.size;

		lpddr4_tcm_desc->size = ddr_tcm_size;
	}
#else
	lpddr4_tcm_desc->size = timing_block.size;
	lpddr4_tcm_desc->sign = timing_block.id;
#endif
}
