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

#ifdef CONFIG_COMPULAB_DEBUG_UART1
#undef CFG_MXC_UART_BASE
#define CFG_MXC_UART_BASE UART1_BASE_ADDR
#endif


#ifdef CONFIG_COMPULAB_DEBUG_UART4
#undef CFG_MXC_UART_BASE
#define CFG_MXC_UART_BASE UART4_BASE_ADDR
#endif

#if defined(CONFIG_ANDROID_SUPPORT)
#include "ucm-imx8m-plus_android.h"
#endif
#endif

#ifdef CONFIG_SBEV_UCMIMX8PLUS
#ifdef CFG_FEC_MXC_PHYADDR
#undef CFG_FEC_MXC_PHYADDR
#define CFG_FEC_MXC_PHYADDR 4
#endif
#endif
