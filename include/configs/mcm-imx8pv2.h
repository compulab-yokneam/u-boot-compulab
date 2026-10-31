/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright 2026 CompuLab */

#ifndef __MCM_IMX8PV2_H
#define __MCM_IMX8PV2_H

/* SBC-MCM8PV2 video link 1 is LCDIF2/LVDS. */
#define COMPULAB_DEFAULT_VIDEO_LINK "1"

#ifdef CONFIG_VIDEO_IMX8MP_HDMI
#define MACHINE_EXTRA_ENV_SETTINGS "hdmi_mode=auto\0"
#endif

#include "compulab-imx8m-plus.h"

#undef CONFIG_MMCROOT
#define CONFIG_MMCROOT "/dev/mmcblk2p2"

#if defined(CONFIG_CMD_NET)
#define FEC_QUIRK_ENET_MAC
#define PHY_ANEG_TIMEOUT 20000
#endif

#endif
