/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 C-Lab
 */

#ifndef __IOT_LINK_H
#define __IOT_LINK_H

#define CONFIG_IOT_LINK
#include "compulab-imx93.h"
#define CONFIG_SYS_I2C_EEPROM_ADDR_SB	0x50

#ifdef CONFIG_SYS_PROMPT
#undef CONFIG_SYS_PROMPT
#endif

#define CONFIG_SYS_PROMPT CONFIG_SYS_BOARD"=> "

#endif
