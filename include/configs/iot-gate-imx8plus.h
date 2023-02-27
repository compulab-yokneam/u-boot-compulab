/*
 * Copyright 2021 CompuLab
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __IOT_GATE_IMX8PLUS_H
#define __IOT_GATE_IMX8PLUS_H

#include "compulab-imx8m-plus.h"

#define CONFIG_IMX6_PWM_PER_CLK 66000000

#if defined(CONFIG_CMD_NET)
#define CONFIG_ETHPRIME                 "eth0" /* Set eqos to primary since we use its MDIO */

#define CONFIG_FEC_XCV_TYPE             RGMII
#define CONFIG_FEC_MXC_PHYADDR         	-1
#define FEC_QUIRK_ENET_MAC

#ifdef CONFIG_DWC_ETH_QOS
#define CONFIG_SYS_NONCACHED_MEMORY     (1 * SZ_1M)     /* 1M */
#define DWC_NET_PHYADDR	               	-1
#endif

#define PHY_ANEG_TIMEOUT 20000

#endif

#endif
