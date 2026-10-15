#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
"""Compile the production decoder and all profiles; verify writes and saved ABI.

Requires cc with ASan/UBSan. Optionally compare original AArch64 SPL tables:
  test_ddr_compact.py --reference-elf /path/to/original/spl/u-boot-spl
Requires pyelftools only for the optional ELF comparison.
"""
import argparse
import importlib.util
from pathlib import Path
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
DDR = ROOT / 'board/compulab/plat/imx8mp/ddr'
spec = importlib.util.spec_from_file_location('pack', DDR / 'pack.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)

def pairs(entries):
    return b''.join(struct.pack('<II', *p) for p in entries)

def reference_elf(path, profiles):
    from elftools.elf.elffile import ELFFile
    by_symbol = {p['symbol']: p for p in profiles}
    checked = 0
    with open(path, 'rb') as stream:
        elf = ELFFile(stream)
        assert elf.elfclass == 64 and elf.little_endian and elf['e_machine'] == 'EM_AARCH64'
        def read(addr, size):
            for section in elf.iter_sections():
                offset = addr - section['sh_addr']
                if 0 <= offset and offset + size <= section['sh_size']:
                    return section.data()[offset:offset + size]
            raise AssertionError(f'Unmapped ELF address {addr:#x}')
        def table(desc, offset):
            addr, count = struct.unpack_from('<QI', desc, offset)
            return read(addr, count * 8)
        for symbol in elf.get_section_by_name('.symtab').iter_symbols():
            if symbol.name not in by_symbol:
                continue
            p = by_symbol[symbol.name]
            assert symbol['st_size'] == 96
            desc = read(symbol['st_value'], 96)
            for name, offset in [('ddrc_cfg', 0), ('ddrphy_cfg', 16), ('ddrphy_pie', 64)]:
                assert table(desc, offset) == pairs(p['arrays'][p['fields'][name]]), (symbol.name, name)
            addr, count = struct.unpack_from('<QI', desc, 32)
            assert count == len(p['messages'])
            for index, (rate, fw, name) in enumerate(p['messages']):
                msg = read(addr + index * 24, 24)
                r, f, cfg, n = struct.unpack('<IIQI4x', msg)
                assert (r, f) == (int(rate), int(fw == 'FW_2D_IMAGE'))
                assert read(cfg, n * 8) == pairs(p['arrays'][name])
            rates = p['rates'] + [0] * (4 - len(p['rates']))
            assert desc[76:92] == struct.pack('<4I', *rates), (symbol.name, desc[76:92].hex(), rates)
            checked += 1
    assert checked, 'No matching profiles found in reference ELF'
    print(f'PASS: {checked} linked original ELF profiles match raw inputs')

def compact_elf(path, profiles):
    from elftools.elf.elffile import ELFFile
    by_symbol = {p['symbol']: p for p in profiles}
    checked = 0
    with open(path, 'rb') as stream:
        elf = ELFFile(stream)
        assert elf.elfclass == 64 and elf.little_endian and elf['e_machine'] == 'EM_AARCH64'
        def read(addr, size):
            for section in elf.iter_sections():
                offset = addr - section['sh_addr']
                if 0 <= offset and offset + size <= section['sh_size']:
                    return section.data()[offset:offset + size]
            raise AssertionError(f'Unmapped ELF address {addr:#x}')
        def unpack_entries(data, width):
            entries = []
            for i in range(0, len(data), width):
                if width == 6:
                    r, v = struct.unpack_from('<HI', data, i)
                    entries.append((r + 0x3d400000, v))
                else:
                    entries.append((int.from_bytes(data[i:i+3], 'little'), int.from_bytes(data[i+3:i+5], 'little')))
            return entries
        def table(addr):
            data, base, count, size, nbase, width = struct.unpack('<QQHHHBx', read(addr, 24))
            assert width in (5, 6)
            blob = read(data, size)
            if not base:
                result = unpack_entries(blob, width)
            else:
                original = unpack_entries(read(base, nbase * width), width)
                result = []
                pos = 0
                while pos < size:
                    command = int.from_bytes(blob[pos:pos+2], 'little'); pos += 2
                    length = command & 0x7fff
                    assert length
                    if command & 0x8000:
                        result.extend(unpack_entries(blob[pos:pos+length*width], width))
                        pos += length * width
                    else:
                        offset = int.from_bytes(blob[pos:pos+2], 'little'); pos += 2
                        assert offset + length <= nbase
                        result.extend(original[offset:offset+length])
                assert pos == size
            assert len(result) == count
            return tuple(result)
        for symbol in elf.get_section_by_name('.symtab').iter_symbols():
            if symbol.name not in by_symbol:
                continue
            p = by_symbol[symbol.name]
            assert symbol['st_size'] == 56
            ctrl, phy, pie, msgs, count, *rates = struct.unpack('<QQQQI4I4x', read(symbol['st_value'], 56))
            for address, field in [(ctrl, 'ddrc_cfg'), (phy, 'ddrphy_cfg'), (pie, 'ddrphy_pie')]:
                assert table(address) == p['arrays'][p['fields'][field]], (symbol.name, field)
            assert rates == p['rates'] + [0] * (4 - len(p['rates']))
            assert count == len(p['messages'])
            for i, (rate, fw, name) in enumerate(p['messages']):
                r, f, config = struct.unpack('<IIQ', read(msgs + i * 16, 16))
                assert (r, f) == (int(rate), int(fw == 'FW_2D_IMAGE'))
                assert table(config) == p['arrays'][name], (symbol.name, name)
            checked += 1
    assert checked, 'No compact profiles linked'
    print(f'PASS: {checked} linked compact ELF profiles match raw inputs')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-elf', type=Path)
    parser.add_argument('--compact-elf', type=Path, action='append', default=[])
    args = parser.parse_args()
    subprocess.run(['python3', str(DDR / 'pack.py'), '--check'], check=True)
    profiles = [pack.read_profile(p) for p in sorted((DDR / 'raw').glob('lpddr4_timing_*.c'))]
    if args.reference_elf:
        reference_elf(args.reference_elf, profiles)
    for elf in args.compact_elf:
        compact_elf(elf, profiles)
    ddr_header = (ROOT / 'arch/arm/include/asm/arch-imx8m/ddr.h').read_text()
    definitions = ddr_header[ddr_header.index('enum fw_type {'):ddr_header.index('extern struct dram_timing_info dram_timing;')]
    csr_source = (ROOT / 'drivers/ddr/imx/phy/ddrphy_csr.c').read_text()
    body = re.search(r'ddrphy_trained_csr\[\]\s*=\s*\{(.*?)\};', csr_source, re.S)[1]
    csr = [(int(a, 0), 0x9000 + i) for i, (a, _) in enumerate(pack.PAIR.findall(body))]
    expected = bytearray()
    declarations, calls = [], []
    for p in profiles:
        declarations.append('extern const struct dram_compact_timing ' + p['symbol'] + ';')
        calls.append(' check(&' + p['symbol'] + ');')
        expected += struct.pack('<4I', *(p['rates'] + [0] * (4 - len(p['rates']))))
        for field in ('ddrc_cfg', 'ddrphy_cfg'):
            expected += pairs(p['arrays'][p['fields'][field]])
        for rate, fw, name in p['messages']:
            expected += struct.pack('<II', int(rate), int(fw == 'FW_2D_IMAGE'))
            expected += pairs(p['arrays'][name])
        expected += pairs(p['arrays'][p['fields']['ddrphy_pie']])
        # The saved timing ABI contains ordinary DDRC, PHY, driver CSR, PIE.
        for field in ('ddrc_cfg', 'ddrphy_cfg'):
            expected += pairs(p['arrays'][p['fields'][field]])
        expected += pairs(csr)
        expected += pairs(p['arrays'][p['fields']['ddrphy_pie']])
    harness = r'''
#include <common.h>
#include <errno.h>
#include <imx8m_ddr_compact.h>
static bool phy_target;
void reg32_write(unsigned long reg, unsigned int val) {
 u32 pair[2] = {reg, val}; assert(!phy_target); fwrite(pair, sizeof(pair), 1, stdout);
}
void dwc_ddrphy_apb_wr(unsigned int reg, unsigned int val) {
 u32 pair[2] = {reg, val}; assert(phy_target); fwrite(pair, sizeof(pair), 1, stdout);
}
static void emit(const struct dram_compact_table *table, bool phy) {
 phy_target = phy; assert(ddr_compact_write(table, phy) == 0);
}
static void check(const struct dram_compact_timing *t) {
 union { max_align_t align; u8 bytes[32768]; } buffer;
 struct dram_timing_info *saved = (void *)buffer.bytes;
 memset(&buffer, 0xa5, sizeof(buffer));
 assert(sizeof(*saved) == 96);
 fwrite(t->fsp_table, sizeof(t->fsp_table), 1, stdout);
 emit(t->ddrc_cfg, false); emit(t->ddrphy_cfg, true);
 for (unsigned int i=0; i<t->fsp_msg_num; i++) {
  u32 msg[2] = {t->fsp_msg[i].drate, t->fsp_msg[i].fw_type};
  fwrite(msg, sizeof(msg), 1, stdout); emit(t->fsp_msg[i].cfg, true);
 }
 emit(t->ddrphy_pie, true);
 assert(ddr_compact_save(t, (ulong)&buffer) == 0);
 assert(!saved->fsp_msg && !saved->fsp_msg_num);
 assert(!memcmp(saved->fsp_table, t->fsp_table, sizeof(t->fsp_table)));
 assert(saved->ddrc_cfg_num == t->ddrc_cfg->count);
 assert(saved->ddrphy_cfg_num == t->ddrphy_cfg->count);
 assert(saved->ddrphy_pie_num == t->ddrphy_pie->count);
 assert(saved->ddrphy_trained_csr_num == ddrphy_trained_csr_num);
 assert((void *)saved->ddrc_cfg == saved+1);
 assert(saved->ddrphy_cfg == saved->ddrc_cfg + saved->ddrc_cfg_num);
 assert(saved->ddrphy_trained_csr == saved->ddrphy_cfg + saved->ddrphy_cfg_num);
 assert(saved->ddrphy_pie == saved->ddrphy_trained_csr + ddrphy_trained_csr_num);
 fwrite(saved->ddrc_cfg, 8, saved->ddrc_cfg_num, stdout);
 fwrite(saved->ddrphy_cfg, 8, saved->ddrphy_cfg_num, stdout);
 fwrite(saved->ddrphy_trained_csr, 8, saved->ddrphy_trained_csr_num, stdout);
 fwrite(saved->ddrphy_pie, 8, saved->ddrphy_pie_num, stdout);
 unsigned int used=sizeof(*saved)+8*(saved->ddrc_cfg_num+saved->ddrphy_cfg_num+saved->ddrphy_pie_num+ddrphy_trained_csr_num);
 for (unsigned int i=used; i<sizeof(buffer); i++) assert(buffer.bytes[i]==0xa5);
}
static void reject(const u8 *data, u16 size, u16 count, const u8 *base, u16 nbase, u8 width) {
 struct dram_compact_table t = {data, base, count, size, nbase, width};
 struct dram_compact_iter it; struct dram_cfg_param entry; int result;
 ddr_compact_iter_init(&it, &t);
 do {result = ddr_compact_next(&it, &entry);} while (result > 0);
 assert(result == -EINVAL);
}
static void malformed(void) {
 const u8 base[5] = {0,0,0,0,0};
 const u8 short_command[] = {1};
 const u8 zero_run[] = {0,0};
 const u8 bad_copy[] = {1,0,1,0};
 const u8 short_literal[] = {1,0x80,0};
 const u8 too_many[] = {2,0,0,0};
 const u8 extra[] = {1,0,0,0,0};
 reject(base, 4, 1, NULL, 0, 5);
 reject(base, 5, 1, NULL, 0, 4);
 reject(short_command, 1, 1, base, 1, 5);
 reject(zero_run, 2, 1, base, 1, 5);
 reject(bad_copy, 4, 1, base, 1, 5);
 reject(short_literal, 3, 1, base, 1, 5);
 reject(too_many, 4, 1, base, 1, 5);
 reject(extra, 5, 1, base, 1, 5);
}
'''
    with tempfile.TemporaryDirectory(prefix='ddr-compact-test-') as directory:
        temp = Path(directory)
        (temp / 'asm/arch').mkdir(parents=True)
        (temp / 'imx8m_ddr_compact.h').write_bytes((ROOT / 'include/imx8m_ddr_compact.h').read_bytes())
        (temp / 'common.h').write_text('''#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef unsigned long ulong;
void reg32_write(unsigned long, unsigned int);
void dwc_ddrphy_apb_wr(unsigned int, unsigned int);
''')
        (temp / 'asm/arch/ddr.h').write_text('#pragma once\n#include <common.h>\n' + definitions + '\nextern struct dram_cfg_param ddrphy_trained_csr[];\nextern u32 ddrphy_trained_csr_num;\n')
        harness += '\nstruct dram_cfg_param ddrphy_trained_csr[] = {' + ','.join('{%d,%d}' % pair for pair in csr) + '};\nu32 ddrphy_trained_csr_num = %d;\n' % len(csr)
        harness += '\n'.join(declarations) + '\nint main(void) { malformed();\n' + '\n'.join(calls) + '\nreturn 0; }\n'
        (temp / 'test.c').write_text(harness)
        sources = [ROOT / 'drivers/ddr/imx/imx8m/ddr_compact.c', DDR / 'lpddr4_compact_tables.c', *[DDR / p['path'].name for p in profiles], temp / 'test.c']
        command = ['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-g', '-I'+str(temp), *map(str, sources), '-o', str(temp / 'test')]
        subprocess.run(command, check=True)
        actual = subprocess.check_output([str(temp / 'test')])
        assert actual == expected, 'Register trace or saved timing ABI differs from original input'
    print(f'PASS: all {len(profiles)} profiles, exact register traces, saved timing ABI and malformed-stream rejection (ASan/UBSan)')

if __name__ == '__main__':
    main()
