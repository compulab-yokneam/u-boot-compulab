/* SPDX-License-Identifier: GPL-2.0+ */
#ifdef CONFIG_TARGET_COMPULAB_IMX8MP
#include "eeprom_imx8mp.h"
#else
/*
 * (C) Copyright 2011 CompuLab, Ltd. <www.compulab.co.il>
 *
 * Authors: Nikita Kiryanov <nikita@compulab.co.il>
 *	    Igor Grinberg <grinberg@compulab.co.il>
 */

#ifndef _EEPROM_
#define _EEPROM_
#include <errno.h>

#if !CONFIG_IS_ENABLED(TARGET_TRIMSLICE)
int cl_eeprom_read_mac_addr(uchar *buf, uint eeprom_bus);
u32 cl_eeprom_get_board_rev(uint eeprom_bus);
int cl_eeprom_get_product_name(uchar *buf, uint eeprom_bus);
#else
static inline int cl_eeprom_read_mac_addr(uchar *buf, uint eeprom_bus)
{
	return 1;
}
static inline u32 cl_eeprom_get_board_rev(uint eeprom_bus)
{
	return 0;
}
static inline int cl_eeprom_get_product_name(uchar *buf, uint eeprom_bus)
{
	return -ENOSYS;
}
#endif

#endif

#endif /* CONFIG_TARGET_COMPULAB_IMX8MP */
