/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 C-Lab
 */

#ifndef __MCM_IMX93_H
#define __MCM_IMX93_H

#include "compulab-imx93.h"

#ifdef CONFIG_SYS_PROMPT
#undef CONFIG_SYS_PROMPT
#endif

#define CONFIG_SYS_PROMPT CONFIG_SYS_BOARD"=> "

#endif
