/*
 * (C) Copyright 2011 CompuLab, Ltd. <www.compulab.co.il>
 *
 * Authors: Nikita Kiryanov <nikita@compulab.co.il>
 *	    Igor Grinberg <grinberg@compulab.co.il>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <linux/ctype.h>
#include <common.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <i2c.h>
#include <dm.h>
#include <eeprom_layout.h>
#include <eeprom_field.h>
#include <linux/kernel.h>
#include <asm/setup.h>
#include <net.h>
#include "eeprom.h"

#ifndef CONFIG_SYS_I2C_EEPROM_ADDR
# define CONFIG_SYS_I2C_EEPROM_ADDR	0x50
# define CONFIG_SYS_I2C_EEPROM_ADDR_LEN	1
#endif

#ifndef CONFIG_SYS_I2C_EEPROM_ADDR_SB
# define CONFIG_SYS_I2C_EEPROM_ADDR_SB	0x54
#endif

#ifndef CONFIG_SYS_I2C_EEPROM_BUS
# define CONFIG_SYS_I2C_EEPROM_BUS	1
#endif

#ifndef CONFIG_SYS_I2C_EEPROM_BUS_SB
# define CONFIG_SYS_I2C_EEPROM_BUS_SB	0
#endif

#define LAYOUT_INVALID	0
#define LAYOUT_LEGACY	0xff

static int cl_eeprom_layout; /* Implicitly LAYOUT_INVALID */

struct eeprom_path {
	int bus;
	uint8_t chip;
};

static const struct eeprom_path eeprom_som = {
	CONFIG_SYS_I2C_EEPROM_BUS,
	CONFIG_SYS_I2C_EEPROM_ADDR
};
static const struct eeprom_path eeprom_sb = {
	CONFIG_SYS_I2C_EEPROM_BUS_SB,
	CONFIG_SYS_I2C_EEPROM_ADDR_SB
};

static const struct eeprom_path *working_eeprom;

static struct udevice *g_dev = NULL;

static int cpl_eeprom_init(void) {

	struct udevice *bus, *dev;
	int ret;

	if (!g_dev) {

		ret = uclass_get_device_by_seq(UCLASS_I2C, working_eeprom->bus, &bus);
		if (ret) {
			printf("%s: No bus %d\n", __func__, working_eeprom->bus);
			return ret;
		}

		ret = dm_i2c_probe(bus, working_eeprom->chip, 0, &dev);
		if (ret) {
			printf("%s: Can't find device id=0x%x, on bus %d\n",
				__func__, working_eeprom->chip, working_eeprom->bus);
			return ret;
		}

		/* Init */
		g_dev = dev;
	}

	return 0;
}

static int cl_eeprom_read(uint offset, uchar *buf, int len)
{
	int res;

	res = cpl_eeprom_init();
	if (res < 0)
		return res;

	res  = dm_i2c_read(g_dev, offset, buf, len);

	return res;
}

static int cl_eeprom_write(uint offset, uchar *buf, int len)
{
	int res;

	res = cpl_eeprom_init();
	if (res < 0)
		return res;

	res  = dm_i2c_write(g_dev, offset, buf, len);

	return res;
}

static int cl_eeprom_setup(const struct eeprom_path *eeprom)
{
	int res;

	/*
	 * We know the setup was already done when the layout is set to a valid
	 * value and we're using the same bus as before.
	 */
	if (cl_eeprom_layout != LAYOUT_INVALID && eeprom == working_eeprom)
		return 0;

	working_eeprom = eeprom;
	g_dev = NULL;
	res = cl_eeprom_read(EEPROM_LAYOUT_VER_OFFSET,
			     (uchar *)&cl_eeprom_layout, 1);
	if (res) {
		cl_eeprom_layout = LAYOUT_INVALID;
		return res;
	}

	if (cl_eeprom_layout == 0 || cl_eeprom_layout >= 0x20)
		cl_eeprom_layout = LAYOUT_LEGACY;

	return 0;
}

static void cpl_get_board_serial(struct tag_serialnr *serialnr, const struct eeprom_path *eeprom)
{
	u32 serial[2];
	uint offset;

	memset(serialnr, 0, sizeof(*serialnr));

	if (cl_eeprom_setup(eeprom))
		return;

	offset = (cl_eeprom_layout != LAYOUT_LEGACY) ?
		BOARD_SERIAL_OFFSET : BOARD_SERIAL_OFFSET_LEGACY;

	if (cl_eeprom_read(offset, (uchar *)serial, 8))
		return;

	if (serial[0] != 0xffffffff && serial[1] != 0xffffffff) {
		serialnr->low = serial[0];
		serialnr->high = serial[1];
	}
}
inline void cpl_get_som_serial(struct tag_serialnr *serialnr)
{
	return cpl_get_board_serial(serialnr, &eeprom_som);
}
inline void cpl_get_sb_serial(struct tag_serialnr *serialnr)
{
	return cpl_get_board_serial(serialnr, &eeprom_sb);
}

/*
 * Routine: cl_eeprom_read_mac_addr
 * Description: read mac address and store it in buf.
 */
int cl_eeprom_read_mac_addr(uchar *buf, uint eeprom_bus __attribute__((unused)))
{
	uint offset;
	int err;

	err = cl_eeprom_setup(&eeprom_som);
	if (err)
		return err;

	offset = (cl_eeprom_layout != LAYOUT_LEGACY) ?
			MAC_ADDR_OFFSET : MAC_ADDR_OFFSET_LEGACY;

	return cl_eeprom_read(offset, buf, 6);
}

/*
 * Routine: cl_eeprom_read_n_mac_addr
 * Description: read iface_number mac address and store it in buf.
 */
int cl_eeprom_read_n_mac_addr(uchar *buf, uint iface_number, uint eeprom_bus __attribute__((unused)))
{
	uint offset;
	int err;

	err = cl_eeprom_setup(&eeprom_som);
	if (err)
		return err;

	offset = (iface_number == 0) ? MAC_ADDR_OFFSET : MAC1_ADDR_OFFSET;

	err = cl_eeprom_read(offset, buf, 6);
	{ /* generate a random address if the som eeprom is empty */
		u32 mac0, mac2;
		mac0 = (u32) buf[0];
		mac2 = (u32) buf[2];
		if ( mac0 == mac2 )
			net_random_ethaddr(buf);
	}
	return err;
}

static u32 board_rev;

/*
 * Routine: cl_eeprom_get_board_rev
 * Description: read system revision from eeprom
 */
u32 cl_eeprom_get_board_rev(uint eeprom_bus __attribute__((unused)))
{
	char str[5]; /* Legacy representation can contain at most 4 digits */
	uint offset = BOARD_REV_OFFSET_LEGACY;

	if (board_rev)
		return board_rev;

	if (cl_eeprom_setup(&eeprom_som))
		return 0;

	if (cl_eeprom_layout != LAYOUT_LEGACY)
		offset = BOARD_REV_OFFSET;

	if (cl_eeprom_read(offset, (uchar *)&board_rev, BOARD_REV_SIZE))
		return 0;

	/*
	 * Convert legacy syntactic representation to semantic
	 * representation. i.e. for rev 1.00: 0x100 --> 0x64
	 */
	if (cl_eeprom_layout == LAYOUT_LEGACY) {
		sprintf(str, "%x", board_rev);
		board_rev = simple_strtoul(str, NULL, 10);
	}

	return board_rev;
}

static u32 cl_eeprom_get_revision(const struct eeprom_path *eeprom)
{
	u32 revision;

	if (cl_eeprom_setup(eeprom))
		return 0;

	if (cl_eeprom_read(BOARD_REV_OFFSET, (uchar *)&revision, BOARD_REV_SIZE))
		return 0;

	return revision;
}
u32 cl_eeprom_get_som_revision(void)
{
	return cl_eeprom_get_revision(&eeprom_som);
}
u32 cl_eeprom_get_sb_revision(void)
{
	return cl_eeprom_get_revision(&eeprom_sb);
}


/*
 * Routine: cl_eeprom_get_board_rev
 * Description: read system revision from eeprom
 *
 * @buf: buffer to store the product name
 * @eeprom_bus: i2c bus num of the eeprom
 *
 * @return: 0 on success, < 0 on failure
 */
int cl_eeprom_get_product_name(uchar *buf, uint eeprom_bus __attribute__((unused)))
{
	int err;

	if (buf == NULL)
		return -EINVAL;

	err = cl_eeprom_setup(&eeprom_som);
	if (err)
		return err;

	err = cl_eeprom_read(PRODUCT_NAME_OFFSET, buf, PRODUCT_NAME_SIZE);
	if (!err) /* Protect ourselves from invalid data (unterminated str) */
		buf[PRODUCT_NAME_SIZE - 1] = '\0';

	return err;
}

static int cl_eeprom_read_options(char *buf, const struct eeprom_path *eeprom)
{
	int len = 0;
	int err;
	uchar tmp[PRODUCT_OPTION_SIZE];

	err = cl_eeprom_setup(eeprom);
	if (err) {
		printf("%s: Error accesing i2c %x@%x\n", __func__, eeprom->bus, eeprom->chip);
		return snprintf(buf, PRODUCT_NAME_SIZE, "unknown");
	}

	for(int i = 0; i < PRODUCT_OPTION_NUM; ++i) {
		err = cl_eeprom_read(PRODUCT_OPTION_OFFSET + PRODUCT_OPTION_SIZE * i, tmp, PRODUCT_OPTION_SIZE);
		if (!err && tmp[0] != 0xff) // Check if the flash isn't written
			len += snprintf(buf + len, PRODUCT_OPTION_SIZE, (char*)tmp);
	}
	return len;
}
int cl_eeprom_read_som_options(char *buf)
{
	return cl_eeprom_read_options(buf, &eeprom_som);
}
int cl_eeprom_read_sb_options(char *buf)
{
	return cl_eeprom_read_options(buf, &eeprom_sb);
}

static int cl_eeprom_read_product_name(char *buf, const struct eeprom_path *eeprom)
{
	int len;
	int err;
	uchar tmp[PRODUCT_NAME_SIZE];

	err = cl_eeprom_setup(eeprom);
	if (err)
		printf("%s: Error accesing i2c %x@%x\n", __func__, eeprom->bus, eeprom->chip);
	else
		err = cl_eeprom_read(PRODUCT_NAME_OFFSET, tmp, PRODUCT_NAME_SIZE);

	if (!err && tmp[0] != 0xff) // Check if the flash isn't written
		len = snprintf(buf, PRODUCT_NAME_SIZE, (char*)tmp);
	else
		len = snprintf(buf, PRODUCT_NAME_SIZE, "unknown");

	return len;
}

int cl_eeprom_read_sb_name(char *buf)
{
	return cl_eeprom_read_product_name(buf, &eeprom_sb);
}

int cl_eeprom_read_som_name(char *buf)
{
	return cl_eeprom_read_product_name(buf, &eeprom_som);
}

void cl_eeprom_get_suite(char *buf)
{
	buf += cl_eeprom_read_product_name(buf, &eeprom_som);
	buf += sprintf(buf, " on ");
	buf += cl_eeprom_read_product_name(buf, &eeprom_sb);
}

#ifdef CONFIG_CMD_EEPROM_LAYOUT
/**
 * eeprom_field_print_bin_ver() - print a "version field" which contains binary
 *				  data
 *
 * Treat the field data as simple binary data, and print it formatted as a
 * version number (2 digits after decimal point).
 * The field size must be exactly 2 bytes.
 *
 * Sample output:
 *      Field Name      123.45
 *
 * @field:	an initialized field to print
 * @fbuf:	field buffer
 */
void eeprom_field_print_bin_ver(const struct eeprom_field *field, uchar *fbuf)
{
	if ((fbuf[0] == 0xff) && (fbuf[1] == 0xff)) {
		fbuf[0] = 0;
		fbuf[1] = 0;
	}

	printf(PRINT_FIELD_SEGMENT, field->name);
	int major = (fbuf[1] << 8 | fbuf[0]) / 100;
	int minor = (fbuf[1] << 8 | fbuf[0]) - major * 100;
	printf("%d.%02d\n", major, minor);
}

/**
 * eeprom_field_update_bin_ver() - update a "version field" which contains
 *				   binary data
 *
 * This function takes a version string in the form of x.y (x and y are both
 * decimal values, y is limited to two digits), translates it to the binary
 * form, then writes it to the field. The field size must be exactly 2 bytes.
 *
 * This function strictly enforces the data syntax, and will not update the
 * field if there's any deviation from it. It also protects from overflow.
 *
 * @field:	an initialized field
 * @fbuf:	field buffer
 * @value:	a version string
 *
 * Returns 0 on success, -1 on failure.
 */
int eeprom_field_update_bin_ver(struct eeprom_field *field, uchar *fbuf,
				char *value)
{
	char *endptr;
	char *tok = strtok(value, ".");
	if (tok == NULL)
		return -1;

	int num = simple_strtol(tok, &endptr, 0);
	if (*endptr != '\0')
		return -1;

	tok = strtok(NULL, "");
	if (tok == NULL)
		return -1;

	int remainder = simple_strtol(tok, &endptr, 0);
	if (*endptr != '\0')
		return -1;

	num = num * 100 + remainder;
	if (num >> 16)
		return -1;

	fbuf[0] = (unsigned char)num;
	fbuf[1] = num >> 8;

	return 0;
}

char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/**
 * eeprom_field_print_date() - print a field which contains date data
 *
 * Treat the field data as simple binary data, and print it formatted as a date.
 * Sample output:
 *      Field Name      07/Feb/2014
 *      Field Name      56/BAD/9999
 *
 * @field:	an initialized field to print
 * @fbuf:	field buffer
 */
void eeprom_field_print_date(const struct eeprom_field *field, uchar *fbuf)
{
	printf(PRINT_FIELD_SEGMENT, field->name);
	printf("%02d/", fbuf[0]);
	if (fbuf[1] >= 1 && fbuf[1] <= 12)
		printf("%s", months[fbuf[1] - 1]);
	else
		printf("BAD");

	printf("/%d\n", fbuf[3] << 8 | fbuf[2]);
}

static int validate_date(unsigned char day, unsigned char month,
			unsigned int year)
{
	int days_in_february;

	switch (month) {
	case 0:
	case 2:
	case 4:
	case 6:
	case 7:
	case 9:
	case 11:
		if (day > 31)
			return -1;
		break;
	case 3:
	case 5:
	case 8:
	case 10:
		if (day > 30)
			return -1;
		break;
	case 1:
		days_in_february = 28;
		if (year % 4 == 0) {
			if (year % 100 != 0)
				days_in_february = 29;
			else if (year % 400 == 0)
				days_in_february = 29;
		}

		if (day > days_in_february)
			return -1;

		break;
	default:
		return -1;
	}

	return 0;
}

/**
 * eeprom_field_update_date() - update a date field which contains binary data
 *
 * This function takes a date string in the form of x/Mon/y (x and y are both
 * decimal values), translates it to the binary representation, then writes it
 * to the field.
 *
 * This function strictly enforces the data syntax, and will not update the
 * field if there's any deviation from it. It also protects from overflow in the
 * year value, and checks the validity of the date.
 *
 * @field:	an initialized field
 * @fbuf:	field buffer
 * @value:	a date string
 *
 * Returns 0 on success, -1 on failure.
 */
int eeprom_field_update_date(struct eeprom_field *field, uchar *fbuf,
			     char *value)
{
	char *endptr;
	char *tok1 = strtok(value, "/");
	char *tok2 = strtok(NULL, "/");
	char *tok3 = strtok(NULL, "/");

	if (tok1 == NULL || tok2 == NULL || tok3 == NULL) {
		printf("%s: syntax error\n", field->name);
		return -1;
	}

	unsigned char day = (unsigned char)simple_strtol(tok1, &endptr, 0);
	if (*endptr != '\0' || day == 0) {
		printf("%s: invalid day\n", field->name);
		return -1;
	}

	unsigned char month;
	for (month = 1; month <= 12; month++)
		if (!strcmp(tok2, months[month - 1]))
			break;

	unsigned int year = simple_strtol(tok3, &endptr, 0);
	if (*endptr != '\0') {
		printf("%s: invalid year\n", field->name);
		return -1;
	}

	if (validate_date(day, month - 1, year)) {
		printf("%s: invalid date\n", field->name);
		return -1;
	}

	if (year >> 16) {
		printf("%s: year overflow\n", field->name);
		return -1;
	}

	fbuf[0] = day;
	fbuf[1] = month;
	fbuf[2] = (unsigned char)year;
	fbuf[3] = (unsigned char)(year >> 8);

	return 0;
}

#define	LAYOUT_VERSION_LEGACY 1
#define	LAYOUT_VERSION_VER1 2
#define	LAYOUT_VERSION_VER2 3
#define	LAYOUT_VERSION_VER3 4
#define	LAYOUT_VERSION_VER4 5

extern struct eeprom_field layout_unknown[1];

#define DEFINE_FIELD_FUNC(x) eeprom_field_print_##x, eeprom_field_update_##x, \
			     eeprom_field_read_bin

#define FIELD_FUNC_RES_LAST eeprom_field_print_reserved, \
			    eeprom_field_update_ascii,   \
			    eeprom_field_read_bin

#define FIELD_FUNC_SERIAL eeprom_field_print_bin_rev,  \
			  eeprom_field_update_bin_rev, \
			  eeprom_field_read_rev

#ifdef CONFIG_CM_T3X
struct eeprom_field layout_legacy[5] = {
	{ "MAC address",          6, DEFINE_FIELD_FUNC(mac) },
	{ "Board Revision",       2, DEFINE_FIELD_FUNC(bin) },
	{ "Serial Number",        8, DEFINE_FIELD_FUNC(bin) },
	{ "Board Configuration", 64, DEFINE_FIELD_FUNC(ascii) },
	{ RESERVED_FIELDS,      176, FIELD_FUNC_RES_LAST }
};
#else
#define layout_legacy layout_unknown
#endif

#if defined(CONFIG_CM_T3X) || defined(CONFIG_CM_T3517)
struct eeprom_field layout_v1[12] = {
	{ "Major Revision",      2, DEFINE_FIELD_FUNC(bin_ver) },
	{ "Minor Revision",      2, DEFINE_FIELD_FUNC(bin_ver) },
	{ "1st MAC Address",     6, DEFINE_FIELD_FUNC(mac) },
	{ "2nd MAC Address",     6, DEFINE_FIELD_FUNC(mac) },
	{ "Production Date",     4, DEFINE_FIELD_FUNC(date) },
	{ "Serial Number",      12, FIELD_FUNC_SERIAL },
	{ RESERVED_FIELDS,      96, DEFINE_FIELD_FUNC(reserved) },
	{ "Product Name",       16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #1", 16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #2", 16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #3", 16, DEFINE_FIELD_FUNC(ascii) },
	{ RESERVED_FIELDS,      64, FIELD_FUNC_RES_LAST }
};
#else
#define layout_v1 layout_unknown
#endif

struct eeprom_field layout_v2[15] = {
	{ "Major Revision",            2, DEFINE_FIELD_FUNC(bin_ver) },
	{ "Minor Revision",            2, DEFINE_FIELD_FUNC(bin_ver) },
	{ "1st MAC Address",           6, DEFINE_FIELD_FUNC(mac) },
	{ "2nd MAC Address",           6, DEFINE_FIELD_FUNC(mac) },
	{ "Production Date",           4, DEFINE_FIELD_FUNC(date) },
	{ "Serial Number",            12, FIELD_FUNC_SERIAL },
	{ "3rd MAC Address (WIFI)",    6, DEFINE_FIELD_FUNC(mac) },
	{ "4th MAC Address (Bluetooth)", 6, DEFINE_FIELD_FUNC(mac) },
	{ "Layout Version",            1, DEFINE_FIELD_FUNC(bin) },
	{ RESERVED_FIELDS,            83, DEFINE_FIELD_FUNC(reserved) },
	{ "Product Name",             16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #1",       16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #2",       16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #3",       16, DEFINE_FIELD_FUNC(ascii) },
	{ RESERVED_FIELDS,            64, FIELD_FUNC_RES_LAST },
};

struct eeprom_field layout_v3[16] = {
	{ "Major Revision",            2, DEFINE_FIELD_FUNC(bin_ver) },
	{ "Minor Revision",            2, DEFINE_FIELD_FUNC(bin_ver) },
	{ "1st MAC Address",           6, DEFINE_FIELD_FUNC(mac) },
	{ "2nd MAC Address",           6, DEFINE_FIELD_FUNC(mac) },
	{ "Production Date",           4, DEFINE_FIELD_FUNC(date) },
	{ "Serial Number",            12, FIELD_FUNC_SERIAL },
	{ "3rd MAC Address (WIFI)",    6, DEFINE_FIELD_FUNC(mac) },
	{ "4th MAC Address (Bluetooth)", 6, DEFINE_FIELD_FUNC(mac) },
	{ "Layout Version",            1, DEFINE_FIELD_FUNC(bin) },
	{ "CompuLab EEPROM ID",        3, DEFINE_FIELD_FUNC(bin) },
	{ RESERVED_FIELDS,            80, DEFINE_FIELD_FUNC(reserved) },
	{ "Product Name",             16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #1",       16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #2",       16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #3",       16, DEFINE_FIELD_FUNC(ascii) },
	{ RESERVED_FIELDS,            64, FIELD_FUNC_RES_LAST },
};

struct eeprom_field layout_v4[21] = {
	{ "Major Revision",            2, DEFINE_FIELD_FUNC(bin_ver) },
	{ "Minor Revision",            2, DEFINE_FIELD_FUNC(bin_ver) },
	{ "1st MAC Address",           6, DEFINE_FIELD_FUNC(mac) },
	{ "2nd MAC Address",           6, DEFINE_FIELD_FUNC(mac) },
	{ "Production Date",           4, DEFINE_FIELD_FUNC(date) },
	{ "Serial Number",            12, FIELD_FUNC_SERIAL },
	{ "3rd MAC Address (WIFI)",    6, DEFINE_FIELD_FUNC(mac) },
	{ "4th MAC Address (Bluetooth)", 6, DEFINE_FIELD_FUNC(mac) },
	{ "Layout Version",            1, DEFINE_FIELD_FUNC(bin) },
	{ "CompuLab EEPROM ID",        3, DEFINE_FIELD_FUNC(bin) },
	{ "5th MAC Address",           6, DEFINE_FIELD_FUNC(mac) },
	{ "6th MAC Address",           6, DEFINE_FIELD_FUNC(mac) },
	{ RESERVED_FIELDS,             4, DEFINE_FIELD_FUNC(reserved) },
	{ RESERVED_FIELDS,            64, DEFINE_FIELD_FUNC(reserved) },
	{ "Product Name",             16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #1",       16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #2",       16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #3",       16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #4",       16, DEFINE_FIELD_FUNC(ascii) },
	{ "Product Options #5",       16, DEFINE_FIELD_FUNC(ascii) },
	{ RESERVED_FIELDS,            32, DEFINE_FIELD_FUNC(reserved) },
};

void eeprom_layout_assign(struct eeprom_layout *layout, int layout_version)
{
	switch (layout->layout_version) {
	case LAYOUT_VERSION_LEGACY:
		layout->fields = layout_legacy;
		layout->num_of_fields = ARRAY_SIZE(layout_legacy);
		break;
	case LAYOUT_VERSION_VER1:
		layout->fields = layout_v1;
		layout->num_of_fields = ARRAY_SIZE(layout_v1);
		break;
	case LAYOUT_VERSION_VER2:
		layout->fields = layout_v2;
		layout->num_of_fields = ARRAY_SIZE(layout_v2);
		break;
	case LAYOUT_VERSION_VER3:
		layout->fields = layout_v3;
		layout->num_of_fields = ARRAY_SIZE(layout_v3);
		break;
	case LAYOUT_VERSION_VER4:
		layout->fields = layout_v4;
		layout->num_of_fields = ARRAY_SIZE(layout_v4);
		break;
	default:
		__eeprom_layout_assign(layout, layout_version);
	}
}

int eeprom_parse_layout_version(char *str)
{
	if (!strcmp(str, "legacy"))
		return LAYOUT_VERSION_LEGACY;
	else if (!strcmp(str, "v1"))
		return LAYOUT_VERSION_VER1;
	else if (!strcmp(str, "v2"))
		return LAYOUT_VERSION_VER2;
	else if (!strcmp(str, "v3"))
		return LAYOUT_VERSION_VER3;
	else if (!strcmp(str, "v4"))
		return LAYOUT_VERSION_VER4;
	else
		return LAYOUT_VERSION_UNRECOGNIZED;
}

int eeprom_layout_detect(unsigned char *data)
{
	switch (data[EEPROM_LAYOUT_VER_OFFSET]) {
	case 0xff:
	case 0:
		return LAYOUT_VERSION_VER1;
	case 2:
		return LAYOUT_VERSION_VER2;
	case 3:
		return LAYOUT_VERSION_VER3;
	case 4 ... 0x1f:
		return LAYOUT_VERSION_VER4;
	}

	if (data[EEPROM_LAYOUT_VER_OFFSET] >= 0x20)
		return LAYOUT_VERSION_LEGACY;

	return LAYOUT_VERSION_UNRECOGNIZED;
}
#endif

#define BOARD_LDO4_OFFSET 0x60
#define BOARD_LDO4_SIZE 1
static const struct eeprom_path eeprom_51 = {
	/*CONFIG_SYS_I2C_EEPROM_BUS*/ 0x1,
	/*CONFIG_SYS_I2C_EEPROM_ADDR_P1*/ 0x51,
};

static u8 board_ldo4 = 0xff;
u8 cl_eeprom_get_ldo4(void)
{
	g_dev = NULL;
	working_eeprom = &eeprom_51;
	if (cl_eeprom_read(BOARD_LDO4_OFFSET, (uchar *)&board_ldo4, BOARD_LDO4_SIZE))
		return 0xff;

	return board_ldo4;
};

u8 cl_eeprom_set_ldo4(u8 ldo4)
{
	g_dev = NULL;
	working_eeprom = &eeprom_51;
	if (cl_eeprom_write(BOARD_LDO4_OFFSET, (uchar *)&ldo4, BOARD_LDO4_SIZE))
		return 0xff;
	board_ldo4 = ldo4;

	return board_ldo4;
};

#ifndef CONFIG_SPL_BUILD
#define BOARD_DDRINFO_OFFSET 0x40
#define BOARD_DDRINFO_SIZE 4
static u32 board_ddrinfo = 0xdeadbeef;

u32 cl_eeprom_get_ddrinfo(void)
{
	g_dev = NULL;
	working_eeprom = &eeprom_51;
		if (cl_eeprom_read(BOARD_DDRINFO_OFFSET, (uchar *)&board_ddrinfo, BOARD_DDRINFO_SIZE))
			return 0;
	return board_ddrinfo;
};

u32 cl_eeprom_set_ddrinfo(u32 ddrinfo)
{
	g_dev = NULL;
	working_eeprom = &eeprom_51;
	if (cl_eeprom_write(BOARD_DDRINFO_OFFSET, (uchar *)&ddrinfo, BOARD_DDRINFO_SIZE))
		return 0;

	board_ddrinfo = ddrinfo;

	return board_ddrinfo;
};

void cl_eeprom_clr_ddrinfo(void)
{
	u32 ddrinfo[2] = { 0xFFFFFFFF , 0xFFFFFFFF };
	g_dev = NULL;
	working_eeprom = &eeprom_51;
	cl_eeprom_write(BOARD_DDRINFO_OFFSET, (uchar *)ddrinfo, (BOARD_DDRINFO_SIZE<<1));
	return;
};

#define BOARD_DDRSUBIND_OFFSET 0x44
#define BOARD_DDRSUBIND_SIZE 1
static u8 board_ddrsubind = 0xff;
u8 cl_eeprom_get_subind(void)
{
	g_dev = NULL;
	working_eeprom = &eeprom_51;
	if (cl_eeprom_read(BOARD_DDRSUBIND_OFFSET, (uchar *)&board_ddrsubind, BOARD_DDRSUBIND_SIZE))
		return 0xff;

	return board_ddrsubind;
};

u8 cl_eeprom_set_subind(u8 ddrsubind)
{
	g_dev = NULL;
	working_eeprom = &eeprom_51;
	if (cl_eeprom_write(BOARD_DDRSUBIND_OFFSET, (uchar *)&ddrsubind, BOARD_DDRSUBIND_SIZE))
		return 0xff;

	board_ddrsubind = ddrsubind;

	return board_ddrsubind;
};
#endif
