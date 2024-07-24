/*
 * Copyright 2023 CompuLab
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __UCM_IMX8M_PLUS_H
#define __UCM_IMX8M_PLUS_H

#include "compulab-imx8m-plus.h"

#if defined(CONFIG_CMD_NET)
#define CFG_FEC_MXC_PHYADDR         	-1
#define FEC_QUIRK_ENET_MAC

#ifdef CONFIG_DWC_ETH_QOS
#define DWC_NET_PHYADDR	               	-1
#endif

#define PHY_ANEG_TIMEOUT 20000

#endif

#if defined(CONFIG_ANDROID_SUPPORT)
#include "ucm-imx8m-plus_android.h"
#endif
#endif
