#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
"""Compare production PHY execution traces for expanded and compact probe/init."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
header = (root / 'arch/arm/include/asm/arch-imx8m/ddr.h').read_text()
definitions = header[header.index('enum fw_type {'):header.index('extern struct dram_timing_info dram_timing;')]
harness = r'''
#include <common.h>
#include <errno.h>
#include <imx8m_ddr_compact.h>
struct dram_cfg_param ddrphy_trained_csr[1];
u32 ddrphy_trained_csr_num = 1;
static struct dram_cfg_param trace[256], reference[256];
static unsigned int writes, reference_count, clocks[4], clock_count;
static int init_waits, full_waits, cdd, csr, loads, results, firmware_error;
void dwc_ddrphy_apb_wr(unsigned int reg, unsigned int val) {
 assert(writes < 256); trace[writes++] = (struct dram_cfg_param){reg,val};
}
u32 dwc_ddrphy_apb_rd(unsigned int reg) {
 assert(reg == 0x54012 || reg == 0x5402c); return reg == 0x54012 ? 0x110 : 0x100;
}
void reg32_write(unsigned long reg, unsigned int val) { (void)reg; (void)val; assert(0); }
void ddr_load_train_firmware(enum fw_type fw) { assert(fw == FW_1D_IMAGE); loads++; }
void ddrphy_init_set_dfi_clk(unsigned int rate) { assert(clock_count<4); clocks[clock_count++]=rate; }
int wait_ddrphy_training_complete(void) { full_waits++; return firmware_error; }
int wait_ddrphy_init_complete_timeout(void) { init_waits++; return firmware_error; }
void get_trained_CDD(unsigned int index) { assert(index==(unsigned int)cdd); cdd++; }
void ddrphy_init_read_msg_block(enum fw_type fw) { assert(fw==FW_1D_IMAGE); results++; }
void ddrphy_trained_csr_save(struct dram_cfg_param *p, unsigned int n) { assert(p==ddrphy_trained_csr && n==1); csr++; }
static struct dram_cfg_param phy[]={{0x123,0x44}}, pie[]={{0x90000,0x11}};
static struct dram_cfg_param fsp0[]={{0x54005,0x10}}, fsp1[]={{0x54005,0x20}}, fsp2[]={{0x54005,0x30}};
static struct dram_fsp_msg expanded_messages[] = {
 {3200,FW_1D_IMAGE,fsp0,1}, {400,FW_1D_IMAGE,fsp1,1}, {100,FW_1D_IMAGE,fsp2,1}
};
static struct dram_timing_info expanded = {
 .ddrphy_cfg=phy, .ddrphy_cfg_num=1, .ddrphy_pie=pie, .ddrphy_pie_num=1,
 .fsp_msg=expanded_messages, .fsp_msg_num=3
};
static const u8 phy_bytes[]={0x23,1,0,0x44,0}, pie_bytes[]={0,0,9,0x11,0};
static const u8 f0_bytes[]={5,0x40,5,0x10,0}, f1_bytes[]={5,0x40,5,0x20,0}, f2_bytes[]={5,0x40,5,0x30,0};
static const struct dram_compact_table tphy={phy_bytes,NULL,1,5,0,5}, tpie={pie_bytes,NULL,1,5,0,5};
static const struct dram_compact_table t0={f0_bytes,NULL,1,5,0,5}, t1={f1_bytes,NULL,1,5,0,5};
static struct dram_compact_table t2={f2_bytes,NULL,1,5,0,5};
static const struct dram_compact_fsp_msg compact_messages[] = {
 {3200,FW_1D_IMAGE,&t0}, {400,FW_1D_IMAGE,&t1}, {100,FW_1D_IMAGE,&t2}
};
static const struct dram_compact_timing compact = {
 .ddrphy_cfg=&tphy, .ddrphy_pie=&tpie, .fsp_msg=compact_messages, .fsp_msg_num=3
};
static void clear(void) {
 writes=clock_count=0;init_waits=full_waits=cdd=csr=loads=results=firmware_error=0;
}
static unsigned int occurrences(u32 reg, u32 val) {
 unsigned int count=0; for(unsigned int i=0;i<writes;i++) count+=trace[i].reg==reg && trace[i].val==val;
 return count;
}
static void save(void) {reference_count=writes;memcpy(reference,trace,writes*sizeof(*trace));}
static void same(void) {assert(writes==reference_count);assert(!memcmp(reference,trace,writes*sizeof(*trace)));}
int main(void) {
 clear();assert(ddr_cfg_phy_mrr(&expanded,2)==0);save();
 clear();assert(ddr_cfg_phy_compact(&expanded,&compact,2)==0);same();
 assert(clock_count==1 && clocks[0]==100 && loads==1 && init_waits==1 && !full_waits && !cdd && !csr);
 assert(results==1 && occurrences(0x54005,0x30)==1);
 assert(!occurrences(0x54005,0x10) && !occurrences(0x54005,0x20));
 assert(occurrences(0x54008,1)==1 && occurrences(0x54012,0x310)==1 && occurrences(0x5402c,0x103)==1);
 clear();assert(ddr_cfg_phy(&expanded)==0);save();
 clear();assert(ddr_cfg_phy_compact(&expanded,&compact,-1)==0);same();
 assert(clock_count==3 && clocks[0]==3200 && clocks[1]==400 && clocks[2]==100);
 assert(loads==3 && full_waits==3 && !init_waits && cdd==3 && csr==1 && results==3);
 assert(!occurrences(0x54008,1) && !occurrences(0x54012,0x310));
 clear();firmware_error=-EIO;assert(ddr_cfg_phy_compact(&expanded,&compact,2)==-EIO);
 assert(init_waits==1 && !csr && !cdd && !occurrences(0x90000,0x11));
 clear();t2.width=4;assert(ddr_cfg_phy_compact(&expanded,&compact,2)==-EINVAL);
 assert(!init_waits && !full_waits && !csr && !occurrences(0xd0099,9));
 puts("PASS: expanded/compact PHY traces match; selected FSP, DevInit/ranks, CSR exclusion and error paths verified");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='compact-phy-test-') as directory:
    p=Path(directory)
    for name in ('asm/arch','linux'): (p/name).mkdir(parents=True,exist_ok=True)
    (p/'common.h').write_text('''#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef unsigned long ulong;
#define IS_ENABLED(x) (x)
#define debug(...) do {} while(0)
void reg32_write(unsigned long,unsigned int);
void dwc_ddrphy_apb_wr(unsigned int,unsigned int);
u32 dwc_ddrphy_apb_rd(unsigned int);
''')
    (p/'asm/arch/ddr.h').write_text('#pragma once\n#include <common.h>\n'+definitions+'''
extern struct dram_cfg_param ddrphy_trained_csr[];
extern u32 ddrphy_trained_csr_num;
void ddr_load_train_firmware(enum fw_type);
void ddrphy_init_set_dfi_clk(unsigned int);
int wait_ddrphy_training_complete(void);
int wait_ddrphy_init_complete_timeout(void);
void get_trained_CDD(unsigned int);
void ddrphy_init_read_msg_block(enum fw_type);
void ddrphy_trained_csr_save(struct dram_cfg_param *,unsigned int);
int ddr_cfg_phy(struct dram_timing_info *);
int ddr_cfg_phy_mrr(struct dram_timing_info *,unsigned int);
''')
    for name in ('log.h','linux/kernel.h','asm/arch/sys_proto.h'): (p/name).write_text('')
    (p/'imx8m_ddr_compact.h').write_bytes((root/'include/imx8m_ddr_compact.h').read_bytes())
    (p/'test.c').write_text(harness)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-sign-compare','-fsanitize=address,undefined','-g','-DCONFIG_IMX8M_DDR_COMPACT=1','-DCONFIG_COMPULAB_LPDDR4_MRR_PROBE=1','-I'+str(p),str(root/'drivers/ddr/imx/imx8m/ddr_compact.c'),str(root/'drivers/ddr/imx/phy/ddrphy_train.c'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
