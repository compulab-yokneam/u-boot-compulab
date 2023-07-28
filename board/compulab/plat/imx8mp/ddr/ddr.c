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
#include <linux/delay.h>
#include "ddr.h"

/* Forward declarations */
u32 cl_eeprom_get_ddrinfo(void);
u32 cl_eeprom_set_ddrinfo(u32 ddrinfo);
u32 cl_eeprom_get_subind(void);
u32 cl_eeprom_set_subind(u32 subind);
void reset_misc(void);

static void do_reset_spl(void) { reset_misc(); }

#define DEFAULT (('D' << 24) + ('E' << 16 ) + ( 'F' << 8 ) + 'A')
#define VALID 0xCAFECAFE
#define DDR_INIT_IN  0xCACACACA
#define DDR_INIT_OUT 0x0C0C0C0C

int cl_eeprom_buffer_write(uint offset, uchar *buf, int len);
int cl_eeprom_buffer_read(uint offset, uchar *buf, int len);

static inline void lpddr4_data_get(struct lpddr4_tcm_desc *lpddr4_tcm_desc) {
	cl_eeprom_buffer_read(0, (uchar *)lpddr4_tcm_desc, sizeof(struct lpddr4_tcm_desc));
}

static inline void lpddr4_data_set(struct lpddr4_tcm_desc *lpddr4_tcm_desc) {
	cl_eeprom_buffer_write(0, (uchar *)lpddr4_tcm_desc, sizeof(struct lpddr4_tcm_desc));
}

static void spl_tcm_init(struct lpddr4_tcm_desc *lpddr4_tcm_desc) {
    if (lpddr4_tcm_desc->sign == DEFAULT)
	    return;

    memset((char *)lpddr4_tcm_desc, 0x0, sizeof(struct lpddr4_tcm_desc));
    lpddr4_tcm_desc->sign = DEFAULT;
}

static void spl_tcm_fini(struct lpddr4_tcm_desc *lpddr4_tcm_desc) {
    if (lpddr4_tcm_desc->sign == VALID)
	    return;

    memset((char *)lpddr4_tcm_desc, 0x0, sizeof(struct lpddr4_tcm_desc));
    lpddr4_tcm_desc->sign = VALID;
}

static void spl_tcm_clr(struct lpddr4_tcm_desc *lpddr4_tcm_desc) {
    memset((char *)lpddr4_tcm_desc, 0xFF, sizeof(struct lpddr4_tcm_desc));
}

static struct lpddr4_tcm_desc spl_tcm_data;
#define SPL_TCM_DATA &spl_tcm_data
#define SPL_TCM_CLR  spl_tcm_clr(lpddr4_tcm_desc)
#define SPL_TCM_INIT spl_tcm_init(lpddr4_tcm_desc)
#define SPL_TCM_FINI spl_tcm_fini(lpddr4_tcm_desc)

static int _spl_dram_init(void)
{
	unsigned int ddr_info = 0xdeadbeef;
	unsigned char subind = 0xfF;
	unsigned int ddr_info_mrr = 0xdeadbeef;
	unsigned int ddr_found = 0;
	int i = 0;

	struct lpddr4_tcm_desc *lpddr4_tcm_desc = SPL_TCM_DATA;

	if (lpddr4_tcm_desc->sign == VALID) {
		/* get ddr type from the eeprom if not in tcm scan mode */
		ddr_info = cl_eeprom_get_ddrinfo();
		subind = cl_eeprom_get_subind();

		printf("DDRINFO: EEPROM VALID DATA [ [ %x ] = %x %x \n",
                lpddr4_tcm_desc->sign, ddr_info, subind);

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

        /* Let's check the latest training status */
        if (lpddr4_tcm_desc->ddr_init_status == DDR_INIT_IN) {
            printf("%s Bad attempt %d skip\n",__func__,(lpddr4_tcm_desc->index+1));
			lpddr4_tcm_desc->index += 1;
        }

		if (lpddr4_tcm_desc->index < ARRAY_SIZE(lpddr4_array)) {
			printf("DDRINFO: Cfg attempt: [ %d/%lu ]\n", lpddr4_tcm_desc->index+1, ARRAY_SIZE(lpddr4_array));
			i = lpddr4_tcm_desc->index;
			lpddr4_tcm_desc->index += 1;
		} else {
			/* Ran out all available ddr setings */
			SPL_TCM_CLR;
			printf("DDRINFO: Ran out all [ %lu ] cfg attempts. A non supported configuration.\n", ARRAY_SIZE(lpddr4_array));
            return -1;
		}

		ddr_info = lpddr4_array[i].id;
	}

	printf("DDRINFO(%s): %s %dMB @ %d MHz\n", (ddr_found ? "D" : "?" ), lpddr4_array[i].name,
			lpddr4_array[i].size, lpddr4_array[i].timing->fsp_table[0]);


	if (ddr_found == 0) {
        /* This is a discovery case, save in ddr_init_status 'cause it can stack */
        lpddr4_tcm_desc->ddr_init_status = DDR_INIT_IN;
        /* Save the data before training */
        lpddr4_data_set(SPL_TCM_DATA);
    }

	if (ddr_init(lpddr4_array[i].timing)) {
		SPL_TCM_INIT;
		return 1;
	}

	if (ddr_found == 0) {
        /* This is a discovery case, save out ddr_init_status */
        lpddr4_tcm_desc->ddr_init_status = DDR_INIT_OUT;
        /* Save the data after training */
        lpddr4_data_set(SPL_TCM_DATA);
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

	lpddr4_tcm_desc->size = lpddr4_array[i].size;

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
        /* Set the data valid */
	    SPL_TCM_FINI;
        /* Return with 1 in ordert to make the caller save the ddr discovery status */
	    return 1;
	}

	return 0;
}

static inline void spl_dram_share_info(void) {
#ifdef SHARED_DDR_INFO
    struct lpddr4_tcm_desc *lpddr4_tcm_desc = (void *) SHARED_DDR_INFO;
    memcpy(lpddr4_tcm_desc, SPL_TCM_DATA, sizeof(struct lpddr4_tcm_desc));
#endif
}

void spl_dram_init(void)
{
    int rc=0;

    lpddr4_data_get(SPL_TCM_DATA);

    rc=_spl_dram_init();

    if (rc==0) {
        spl_dram_share_info();
        return;
    }

    lpddr4_data_set(SPL_TCM_DATA);

    if (rc==1) {
        printf("%s Reset ... \n",__func__);
        do_reset_spl();
    }

    while ( 1 ) { };
}
