/*
 * Copyright 2020 CompuLab
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __MCM_IMX8M_MINI_H
#define __MCM_IMX8M_MINI_H

#define FALLBACK_COMMAND "usb start; ums 0 mmc ${mmcdev};"

#include "cpl-imx8m-mini.h"

#if defined(CONFIG_ANDROID_SUPPORT)
#include "mcm-imx8m-mini_android.h"
#endif
#endif
