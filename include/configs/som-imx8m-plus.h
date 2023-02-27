/*
 * Copyright 2021 CompuLab
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __SOM_IMX8M_PLUS_H
#define __SOM_IMX8M_PLUS_H

#include "compulab-imx8m-plus.h"

#define CONFIG_IMX6_PWM_PER_CLK 66000000

#if defined(CONFIG_CMD_NET)
#define CONFIG_ETHPRIME                 "eth0"

#define CONFIG_FEC_XCV_TYPE             RGMII
#define CONFIG_FEC_MXC_PHYADDR         	-1
#define FEC_QUIRK_ENET_MAC

#define DWC_NET_PHYADDR	               	-1
#define PHY_ANEG_TIMEOUT 20000

#endif

#if defined(CONFIG_ANDROID_SUPPORT)
#include "som-imx8m-plus_android.h"
#endif
#endif
