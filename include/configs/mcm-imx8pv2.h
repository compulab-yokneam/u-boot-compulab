/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright 2026 CompuLab */

#ifndef __MCM_IMX8PV2_H
#define __MCM_IMX8PV2_H

#ifdef CONFIG_VIDEO_IMX8MP_HDMI
/* LCDIF1/DSI, LCDIF2/LVDS, then LCDIF3/HDMI in video-link order. */
#define COMPULAB_DEFAULT_VIDEO_LINK "2"
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
