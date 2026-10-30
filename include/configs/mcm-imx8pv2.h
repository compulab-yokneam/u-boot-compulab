/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright 2026 CompuLab */

#ifndef __MCM_IMX8PV2_H
#define __MCM_IMX8PV2_H

#include "compulab-imx8m-plus.h"

#undef CONFIG_MMCROOT
#define CONFIG_MMCROOT "/dev/mmcblk2p2"

#if defined(CONFIG_CMD_NET)
#define FEC_QUIRK_ENET_MAC
#define PHY_ANEG_TIMEOUT 20000
#endif

#endif
