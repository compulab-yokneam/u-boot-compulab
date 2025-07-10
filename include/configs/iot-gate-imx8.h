/*
 * Copyright 2020 CompuLab
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __IOT_GATE_IMX8_H
#define __IOT_GATE_IMX8_H

#define BOOT_CANDIDATE_LIST usb_ul emmc_ul

#ifdef CONFIG_FASTBOOT_DEBUG
#define FALLBACK_COMMAND "echo \"Run fastboot ...\"; fastboot 0; "
#else 
#define FALLBACK_COMMAND "echo \"No boot candidate found\";"
#endif

#include "cpl-imx8m-mini.h"

#if defined(CONFIG_ANDROID_SUPPORT)
#include "ucm-imx8m-mini_android.h"
#endif
#endif
