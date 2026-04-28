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
#include "lpddr4_timing_block.h"
#include "ddr_ddrphy_trained_csr.h"

/* Forward declarations */
u32 cl_eeprom_get_ddrinfo(void);
u32 cl_eeprom_set_ddrinfo(u32 ddrinfo);
u8 cl_eeprom_get_subind(void);
u8 cl_eeprom_set_subind(u8 subind);

/* Placeholder to be filled with a real timing table read from MMC */
__attribute__ ((section (".data")))
static struct lpddr4_timing_block timing_block[] = {
	{.magic = LPDDR_EMPTY_MAGIC,},
	{.magic = LPDDR_EMPTY_MAGIC,},
	{.magic = LPDDR_EMPTY_MAGIC,},
	{.magic = LPDDR_EMPTY_MAGIC,},
	{.magic = LPDDR_EMPTY_MAGIC,},
	{.magic = LPDDR_EMPTY_MAGIC,},
};

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
static const struct timing_desc lpddr4_array[] = {
	{ .name = "Kingston",	.id = 0xff070010, .subind = 0x04, .timing_sign = 0xff070110},//C3222PM4CDGUI-U
	{ .name = "Kingston",	.id = 0xff070010, .subind = 0x02, .timing_sign = 0xff070010},//D1621PM4CDGUI
#ifdef CONFIG_TARGET_MCM_IMX8M_MINI
	{ .name = "Nanya",	.id = 0x05000010, .subind = 0xff, .timing_sign = 0x01061010},
#else
	{ .name = "Nanya",	.id = 0x05000010, .subind = 0xff, .timing_sign = 0x05000010},
#endif
	{ .name = "ISSI",	.id = 0x1b000008, .subind = 0xff, .timing_sign = 0x1b000008}, //IS43LQ32256B-062BLI
	{ .name = "ISSI",	.id = 0x1b010008, .subind = 0xff, .timing_sign = 0x1b000008}, //IS43LQ32256C-046BLI
	{ .name = "Etron",	.id = 0x1a000008, .subind = 0xff, .timing_sign = 0x1b000008}, //EM6LF32MBAJB-46ISH
	{ .name = "Winbond",	.id = 0x08000008, .subind = 0xff, .timing_sign = 0x1b000008}, //W66DP2RQQAGJ
	//{ .name = "ISSI",	.id = 0x13000210, .subind = 0x04, .timing_sign = 0}, //
	{ .name = "ISSI",	.id = 0x13000210, .subind = 0x02, .timing_sign = 0xff070010}, //IS43LQ32512A-053BLI
	{ .name = "Samsung",	.id = 0x01061010, .subind = 0x04, .timing_sign = 0xff000110},
	{ .name = "Samsung",	.id = 0x01061010, .subind = 0x02, .timing_sign = 0x01061010},
	{ .name = "Samsung",	.id = 0x01080010, .subind = 0x04, .timing_sign = 0xff000110},
	{ .name = "Samsung",	.id = 0x01080010, .subind = 0x02, .timing_sign = 0x01061010},
	{ .name = "Samsung",	.id = 0x01050008, .subind = 0xff, .timing_sign = 0x01050008},
	{ .name = "Samsung",	.id = 0x01060008, .subind = 0xff, .timing_sign = 0x01050008},
	{ .name = "Alliance",	.id = 0x52000008, .subind = 0xff, .timing_sign = 0x01050008},
	{ .name = "Kingston",	.id = 0xff050010, .subind = 0xff, .timing_sign = 0x01061010},
	{ .name = "Kingston",	.id = 0xff000010, .subind = 0x04, .timing_sign = 0xff000110},
	{ .name = "Kingston",	.id = 0xff000010, .subind = 0x02, .timing_sign = 0x01061010},
	{ .name = "Micron",	.id = 0xff020008, .subind = 0xff, .timing_sign = 0xff020008},
	{ .name = "Micron",	.id = 0xff000110, .subind = 0xff, .timing_sign = 0xff000110},
	{ .name = "Etron",	.id = 0xff070018, .subind = 0xff, .timing_sign = 0xff070018}, //EM6LH32MVAJA
};

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

static int spl_mmc_get_device_index(u32 boot_device)
{
	switch (boot_device) {
	case BOOT_DEVICE_MMC1:
		return 0;
	case BOOT_DEVICE_MMC2:
	case BOOT_DEVICE_MMC2_2:
		return 1;
	}

#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
	printf("spl: unsupported mmc boot device.\n");
#endif

	return -ENODEV;
}

static int spl_mmc_find_device(struct mmc **mmcp, u32 boot_device)
{
	int err, mmc_dev;

	mmc_dev = spl_mmc_get_device_index(boot_device);
	if (mmc_dev < 0)
		return mmc_dev;

#if CONFIG_IS_ENABLED(DM_MMC)
	err = mmc_init_device(mmc_dev);
#else
	err = mmc_initialize(NULL);
#endif /* DM_MMC */
	if (err) {
#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
		printf("spl: could not initialize mmc. error: %d\n", err);
#endif
		return err;
	}
	*mmcp = find_mmc_device(mmc_dev);
	err = *mmcp ? 0 : -ENODEV;
	if (err) {
#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
		printf("spl: could not find mmc device %d. error: %d\n",
			mmc_dev, err);
#endif
		return err;
	}

	return 0;
}

/* Read a timing block from eMMC
* return:
*	0 success
*	negative Error code /TBD/
*	positive Not a valid timig block is found @ the idx
*/
static int read_timing_from_mmc(int idx)
{
	u32 bootdev;
	struct spl_image_info image;
	struct mmc *mmc;
	int err, count;
	struct blk_desc *bd;
	unsigned int sec_cnt;
	unsigned int hwpart;

	bootdev = spl_boot_device();

	image.load_addr = (long unsigned int)&timing_block[0],
	image.boot_device = bootdev,
	image.size = sizeof(timing_block[0]),

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

	if (mmc->part_support && mmc->part_config != MMCPART_NOAVAILABLE) {
		hwpart = EXT_CSD_EXTRACT_BOOT_PART(mmc->part_config);
		hwpart = (7 == hwpart)? 0 : hwpart; // the User part is denoted as 7, but 0 is to switch to
		mmc_switch_part(mmc, hwpart);
	}
	bd = mmc_get_blk_desc(mmc);
	sec_cnt = (sizeof(timing_block[0]) + bd->blksz - 1) / bd->blksz;

	count = blk_dread(bd,
		LPDDR4_TIMINGS_BIN_SECTOR + IMX_BOOT_SEEK * 2 + idx * sec_cnt,
		sec_cnt,
		&timing_block[0]);
	if(sec_cnt != count) {
		printf("%s: %d sector read %d sector necessary\n",
				__func__, count, sec_cnt);
		while(42);
	}

	if(!!strncmp(timing_block[0].magic, LPDDR_BLOCK_MAGIC, sizeof(LPDDR_BLOCK_MAGIC))) {
		printf("No LPDDR block magic found\n");
		return 1;
	}

	return 0;
}

/* Update pointers to get a timing structure operable, basing on a block, read from a storage or found in SPL*/
static void relocate_timing_block(int num)
{
	timing_block[num].ddr_dram_fsp_msg[0].fsp_cfg = timing_block[num].ddr_fsp0_cfg;
	timing_block[num].ddr_dram_fsp_msg[1].fsp_cfg = timing_block[num].ddr_fsp1_cfg;
	timing_block[num].ddr_dram_fsp_msg[2].fsp_cfg = timing_block[num].ddr_fsp2_cfg;
	timing_block[num].ddr_dram_fsp_msg[3].fsp_cfg = timing_block[num].ddr_fsp0_2d_cfg;
	timing_block[num].dram_timing.fsp_msg = timing_block[num].ddr_dram_fsp_msg;

	timing_block[num].dram_timing.ddrc_cfg = timing_block[num].ddr_ddrc_cfg;
	timing_block[num].dram_timing.ddrphy_cfg = timing_block[num].ddr_ddrphy_cfg;
	timing_block[num].dram_timing.ddrphy_pie = timing_block[num].ddr_phy_pie;

	// Initialize the common part of all trainigs
	timing_block[num].dram_timing.ddrphy_trained_csr = ddr_ddrphy_trained_csr;
	timing_block[num].dram_timing.ddrphy_trained_csr_num = ARRAY_SIZE(ddr_ddrphy_trained_csr);
}

/* Find required timing block in SPL body; if no one presented find on storage
* return index in array of timing blocks
* block, found on a storage is copied @ index 0
*/
static int find_timing_block(unsigned long id)
{
unsigned i;
	// First, check, is there a single timing block, incorporated in SPL code directly.
	for(i=0; ARRAY_SIZE(timing_block) > i; ++i) {
		if(!strncmp(timing_block[i].magic, LPDDR_SINGLE_MAGIC, sizeof(LPDDR_SINGLE_MAGIC))) {
			printf("Timing block[%i] 0x%x integrated ", i, timing_block[i].id);
			if(id == timing_block[i].id) {
				printf("match\n");
				relocate_timing_block(i);
				return i;
			} else { //The boot loader with an incorporated timing has no external timings available
				printf("unmatch (0x%x required)\n", id);
			}
		} else {
			break;
		}
	}
	if(0 != i) { // This is an SPL with integrated timings, however no one match
		printf("No one of integrated timing matches (0x%x required)\n", id);
		mdelay(60); // Let printf complete
		do_reset(NULL,0,0,NULL);
	}

	// This is an SPL w/o integrated timing; search a storage
	for(i=0; ARRAY_SIZE(lpddr4_array) > i; ++i) { // Very rough upper limit, just in case
		printf("LPDDR timing 0x%x search entry %i\t", id, i);
		int ret = read_timing_from_mmc(i);
		if(0 == ret) {
			if(id == timing_block[0].id) {
				printf("0x%x found\n", timing_block[0].id);
				relocate_timing_block(0);
				return 0;
			} else {
				printf("not found (0x%x)\n", timing_block[0].id);
			}
		}
		else if( 0 > ret) {
			printf(" error %i\n", ret);
			return ret;
		}
		else if( 0 < ret) {
			printf("end\n");
			return -ENOENT;
		}
	}

	printf("Upper limint reached\n");
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
	int ind = 0;

	struct lpddr4_tcm_desc *lpddr4_tcm_desc = (struct lpddr4_tcm_desc *) SPL_TCM_DATA;

	if (lpddr4_tcm_desc->sign != DEFAULT) {
		/* get ddr type from the eeprom if not in tcm scan mode */
		ddr_info = cl_eeprom_get_ddrinfo();
		unsigned int subind = cl_eeprom_get_subind();
		for ( i = 0; i < ARRAY_SIZE(lpddr4_array); i++ ) {
			if (lpddr4_array[i].id == ddr_info &&
			lpddr4_array[i].subind == subind) {
				ddr_found = 1;
				break;
			}
		}
	}

	/* Walk trought all available ddr ids and apply
	 * one by one. Save the index at the tcm memory that
	 * persists after the reset.
	 */
	if (ddr_found == 0) {

		SPL_TCM_INIT;

		if (lpddr4_tcm_desc->index < ARRAY_SIZE(lpddr4_array)) {
			printf("DDRINFO: Cfg attempt: [ %d/%lu ]\n", lpddr4_tcm_desc->index+1, ARRAY_SIZE(lpddr4_array));
			i = lpddr4_tcm_desc->index;
			lpddr4_tcm_desc->index += 1;
		} else {
			/* Ran out all available ddr setings */
			printf("DDRINFO: Ran out all [ %lu ] cfg attempts. A non supported configuration.\n", ARRAY_SIZE(lpddr4_array));
			while ( 1 ) {};
		}
	}
	ddr_info = lpddr4_array[i].id;

	ind = find_timing_block(lpddr4_array[i].timing_sign);
	if(0 > ind) {
		printf("DDRINFO: Timing block ID = 0x%x[%i] for 0x%x.0x%x not found. A non-supported configuration.\n",
			lpddr4_array[i].timing_sign, i, lpddr4_array[i].id , lpddr4_array[i].subind);
		while ( 1 ) {};
	}

	printf("DDRINFO(%s): %s %dG @ %d MHz\n", (ddr_found ? "eeprom" : "try" ), lpddr4_array[i].name,
		timing_block[ind].size, timing_block[ind].dram_timing.fsp_table[0]);

	if (ddr_init(&timing_block[ind].dram_timing)) {
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

	mdelay(60); //To let printf have time to spit to console
	if (ddr_info_mrr != ddr_info) {
		SPL_TCM_INIT;
		do_reset(NULL,0,0,NULL);
	}

	SPL_TCM_FINI;

	if (ddr_found == 0) {
		/* Update eeprom */
		cl_eeprom_set_ddrinfo(ddr_info_mrr);
		mdelay(50);
		cl_eeprom_set_subind(lpddr4_array[i].subind);
		mdelay(50);
		ddr_info = cl_eeprom_get_ddrinfo();
		mdelay(50);
		/* make sure that the ddr_info has reached the eeprom */
		printf("DDRINFO(eeprom): mr5-8 [ 0x%x ], read back\n", ddr_info);
		if (ddr_info_mrr != ddr_info || cl_eeprom_get_subind() != lpddr4_array[i].subind) {
			printf("%i 0x%x 0x%x 0x%x 0x%x\n", i, ddr_info_mrr, ddr_info, lpddr4_array[i].subind, cl_eeprom_get_subind() );
			printf("DDRINFO(EEPROM): make sure that the eeprom is accessible\n");
			printf("DDRINFO(EEPROM): i2c dev 1; i2c md 0x51 0x40 5\n");
		}
	}
#ifdef CONFIG_SPL_REPORT_FAKE_MEMSIZE
	/* Pass the dram size to th U-Boot through the tcm memory */
	{ /* To figure out what to store into the TCM buffer */
	  /* For debug purpouse only. To override the real memsize */
		unsigned int ddr_tcm_size = cl_eeprom_get_osize();
		if ((ddr_tcm_size == 0) || (ddr_tcm_size == -1))
			ddr_tcm_size = timing_block[ind].size;

		lpddr4_tcm_desc->size = ddr_tcm_size;
	}
#else
	lpddr4_tcm_desc->size = timing_block[ind].size;
#endif
	lpddr4_tcm_desc->sign = lpddr4_array[i].id;
	lpddr4_tcm_desc->timing = lpddr4_array[i].timing_sign;
}
