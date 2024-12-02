// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2009-2016 CompuLab, Ltd.
 *
 * Authors: Nikita Kiryanov <nikita@compulab.co.il>
 *	    Igor Grinberg <grinberg@compulab.co.il>
 */

#include <common.h>
#include <linux/string.h>
#include <eeprom_field.h>

static void __eeprom_field_print_bin(const struct eeprom_field *field,
				     uchar *fbuf, char *delimiter, bool reverse)
{
	int i;
	int from = reverse ? field->size - 1 : 0;
	int to = reverse ? 0 : field->size - 1;

	printf(PRINT_FIELD_SEGMENT, field->name);
	for (i = from; i != to; reverse ? i-- : i++)
		printf("%02x%s", fbuf[i], delimiter);

	printf("%02x\n", fbuf[i]);
}

static int __eeprom_field_update_bin(struct eeprom_field *field, uchar *fbuf,
				     const char *value, bool reverse)
{
	int len = strlen(value);
	int k, j, i = reverse ? len - 1 : 0;
	unsigned char byte;
	char *endptr;

	/* each two characters in the string fit in one byte */
	if (len > field->size * 2)
		return -1;

	memset(fbuf, 0, field->size);

	/* i - string iterator, j - buf iterator */
	for (j = 0; j < field->size; j++) {
		byte = 0;
		char tmp[3] = { 0, 0, 0 };

		if ((reverse && i < 0) || (!reverse && i >= len))
			break;

		for (k = 0; k < 2; k++) {
			if (reverse && i == 0) {
				tmp[k] = value[i];
				break;
			}

			tmp[k] = value[reverse ? i - 1 + k : i + k];
		}

		byte = simple_strtoul(tmp, &endptr, 0);
		if (*endptr != '\0' || byte < 0)
			return -1;

		fbuf[j] = byte;
		i = reverse ? i - 2 : i + 2;
	}

	return 0;
}

static int __eeprom_field_update_bin_delim(struct eeprom_field *field,
					   uchar *fbuf, char *value,
					   char *delimiter)
{
	int count = 0;
	int i, val;
	const char *tmp = value;
	char *tok;
	char *endptr;

	tmp = strstr(tmp, delimiter);
	while (tmp != NULL) {
		count++;
		tmp++;
		tmp = strstr(tmp, delimiter);
	}

	if (count > field->size)
		return -1;

	tok = strtok(value, delimiter);
	for (i = 0; tok && i < field->size; i++) {
		val = simple_strtoul(tok, &endptr, 0);
		if (*endptr != '\0')
			return -1;

		/* here we assume that each tok is no more than byte long */
		fbuf[i] = (unsigned char)val;
		tok = strtok(NULL, delimiter);
	}

	return 0;
}

/**
 * eeprom_field_print_bin() - print a field which contains binary data
 *
 * Treat the field data as simple binary data, and print it as two digit
 * hexadecimal values.
 * Sample output:
 *      Field Name       0102030405060708090a
 *
 * @field:	an initialized field to print
 * @fbuf:	field buffer
 */
void eeprom_field_print_bin(const struct eeprom_field *field, uchar *fbuf)
{
	__eeprom_field_print_bin(field, fbuf, "", false);
}

/**
 * eeprom_field_update_bin() - Update field with new data in binary form
 *
 * @field:	an initialized field
 * @fbuf:	field buffer
 * @value:	a string of values (i.e. "10b234a")
 */
int eeprom_field_update_bin(struct eeprom_field *field, uchar *fbuf,
			    char *value)
{
	return __eeprom_field_update_bin(field, fbuf, value, false);
}

/**
 * eeprom_field_read_bin() - Read field data in binary form
 *
 * @field:     an initialized field
 * @fbuf:      field buffer
 * @buf:       read buffer
 * @buf_size:  read buffer size
 */
int eeprom_field_read_bin(const struct eeprom_field *field, uchar *fbuf,
                         uchar *buf, int buf_size)
{
	memset(buf, 0, buf_size);
	if (field->size <= 0)
		return 0;
	if (field->size > buf_size)
		memcpy(buf, fbuf, buf_size);
	else
		memcpy(buf, fbuf, field->size);

	return 0;
}

/**
 * eeprom_field_update_reserved() - Update reserved field with new data in
 *				    binary form
 *
 * @field:	an initialized field
 * @fbuf:	field buffer
 * @value:	a space delimited string of byte values (i.e. "1 02 3 0x4")
 */
int eeprom_field_update_reserved(struct eeprom_field *field, uchar *fbuf,
				 char *value)
{
	return __eeprom_field_update_bin_delim(field, fbuf, value, " ");
}

/**
 * eeprom_field_print_bin_rev() - print a field which contains binary data in
 *				  reverse order
 *
 * Treat the field data as simple binary data, and print it in reverse order
 * as two digit hexadecimal values.
 *
 * Data in field:
 *                      0102030405060708090a
 * Sample output:
 *      Field Name      0a090807060504030201
 *
 * @field:	an initialized field to print
 * @fbuf:	field buffer
 */
void eeprom_field_print_bin_rev(const struct eeprom_field *field, uchar *fbuf)
{
	__eeprom_field_print_bin(field, fbuf, "", true);
}

/**
 * eeprom_field_update_bin_rev() - Update field with new data in binary form,
 *				   storing it in reverse
 *
 * This function takes a string of byte values, and stores them
 * in the field in the reverse order. i.e. if the input string was "1234",
 * "3412" will be written to the field.
 *
 * @field:	an initialized field
 * @fbuf:	field buffer
 * @value:	a string of byte values
 */
int eeprom_field_update_bin_rev(struct eeprom_field *field, uchar *fbuf,
				char *value)
{
	return __eeprom_field_update_bin(field, fbuf, value, true);
}

/**
 * eeprom_field_read_rev() - Read reversed binary data
 *
 * @field:     an initialized field
 * @fbuf:      field buffer
 * @buf:       read buffer
 * @buf_size:  read buffer size
 */
int eeprom_field_read_rev(const struct eeprom_field *field, uchar *fbuf,
			  uchar *buf, int buf_size)
{
	int i;

	memset(buf, 0, buf_size);

	for (i = field->size - 1; i >= 0; i--, buf++)
		*buf = fbuf[i];

	return 0;
}

/**
 * eeprom_field_print_mac_addr() - print a field which contains a mac address
 *
 * Treat the field data as simple binary data, and print it formatted as a MAC
 * address.
 * Sample output:
 *      Field Name     01:02:03:04:05:06
 *
 * @field:	an initialized field to print
 * @fbuf:	field buffer
 */
void eeprom_field_print_mac(const struct eeprom_field *field, uchar *fbuf)

{
	__eeprom_field_print_bin(field, fbuf, ":", false);
}

/**
 * eeprom_field_update_mac() - Update a mac address field which contains binary
 *			       data
 *
 * @field:	an initialized field
 * @fbuf:	field buffer
 * @value:	a colon delimited string of byte values (i.e. "1:02:3:ff")
 */
int eeprom_field_update_mac(struct eeprom_field *field, uchar *fbuf,
			    char *value)
{
	return __eeprom_field_update_bin_delim(field, fbuf, value, ":");
}

/**
 * eeprom_field_print_ascii() - print a field which contains ASCII data
 * @field:	an initialized field to print
 * @fbuf:	field buffer
 */
void eeprom_field_print_ascii(const struct eeprom_field *field, uchar *fbuf)
{
	char format[8];

	sprintf(format, "%%.%ds\n", field->size);
	printf(PRINT_FIELD_SEGMENT, field->name);
	printf(format, fbuf);
}

/**
 * eeprom_field_update_ascii() - Update field with new data in ASCII form
 * @field:	an initialized field
 * @fbuf:	field buffer
 * @value:	the new string data
 *
 * Returns 0 on success, -1 of failure (new string too long).
 */
int eeprom_field_update_ascii(struct eeprom_field *field, uchar *fbuf,
			      char *value)
{
	if (strlen(value) >= field->size) {
		printf("%s: new data too long\n", field->name);
		return -1;
	}

	strncpy((char *)fbuf, value, field->size - 1);
	fbuf[field->size - 1] = '\0';

	return 0;
}

/**
 * eeprom_field_print_reserved() - print the "Reserved fields" field
 *
 * Print a notice that the following field_size bytes are reserved.
 *
 * Sample output:
 *      Reserved fields              (64 bytes)
 *
 * @field:	an initialized field to print
 * @fbuf:	field buffer
 */
void eeprom_field_print_reserved(const struct eeprom_field *field, uchar *fbuf)
{
	printf(PRINT_FIELD_SEGMENT, "Reserved fields\t");
	printf("(%d bytes)\n", field->size);
}
