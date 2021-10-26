/*
 * (C) Copyright 2011 CompuLab, Ltd. <www.compulab.co.il>
 *
 * Authors: Nikita Kiryanov <nikita@compulab.co.il>
 *	    Igor Grinberg <grinberg@compulab.co.il>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _EEPROM_
#define _EEPROM_
#include <errno.h>
#include <asm/setup.h>

#define EEPROM_LAYOUT_VER_OFFSET	44
#define BOARD_SERIAL_OFFSET		20
#define BOARD_SERIAL_OFFSET_LEGACY	8
#define BOARD_REV_OFFSET		0
#define BOARD_REV_OFFSET_LEGACY		6
#define BOARD_REV_SIZE			2
#define PRODUCT_NAME_OFFSET		128
#define PRODUCT_NAME_SIZE		16
#define PRODUCT_OPTION_OFFSET		144
#define PRODUCT_OPTION_SIZE		16
#define PRODUCT_OPTION_NUM		5
#define MAC_ADDR_OFFSET			4
#define MAC1_ADDR_OFFSET		10
#define MAC_ADDR_OFFSET_LEGACY		0

#if (defined(CONFIG_SYS_I2C) || defined(CONFIG_DM_I2C))
int cl_eeprom_read_mac_addr(uchar *buf, uint eeprom_bus);
int cl_eeprom_read_n_mac_addr(uchar *buf, uint iface_number, uint eeprom_bus);
u32 cl_eeprom_get_som_revision(void);
u32 cl_eeprom_get_sb_revision(void);
int cl_eeprom_get_product_name(uchar *buf, uint eeprom_bus);
int cl_eeprom_read_sb_name(char *buf);
int cl_eeprom_read_som_name(char *buf);
int cl_eeprom_read_som_options(char *buf);
int cl_eeprom_read_sb_options(char *buf);
void cl_eeprom_get_suite(char* buf);
void cpl_get_som_serial(struct tag_serialnr *serialnr);
void cpl_get_sb_serial(struct tag_serialnr *serialnr);
#else
static inline int cl_eeprom_read_mac_addr(uchar *buf, uint eeprom_bus)
{
	return 1;
}
static int cl_eeprom_read_n_mac_addr(uchar *buf, uint iface_number, uint eeprom_bus)
{
	return 1;
}
static inline int cl_eeprom_get_product_name(uchar *buf, uint eeprom_bus)
{
	return -ENOSYS;
}
#endif

#endif
