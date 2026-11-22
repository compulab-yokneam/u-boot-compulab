#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
"""Check production SPL discovery and EEPROM writes across simulated resets."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
board = (root / 'board/compulab/plat/imx8mp/ddr/ddr.c').read_text()
board = board[board.index('#define DEFAULT'):board.index('static inline void spl_dram_share_info')]
eeprom = (root / 'board/compulab/plat/imx8/eeprom/eeprom_spl.c').read_text()
write = eeprom[eeprom.index('static int cl_eeprom_write'):eeprom.index('/* Reserved')]
getters = eeprom[eeprom.index('/* Reserved'):eeprom.index('\n#ifdef CONFIG_SPL_REPORT_FAKE_MEMSIZE\n/* override-size')]
header = (root / 'board/compulab/plat/imx8mp/ddr/ddr.h').read_text()
structs = header[header.index('struct lpddr4_tcm_desc {'):header.index('#ifdef CONFIG_SPL_OS_BOOT')]
compact = (root / 'include/imx8m_ddr_compact.h').read_text()
compact = re.search(r'struct dram_compact_timing \{.*?\};', compact, re.S)[0]
prefix = r'''
#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t u32; typedef uint8_t u8; typedef unsigned int uint; typedef unsigned char uchar;
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
struct dram_compact_table; struct dram_compact_fsp_msg;
static unsigned char device[256];
static unsigned long now, ready;
static int fail_offset, protect_offset, init_result, selected, hang_training;
static u32 mrr_id;
static jmp_buf reset;
static void *g_dev = (void *)1;
static void udelay(unsigned int us) { now += us; }
static void cl_eeprom_we(int enable) { assert(enable); }
static int cpl_eeprom_init(void) { return 0; }
static int dm_i2c_write(void *dev, uint off, const uchar *buf, int len) {
 assert(dev == g_dev && off + len <= sizeof(device));
 if (now < ready || (int)off == fail_offset) return -EIO;
 if ((int)off != protect_offset) memcpy(device + off, buf, len);
 ready = now + 10000;
 return 0;
}
static int cl_eeprom_read(uint off, uchar *buf, int len) {
 if (now < ready) return -EIO;
 assert(off + len <= sizeof(device)); memcpy(buf, device + off, len); return 0;
}
'''
mock = r'''
int cl_eeprom_buffer_write(uint off, uchar *buf, int len) { return cl_eeprom_write(off, buf, len); }
int cl_eeprom_buffer_read(uint off, uchar *buf, int len) { return cl_eeprom_read(off, buf, len); }
static struct dram_compact_timing timings[3] = {
 {.fsp_table={4000}}, {.fsp_table={3200}}, {.fsp_table={3200}}
};
static const struct lpddr4_desc lpddr4_array[] = {
 {.name="first", .id=0xff060018, .size=8192, .subind=8, .timing=&timings[0]},
 {.name="second", .id=0xff070018, .size=8192, .subind=8, .timing=&timings[1]},
 {.name="third", .id=0x13000518, .size=4096, .subind=4, .timing=&timings[2]},
};
static int ddr_init_compact(const struct dram_compact_timing *timing) {
 selected = timing - timings;
 if (hang_training) longjmp(reset, 1);
 return init_result;
}
static u32 lpddr4_get_mr(void) { return mrr_id; }
'''
main = r'''
static void fresh(void) {
 memset(device, 0xff, sizeof(device)); now=ready=0;
 fail_offset=protect_offset=-1; init_result=0; hang_training=0;
 selected=-1; mrr_id=0xff070018; board_ddrinfo=0xdeadbeef;
 memset(&spl_tcm_data, 0, sizeof(spl_tcm_data));
 lpddr4_data_get(&spl_tcm_data);
}
static void reboot(void) {
 memset(&spl_tcm_data, 0, sizeof(spl_tcm_data));
 lpddr4_data_get(&spl_tcm_data);
}
int main(void) {
 /* Write completion covers immediate accesses, and readback is not cached. */
 fresh(); assert(cl_eeprom_get_ddrinfo()==0xffffffff);
 assert(cl_eeprom_set_ddrinfo(0xff070018)==0xff070018);
 assert(now>=ready && cl_eeprom_get_ddrinfo()==0xff070018);
 u32 changed=0x13000518; memcpy(device+0x40, &changed, 4);
 assert(cl_eeprom_get_ddrinfo()==changed);
 /* Reproduce the capture: first mismatches, second succeeds, next boot uses second. */
 fresh(); assert(_spl_dram_init()==1 && selected==0 && spl_tcm_data.index==1);
 reboot(); assert(_spl_dram_init()==1 && selected==1 && spl_tcm_data.sign==VALID);
 lpddr4_data_set(&spl_tcm_data); reboot();
 assert(_spl_dram_init()==0 && selected==1 && spl_tcm_data.size==8192);
 /* Real ID write failure must not mark VALID or request another scan reset. */
 fresh(); assert(_spl_dram_init()==1); reboot(); fail_offset=0x40;
 assert(_spl_dram_init()==-EIO && selected==1 && spl_tcm_data.sign==DEFAULT);
 /* Write protection may ACK the bus: physical readback still rejects it. */
 fresh(); assert(_spl_dram_init()==1); reboot(); protect_offset=0x40;
 assert(_spl_dram_init()==-EIO && spl_tcm_data.sign==DEFAULT);
 /* Subindex write failures also cannot finalize identification. */
 fresh(); assert(_spl_dram_init()==1); reboot(); fail_offset=0x44;
 assert(_spl_dram_init()==-EIO && spl_tcm_data.sign==DEFAULT);
 /* A reset in training uses the pre-advanced index exactly once. */
 fresh(); hang_training=1;
 if (!setjmp(reset)) { _spl_dram_init(); assert(0); }
 reboot(); assert(spl_tcm_data.index==1 && spl_tcm_data.ddr_init_status==DDR_INIT_IN);
 hang_training=0; assert(_spl_dram_init()==1 && selected==1);
 /* Failed training advances through every entry, then exhausts instead of looping. */
 fresh(); init_result=-EIO;
 for (int i=0; i<3; i++) { assert(_spl_dram_init()==1 && selected==i); reboot(); }
 assert(_spl_dram_init()==-1);
 puts("PASS: EEPROM write completion, physical readback, discovery commit and reset progression");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='ddr-discovery-test-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(prefix + compact + structs + write + getters + mock + board + main)
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-Wno-sign-compare', '-fsanitize=address,undefined', '-g', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
