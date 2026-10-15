#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
"""Generate lossless packed timing tables from raw/ (not compiled by SPL)."""
import argparse
from collections import defaultdict
from difflib import SequenceMatcher
from pathlib import Path
import re
import struct
import sys

PAIR = re.compile(r'\{\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\}')

def read_profile(path):
    original = path.read_text()
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', original, flags=re.S)
    arrays = {}
    for name, body in re.findall(r'struct dram_cfg_param\s+(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\};', text, re.S):
        if PAIR.sub('', body).strip(' \r\n\t,'):
            raise ValueError(f'{path}: unsupported non-literal array {name}')
        if name in arrays:
            raise ValueError(f'{path}: duplicate array {name}')
        arrays[name] = tuple(tuple(int(v, 16 if v.startswith('0x') else 10) for v in p) for p in PAIR.findall(body))
    match = re.search(r'struct dram_timing_info\s+(\w+)\s*=\s*\{(.*?)\};', text, re.S)
    if not match:
        raise ValueError(f'{path}: missing timing descriptor')
    allowed = {'ddrc_cfg', 'ddrc_cfg_num', 'ddrphy_cfg', 'ddrphy_cfg_num',
               'fsp_msg', 'fsp_msg_num', 'ddrphy_trained_csr',
               'ddrphy_trained_csr_num', 'ddrphy_pie', 'ddrphy_pie_num',
               'fsp_table'}
    if set(re.findall(r'\.(\w+)\s*=', match[2])) - allowed:
        raise ValueError(f'{path}: unsupported timing descriptor fields')
    message_name = re.search(r'\.fsp_msg\s*=\s*(\w+)', match[2])
    if not message_name or not re.search(
            r'\.fsp_msg_num\s*=\s*ARRAY_SIZE\(\s*' +
            re.escape(message_name[1]) + r'\s*\)', match[2]):
        raise ValueError(f'{path}: unsupported FSP message count')
    fields = dict(re.findall(r'\.(ddrc_cfg|ddrphy_cfg|ddrphy_pie)\s*=\s*(\w+)', match[2]))
    if set(fields) != {'ddrc_cfg', 'ddrphy_cfg', 'ddrphy_pie'}:
        raise ValueError(f'{path}: incomplete descriptor')
    frequencies = re.search(r'\.fsp_table\s*=\s*\{([^}]+)\}', match[2])
    if not frequencies:
        raise ValueError(f'{path}: missing FSP frequency table')
    rates = [int(x.strip(), 0) for x in frequencies[1].split(',') if x.strip()]
    if not 1 <= len(rates) <= 4 or any(not 0 <= r <= 0xffffffff for r in rates):
        raise ValueError(f'{path}: invalid FSP table')
    messages = re.findall(r'\.drate\s*=\s*(\d+)\s*,\s*\.fw_type\s*=\s*(FW_[12]D_IMAGE)\s*,\s*\.fsp_cfg\s*=\s*(\w+)\s*,\s*\.fsp_cfg_num\s*=\s*ARRAY_SIZE\(\s*\3\s*\)', text)
    expected_messages = len(re.findall(r'\.drate\s*=', text))
    if not 1 <= len(messages) <= 4 or len(messages) != expected_messages:
        raise ValueError(f'{path}: unsupported FSP messages')
    used = set(fields.values()) | {m[2] for m in messages}
    if not used <= arrays.keys():
        raise ValueError(f'{path}: missing arrays')
    # Counts must describe the actual referenced arrays, not a stale symbol.
    for field, name in fields.items():
        if not re.search(r'\.' + field + r'_num\s*=\s*ARRAY_SIZE\(\s*' + re.escape(name) + r'\s*\)', match[2]):
            raise ValueError(f'{path}: unsupported count for {field}')
    header = original[:re.search(r'^#include', original, re.M).start()]
    return dict(path=path, header=header, symbol=match[1], arrays=arrays,
                fields=fields, messages=messages, rates=rates, used=used)

def packed(entries, width):
    result = bytearray()
    for reg, val in entries:
        if width == 6:
            if not 0 <= reg - 0x3d400000 <= 0xffff or not 0 <= val <= 0xffffffff:
                raise ValueError(f'DDRC entry cannot be packed: {reg:#x}, {val:#x}')
            result += struct.pack('<HI', reg - 0x3d400000, val)
        else:
            if not 0 <= reg <= 0xffffff or not 0 <= val <= 0xffff:
                raise ValueError(f'PHY entry cannot be packed: {reg:#x}, {val:#x}')
            result += reg.to_bytes(3, 'little') + val.to_bytes(2, 'little')
    return bytes(result)

def encode(base, target, width):
    result = bytearray()
    end = 0
    for a, b, count in SequenceMatcher(None, base, target, autojunk=False).get_matching_blocks():
        if b > end:
            result += struct.pack('<H', 0x8000 | (b - end))
            result += packed(target[end:b], width)
        if count:
            result += struct.pack('<HH', count, a)
        end = b + count
    return bytes(result)

def byte_array(name, data):
    lines = [f'static const u8 {name}[] = {{']
    for start in range(0, len(data), 12):
        lines.append('\t' + ', '.join(f'0x{x:02x}' for x in data[start:start + 12]) + ',')
    return '\n'.join(lines + ['};', ''])

def generate(source, output):
    profiles = [read_profile(p) for p in sorted(source.glob('lpddr4_timing_*.c'))]
    if not profiles:
        raise ValueError(f'No timing sources in {source}')
    symbols = [p['symbol'] for p in profiles]
    if len(set(symbols)) != len(symbols):
        raise ValueError('Duplicate exported timing descriptor')
    families = defaultdict(list)
    for profile in profiles:
        for name in sorted(profile['used']):
            data = profile['arrays'][name]
            if not data or len(data) > 0x7fff:
                raise ValueError(f'Invalid entry count for {name}')
            if data not in families[name]:
                families[name].append(data)
    license_line = '/* SPDX-License-Identifier: GPL-2.0+ AND BSD-3-Clause */\n'
    notice = '/* Generated by pack.py from raw/. Do not edit. */\n'
    copyrights = sorted({line.strip().removeprefix('*').strip()
                         for p in profiles for line in p['header'].splitlines()
                         if 'Copyright' in line})
    provenance = '/*\n' + ''.join(' * ' + line + '\n' for line in copyrights) + ' */\n'
    shared = [license_line, provenance, notice, '#include "lpddr4_compact_tables.h"\n']
    header = [license_line, provenance, notice, '#ifndef __COMPULAB_LPDDR4_COMPACT_TABLES_H\n#define __COMPULAB_LPDDR4_COMPACT_TABLES_H\n', '#include <imx8m_ddr_compact.h>\n']
    names = {}
    payload = 0
    for family, variants in sorted(families.items()):
        width = 6 if family == 'ddr_ddrc_cfg' else 5
        candidates = [(len(packed(base, width)) + sum(min(len(packed(t, width)), len(encode(base, t, width))) for t in variants if t != base), base) for base in variants]
        _, base = min(candidates, key=lambda item: item[0])
        base_name = 'compact_base_' + family
        data = packed(base, width)
        payload += len(data)
        shared.append(byte_array(base_name, data))
        for index, target in enumerate(variants):
            symbol = f'compact_{family}_{index}'
            names[family, target] = symbol
            base_ref, base_count = 'NULL', 0
            if target == base:
                data_name, data = base_name, packed(base, width)
            else:
                literal, encoded = packed(target, width), encode(base, target, width)
                if len(encoded) < len(literal):
                    data, base_ref, base_count = encoded, base_name, len(base)
                else:
                    data = literal
                data_name = symbol + '_data'
                shared.append(byte_array(data_name, data))
                payload += len(data)
            if len(data) > 0xffff:
                raise ValueError('Encoded table exceeds descriptor capacity')
            shared.append(f'const struct dram_compact_table {symbol} = {{\n\t.data = {data_name}, .base = {base_ref},\n\t.count = {len(target)}, .size = {len(data)}, .base_count = {base_count}, .width = {width},\n}};\n')
            header.append(f'extern const struct dram_compact_table {symbol};\n')
    result = {}
    for profile in profiles:
        def reference(name):
            return '&' + names[name, profile['arrays'][name]]
        lines = [profile['header'], notice, '#include "lpddr4_compact_tables.h"\n', 'static const struct dram_compact_fsp_msg messages[] = {']
        for rate, firmware, name in profile['messages']:
            lines.append(f'\t{{ {rate}, {firmware}, {reference(name)} }},')
        lines += ['};\n', f'const struct dram_compact_timing {profile["symbol"]} = {{']
        for field, name in profile['fields'].items():
            lines.append(f'\t.{field} = {reference(name)},')
        lines += ['\t.fsp_msg = messages,', f'\t.fsp_msg_num = {len(profile["messages"])},', '\t.fsp_table = { ' + ', '.join(map(str, profile['rates'])) + ' },', '};', '']
        result[output / profile['path'].name] = '\n'.join(lines)
    result[output / 'lpddr4_compact_tables.c'] = '\n'.join(shared)
    result[output / 'lpddr4_compact_tables.h'] = '\n'.join(header + ['\n#endif\n'])
    return result, payload

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    here = Path(__file__).resolve().parent
    parser.add_argument('--source-dir', type=Path, default=here / 'raw')
    parser.add_argument('--output-dir', type=Path, default=here)
    parser.add_argument('--check', action='store_true', help='Fail if generated files differ; do not write')
    args = parser.parse_args()
    try:
        files, size = generate(args.source_dir, args.output_dir)
        for path, text in files.items():
            if args.check:
                if not path.exists() or path.read_text() != text:
                    raise ValueError(f'{path}: regenerate with pack.py')
            else:
                path.write_text(text)
    except (ValueError, OSError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1
    print(f'{len(files) - 2} profiles; {size} bytes of packed payload; ' + ('generated files match' if args.check else 'generated'))
    return 0

if __name__ == '__main__':
    sys.exit(main())
