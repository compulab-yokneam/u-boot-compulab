/*
 * Copyright 2021 CompuLab
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __UCM_IMX8M_PLUS_H
#define __UCM_IMX8M_PLUS_H

#include "compulab-imx8m-plus.h"

#define CONFIG_IMX6_PWM_PER_CLK 66000000

#if defined(CONFIG_ANDROID_SUPPORT)
#include "ucm-imx8m-plus_android.h"
#endif
#endif
