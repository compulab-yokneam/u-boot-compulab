#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
"""Exercise production probe functions with a simulated register interface.

Extract the OCRAM probe functions and the real polling macro to compile a
host harness. This checks software failure handling, not PHY compatibility.
Run from any directory with: python3 test/compulab/test_mrr_probe.py
Requires a host C compiler with AddressSanitizer and UndefinedBehaviorSanitizer.
"""

from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
common = (root / 'board/compulab/plat/imx8mp/ddr/ddr_common.c').read_text()
common = common[common.index('static int lpddr4_mr_read_timeout'):common.index('\n#endif')]
utils = (root / 'drivers/ddr/imx/phy/ddrphy_utils.c').read_text()
utils = utils[utils.index('static int get_pmu_message_timeout'):utils.index('\nvoid ddrphy_init_set_dfi_clk')]
board = (root / 'board/compulab/plat/imx8mp/ddr/ddr.c').read_text()
board = board[board.index('static unsigned int lpddr4_profile_rank_mask'):board.index('\n#endif')]
controller = (root / 'drivers/ddr/imx/imx8m/ddr_init.c').read_text()
controller = controller[controller.index('static int ddrc_wait'):controller.index('\nstatic int ddr_init_internal')]
poll = (root / 'include/linux/iopoll.h').read_text()
poll = poll[poll.index('#define read_poll_timeout'):poll.index('\n#define readx_poll_sleep_timeout')]
prefix = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
typedef uint32_t u32;
typedef unsigned long ulong;
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define BIT(x) (1U << (x))
#define time_after(a,b) ((long)((b)-(a)) < 0)
#define DDRC_MRSTAT(x) 1
#define DRC_PERF_MON_MRR0_DAT(x) 2
#define DRC_PERF_MON_MRR1_DAT(x) 3
#define DDRC_MRCTRL0(x) 4
#define DDRC_MRCTRL1(x) 5
#define SRC_DDRC_RCR_ADDR 6
#define DDRC_MSTR(x) 7
static ulong now;
static u32 raw_data[2][4], pmu_data[12], selected_mr, rank_value, last_capture;
static int read_number[2], fault, capture_clear, reset_count, init_calls;
static int pmu_pos, pmu_count, pmu_fault, pmu_ack, init_error, cs1_mode;
static int unstable, bad_control;
static ulong timer_get_us(void) { return now++ * 1000; }
static ulong get_timer(ulong base) { return now++ - base; }
static void udelay(unsigned int us) { now += us / 1000; }
static u32 reg32_read(ulong addr) {
 unsigned int rank = rank_value == 0x21;
 if (addr == 1) return fault == 1;
 if (addr == 2) return fault == 2 || (rank && cs1_mode == 3) ? 0 : 8;
 if (addr == 3) {
  assert(selected_mr >= 5 && selected_mr <= 8);
  read_number[rank]++;
  if (rank && cs1_mode == 5) return last_capture;
  u32 result = raw_data[rank][selected_mr - 5];
  if (rank && unstable && read_number[rank] > 4) result ^= 0x100;
  if (!rank && bad_control && read_number[1]) result ^= 0x100;
  last_capture = result;
  return result;
 }
 return 1;
}
static void reg32_write(ulong addr, u32 val) {
 if (addr == 2 && val == 4) capture_clear++;
 if (addr == 4) { assert(val == 0x11 || val == 0x21); rank_value = val; }
 if (addr == 5) selected_mr = val >> 8;
 if (addr == 6) { assert(val == 0x8f00001f); reset_count++; }
}
static void reg32setbit(ulong addr, u32 bit) { (void)addr; (void)bit; }
static u32 dwc_ddrphy_apb_rd(u32 addr) {
 if (addr == 0xd0004) {
  if (pmu_fault == 1) return 1;
  if (pmu_fault == 2) return 0;
  return pmu_ack || pmu_pos == pmu_count;
 }
 if (addr == 0xd0032) return pmu_data[pmu_pos] & 0xffff;
 if (addr == 0xd0034) return pmu_data[pmu_pos] >> 16;
 assert(0); return 0;
}
static void dwc_ddrphy_apb_wr(u32 addr, u32 val) {
 assert(addr == 0xd0031);
 if (!val) pmu_ack = 1;
 else { pmu_ack = 0; pmu_pos++; }
}
enum fw_type { FW_1D_IMAGE, FW_2D_IMAGE };
struct dram_cfg_param { u32 reg, val; };
struct dram_fsp_msg { unsigned int drate; enum fw_type fw_type; };
struct dram_timing_info {
 struct dram_fsp_msg *fsp_msg;
 unsigned int fsp_msg_num;
 struct dram_cfg_param *ddrc_cfg;
 unsigned int ddrc_cfg_num;
};
static struct dram_fsp_msg messages[] = {{3200, FW_1D_IMAGE}, {400, FW_1D_IMAGE}, {100, FW_1D_IMAGE}};
static struct dram_cfg_param single_rank[] = {{DDRC_MSTR(0), 0xa1080020}};
static struct dram_cfg_param dual_rank[] = {{DDRC_MSTR(0), 0xa3080020}};
static struct dram_timing_info ucm_dram_timing_01061010_2G = {messages, 3, single_rank, 1};
static struct dram_timing_info dual_timing = {messages, 3, dual_rank, 1};
static struct { u32 id; struct dram_timing_info *timing; } lpddr4_array[3];
static int ddr_init_mrr(struct dram_timing_info *seed, unsigned int fsp) {
 assert(seed == &ucm_dram_timing_01061010_2G); assert(fsp == 2); init_calls++; return init_error;
}
static void set_id(unsigned int rank, u32 id) {
 for (int mr=0;mr<4;mr++) raw_data[rank][mr] = ((id >> (24-8*mr)) & 0xff) * 0x00010100;
}
static void setup(u32 id) {
 now=0; read_number[0]=0; read_number[1]=0; fault=0; capture_clear=0;
 reset_count=0; init_calls=0; init_error=0; rank_value=0x11; last_capture=0;
 pmu_pos=0; pmu_count=0; pmu_fault=0; pmu_ack=0; cs1_mode=0; unstable=0; bad_control=0;
 lpddr4_array[0].id=id; lpddr4_array[0].timing=&dual_timing;
 lpddr4_array[1].id=id; lpddr4_array[1].timing=&ucm_dram_timing_01061010_2G;
 lpddr4_array[2].id=0x05000010; lpddr4_array[2].timing=&ucm_dram_timing_01061010_2G;
 set_id(0,id); set_id(1,id);
}
'''
main = r'''
int main(void) {
 u32 id=0x12345678, ranks=0;
 setup(0xff070018); assert(lpddr4_get_mr_probe(&id,&ranks)==0 && id==0xff070018 && ranks==3);
 assert(read_number[0]==24 && read_number[1]==12 && capture_clear==36);
 setup(0xff070018); set_id(1,0); assert(lpddr4_get_mr_probe(&id,&ranks)==0 && ranks==1);
 setup(0xff070018); set_id(1,0xffffffff); assert(lpddr4_get_mr_probe(&id,&ranks)==0 && ranks==1);
 setup(0xff070018); unstable=1; assert(lpddr4_get_mr_probe(&id,&ranks)==-EIO);
 setup(0); assert(lpddr4_get_mr_probe(&id,&ranks)==-EIO);
 setup(0xffffffff); assert(lpddr4_get_mr_probe(&id,&ranks)==-EIO);
 setup(0xff070018); fault=1; assert(lpddr4_get_mr_probe(&id,&ranks)==-ETIMEDOUT && capture_clear==1);
 setup(0xff070018); fault=2; assert(lpddr4_get_mr_probe(&id,&ranks)==-ETIMEDOUT && capture_clear==1);
 setup(0xff070018); cs1_mode=3; assert(lpddr4_get_mr_probe(&id,&ranks)==-ETIMEDOUT);
 setup(0xff070018); cs1_mode=5; assert(lpddr4_get_mr_probe(&id,&ranks)==-EIO);
 setup(0xff070018); bad_control=1; assert(lpddr4_get_mr_probe(&id,&ranks)==-EIO);
 setup(0xff070018); set_id(1,0xff060018); assert(lpddr4_get_mr_probe(&id,&ranks)==-EIO);
 setup(0xff070018); raw_data[1][0]&=0xff00; assert(lpddr4_get_mr_probe(&id,&ranks)==-EIO);
 setup(0xff070018); raw_data[1][1]|=0xff; assert(lpddr4_get_mr_probe(&id,&ranks)==-EIO);
 setup(0xff070018); assert(lpddr4_probe_profile()==0 && reset_count==1 && init_calls==1);
 setup(0xff070018); set_id(1,0); assert(lpddr4_probe_profile()==1 && reset_count==1);
 setup(0xff070018); set_id(1,0xffffffff); assert(lpddr4_probe_profile()==1 && reset_count==1);
 setup(0xff070018); lpddr4_array[1].timing=&dual_timing; assert(lpddr4_probe_profile()==-EINVAL && reset_count==1);
 setup(0xff070018); lpddr4_array[0].id=0xff060018; assert(lpddr4_probe_profile()==-1 && reset_count==1);
 setup(0xff070018); init_error=-ETIMEDOUT; assert(lpddr4_probe_profile()==-ETIMEDOUT && reset_count==1 && read_number[0]==0);
 setup(0xff070018); cs1_mode=3; assert(lpddr4_probe_profile()==-ETIMEDOUT && reset_count==1);
 setup(0xff070018); cs1_mode=5; assert(lpddr4_probe_profile()==-EIO && reset_count==1);
 setup(0xff070018); assert(ddrc_wait(9,1,0,true)==-ETIMEDOUT);
 setup(0xff070018); assert(ddrc_wait(9,1,1,true)==0);
 setup(0); pmu_data[0]=7; pmu_count=1; assert(wait_ddrphy_init_complete_timeout()==0);
 setup(0); pmu_data[0]=0xff; pmu_count=1; assert(wait_ddrphy_init_complete_timeout()==-EIO);
 setup(0); pmu_fault=1; assert(wait_ddrphy_init_complete_timeout()==-ETIMEDOUT);
 setup(0); pmu_fault=2; pmu_data[0]=7; pmu_count=1; assert(wait_ddrphy_init_complete_timeout()==-ETIMEDOUT);
 setup(0); pmu_data[0]=8; pmu_data[1]=0x12340002; pmu_data[2]=0xdeadbeef; pmu_data[3]=0x12345678; pmu_data[4]=7; pmu_count=5; assert(wait_ddrphy_init_complete_timeout()==0 && pmu_pos==5);
 setup(0); pmu_data[0]=8; pmu_data[1]=0x1234ffff; pmu_count=2; assert(wait_ddrphy_init_complete_timeout()==-ETIMEDOUT);
 puts("PASS: 30 mocked rank-probe, timeout, firmware and profile-selection scenarios");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='compulab-mrr-test-') as directory:
 path=Path(directory)
 (path/'test.c').write_text(prefix+poll+common+utils+board+controller+main)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(path/'test.c'),'-o',str(path/'test')],check=True)
 subprocess.run([str(path/'test')],check=True)
