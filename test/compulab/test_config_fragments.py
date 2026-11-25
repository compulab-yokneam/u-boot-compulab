#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
"""Compare sequential CompuLab config goals with merging their raw inputs."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
BASE = 'compulab-imx8mp_defconfig'
TARGETS = (
    'iot-gate-imx8plus', 'iot-gate-imx8pv2', 'iotdin-imx8p',
    'mcm-imx8m-plus', 'mcm-imx8pv2', 'som-imx8m-plus',
    'ucm-imx8m-plus-sbev', 'ucm-imx8m-plus',
)


def run(command, out, success=True):
    out.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(command, cwd=ROOT, stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=120)
    with (out / 'test.log').open('a') as log:
        log.write('$ ' + ' '.join(command) + '\n' + result.stdout)
    assert (result.returncode == 0) == success, result.stdout
    return result.stdout


def make(out, compiler, *goals, success=True):
    return run(['make', f'O={out}', f'CROSS_COMPILE={compiler}', '-j8',
                *goals], out, success)


def raw_config(out, compiler, base, *fragments):
    inputs = [str(ROOT / 'configs' / name) for name in (base, *fragments)]
    run([str(ROOT / 'scripts/kconfig/merge_config.sh'), '-m', '-O',
         str(out), *inputs], out)
    make(out, compiler, 'olddefconfig')


def settings(out):
    return {line for line in (out / '.config').read_text().splitlines()
            if re.match(r'(?:# )?CONFIG_\w+', line)}


def compare(actual, expected):
    a, b = settings(actual), settings(expected)
    assert a == b, (str(actual), sorted(a - b), sorted(b - a))


def check(work, compiler):
    for target in TARGETS:
        actual, expected = work / target, work / ('raw-' + target)
        raw_config(expected, compiler, BASE, target + '.config')
        make(actual, compiler, BASE, target + '.config')
        compare(actual, expected)
        assert '# U-Boot fragment base' not in (
            actual / 'generated_defconfig').read_text()
        print('PASS: sequential configuration matches raw merge:', target,
              flush=True)

    expected = work / 'raw-mcm-imx8pv2'
    actual = work / 'separate-invocations'
    make(actual, compiler, BASE)
    make(actual, compiler, 'mcm-imx8pv2.config')
    compare(actual, expected)

    # The original capacity/probe goal sequence must also work with -j8.
    extras = ('d0.config', 'd1.config', 'd2.config', 'd4.config',
              'd8.config', 'lpddr_mrr_probe.config')
    make(actual, compiler, BASE, 'mcm-imx8pv2.config', *extras)
    compare(actual, expected)

    expected = work / 'raw-d2d4'
    raw_config(expected, compiler, BASE, 'mcm-imx8pv2.config', 'd2d4.config')
    make(actual, compiler, BASE, 'mcm-imx8pv2.config', 'd2d4.config')
    compare(actual, expected)

    # Once a target is resolved, subsequent fragments must retain user edits.
    config = actual / '.config'
    config.write_text(re.sub(r'^CONFIG_BOOTDELAY=.*$', 'CONFIG_BOOTDELAY=7',
                             config.read_text(), flags=re.M))
    make(actual, compiler, 'd1.config')
    assert 'CONFIG_BOOTDELAY=7' in settings(actual)
    assert 'CONFIG_DRAM_D1=y' in settings(actual)

    # Reject feature fragments before selecting a target without losing input.
    make(actual, compiler, BASE)
    before = (actual / '.config').read_text()
    output = make(actual, compiler, 'd0.config', success=False)
    assert 'Apply a target .config before capacity or feature fragments' in output
    assert (actual / '.config').read_text() == before
    make(actual, compiler, 'mcm-imx8pv2.config')
    compare(actual, work / 'raw-mcm-imx8pv2')

    # Unmarked standalone defconfigs still use the existing fragment workflow.
    falcon = 'ucm-imx8m-plus_falcon_defconfig'
    expected, actual = work / 'raw-falcon', work / 'falcon'
    raw_config(expected, compiler, falcon, 'd2d4.config')
    make(actual, compiler, falcon, 'd2d4.config')
    compare(actual, expected)
    print('PASS: separate invocations, capacity/probe overrides, user edits, '
          'target-order validation and standalone Falcon configuration', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cross-compile', default=os.environ.get(
        'CROSS_COMPILE', 'aarch64-linux-gnu-'))
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='compulab-config-test-') as directory:
        check(Path(directory), args.cross_compile)
