# 2026.04 build compatibility review

Review date: 2026-10-15. Source baseline: 830d1eddaba.
Reference workflow: u-boot-compulab_v2023.04 README.md at 4c9fce9e0b5.

## Scope

This review checks compatibility of the documented build interface and records
known hardware evidence and patch-style findings. It is not certification,
a license audit of bundled firmware, a security audit, or proof of complete
functional parity with 2023.04.

## Workflow compatibility

The four supported machine defconfigs retain their 2023.04 names.
The make targets flash.bin and u-boot-initial-env, O= output directory,
CROSS_COMPILE prefix and fragment filenames remain available.
DRAM symbols were renamed internally to IMX8MP_DRAM_* to avoid collisions
with other platforms; callers continue to use d2.config, d4.config and
d1d8.config. A bare defconfig defaults to D2.

The README restores the 2023.04 instructions with the experimental branch
name, explicit per-machine/per-RAM output directories, current dependency
guidance and the supported board list. IOT-GATE/IOT-DIN are not covered.
It also documents RAM-only UUU testing, independently of eMMC programming.

## Verification

* Fresh CL-SOM D4 build with GCC 15.2.0: flash.bin and
  u-boot-initial-env generated successfully, without warnings or errors.
* All four documented machines with D2, D4 and D1D8, each combined with
  lab_temp.config and spl_size.config: all 12 configuration commands pass.
  The resulting RAM selection, lab-temperature and SPL-size symbols were
  checked. These are configuration checks, not 12 new complete builds.
* flash.bin-with-env and flash.bin-with-custom-env: both pass and produce
  4 MiB files. The custom-env check uses a copy of the initial text environment.
* README relative links resolve; documentation whitespace checks pass.

Logs on the development machine, under
/mnt/devel/Kostya/u-boot/ucm-imx8plus:
doc-build-check.log, doc-env-check.log, config-doc-check-*.log,
and port-checkpatch.log.

## Patch-style findings

git diff --check passes for the complete port relative to
origin/u-boot-compulab_v2026.04.

Running scripts/checkpatch.pl --no-tree on the combined MCM/CL-SOM changes
86a40f86eca..830d1eddaba reports 33 errors, 110 warnings and 5 checks
over 3361 checked lines. This is a scoped review, not a checkpatch pass for
the full port. Errors include indentation, brace placement and spacing;
warnings include long lines in imported device trees and maintainership
coverage. Existing formatting is retained rather than mixing a broad source
cleanup into the documentation update. Full style compliance is outstanding.

## Hardware evidence and limits

* UCM: DHCP and Startec LVDS confirmed by the user during port validation.
* MCM: Samsung 4 GB boot observed; user reports validation complete.
  A detailed final test matrix was not recorded.
* CL-SOM: ISSI 4 GB at 3200 MT/s and Startec LVDS 1024x600 validated
  with image 830d1eddaba. User confirmed clear text, correct colors and
  no flicker. This was a USB RAM boot, not an eMMC installation.

Build success does not qualify all RAM vendors, carriers, Linux overlays,
environment persistence, long-duration DDR stability or secure boot.
