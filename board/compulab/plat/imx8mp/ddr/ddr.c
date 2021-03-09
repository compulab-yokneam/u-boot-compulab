#include <common.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch-imx8m/imx8m_ddr.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/gpio.h>
#include <asm-generic/gpio.h>
#include <asm/arch/ddr.h>
#include <asm/arch/imx8mp_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/gpio.h>
#include "ddr.h"

/* Forward declarations */
u32 cl_eeprom_get_ddrinfo(void);
u32 cl_eeprom_set_ddrinfo(u32 ddrinfo);
u32 cl_eeprom_get_subind(void);
u32 cl_eeprom_set_subind(u32 subind);

#define DEFAULT (('D' << 24) + ('E' << 16 ) + ( 'F' << 8 ) + 'A')

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

static struct lpddr4_tcm_desc spl_tcm_data;
#define SPL_TCM_DATA &spl_tcm_data
#define SPL_TCM_INIT spl_tcm_init(lpddr4_tcm_desc)
#define SPL_TCM_FINI spl_tcm_fini(lpddr4_tcm_desc)

static int _spl_dram_init(void)
{
	unsigned int ddr_info = 0xdeadbeef;
	unsigned int ddr_info_mrr = 0xdeadbeef;
	unsigned int ddr_found = 0;
	int i = 0;

	struct lpddr4_tcm_desc *lpddr4_tcm_desc = SPL_TCM_DATA;

	if (lpddr4_tcm_desc->sign != DEFAULT) {
		/* get ddr type from the eeprom if not in tcm scan mode */
		ddr_info = cl_eeprom_get_ddrinfo();
		for ( i = 0; i < ARRAY_SIZE(lpddr4_array); i++ ) {
			if (lpddr4_array[i].id == ddr_info &&
			lpddr4_array[i].subind == cl_eeprom_get_subind()) {
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
		ddr_info = lpddr4_array[i].id;
	} else

	printf("DDRINFO(%s): %s %dG @ %d MHz\n", (ddr_found ? "D" : "?" ), lpddr4_array[i].name,
			lpddr4_array[i].size, lpddr4_array[i].timing->fsp_table[0]);

	if (ddr_init(lpddr4_array[i].timing)) {
		SPL_TCM_INIT;
		do_reset(NULL,0,0,NULL);
	}

	ddr_info_mrr = lpddr4_get_mr();
	if (ddr_info_mrr == 0xFFFFFFFF ) {
		printf("DDRINFO(M): mr5-8 [ 0x%x ] is invalid; reset\n", ddr_info_mrr);
		SPL_TCM_INIT;
		return 1;
	}

	printf("DDRINFO(M): mr5-8 [ 0x%x ]\n", ddr_info_mrr);
	printf("DDRINFO(%s): mr5-8 [ 0x%x ]\n", (ddr_found ? "E" : "T" ), ddr_info);

	if (ddr_info_mrr != ddr_info) {
		SPL_TCM_INIT;
		return 1;
	}

	SPL_TCM_FINI;

	if (ddr_found == 0) {
		/* Update eeprom */
		cl_eeprom_set_ddrinfo(ddr_info_mrr);
		mdelay(10);
		ddr_info = cl_eeprom_get_ddrinfo();
		mdelay(10);
		cl_eeprom_set_subind(lpddr4_array[i].subind);
		/* make sure that the ddr_info has reached the eeprom */
		printf("DDRINFO(E): mr5-8 [ 0x%x ], read back\n", ddr_info);
		if (ddr_info_mrr != ddr_info || cl_eeprom_get_subind() != lpddr4_array[i].subind) {
			printf("DDRINFO(EEPROM): make sure that the eeprom is accessible\n");
			printf("DDRINFO(EEPROM): i2c dev 1; i2c md 0x51 0x40 0x50\n");
		}
	}

	lpddr4_tcm_desc->size = lpddr4_array[i].size;
	return 0;
}

int cl_eeprom_buffer_write(uint offset, uchar *buf, int len);
int cl_eeprom_buffer_read(uint offset, uchar *buf, int len);

static inline void lpddr4_data_get(struct lpddr4_tcm_desc *lpddr4_tcm_desc) {
	cl_eeprom_buffer_read(0, (uchar *)lpddr4_tcm_desc, sizeof(struct lpddr4_tcm_desc));
}

static inline void lpddr4_data_set(struct lpddr4_tcm_desc *lpddr4_tcm_desc) {
	cl_eeprom_buffer_write(0, (uchar *)lpddr4_tcm_desc, sizeof(struct lpddr4_tcm_desc));
}

#define MNG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)
static iomux_v3_cfg_t const mng_pads[] = {
	MX8MP_PAD_ECSPI2_MOSI__GPIO5_IO11  | MUX_PAD_CTRL(MNG_PAD_CTRL),
};

static int get_mng_gpio(void)
{
    imx_iomux_v3_setup_multiple_pads(mng_pads, ARRAY_SIZE(mng_pads));
    return gpio_get_value(IMX_GPIO_NR(5, 11));
}

static void spl_dram_eclean(void)
{
    unsigned int ddr_info = 0xffffffff;
    memset(SPL_TCM_DATA, 0xff, sizeof(struct lpddr4_tcm_desc));
    lpddr4_data_set(SPL_TCM_DATA);
    mdelay(10);
    cl_eeprom_set_ddrinfo(ddr_info);
    mdelay(10);
    ddr_info = cl_eeprom_get_ddrinfo();
    printf("DDRINFO(E): mr5-8 [ 0x%x ], read back\n", ddr_info);
    printf("%s The epprom was cleaned up ... \n",__func__);
} 

void spl_dram_init(void)
{
    if (!get_mng_gpio()) {
        spl_dram_eclean();
        while (1) {};
    }
	lpddr4_data_get(SPL_TCM_DATA);
	if (_spl_dram_init()) {
		lpddr4_data_set(SPL_TCM_DATA);
		printf("%s Reset ... \n",__func__);
		do_reset(NULL,0,0,NULL);
	}

	printf("%s Continue w/out reset ... \n",__func__);
}
