#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <hang.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <mmc.h>
#include "mmc.h"
#include "eeprom.h"
#include <env_internal.h>
#include <env.h>
#include <fdt_support.h>
#include <linux/ctype.h>

static int parse_hex_u64(const char *str, u64 *value)
{
	const char *digits = str;
	char *endp;

	if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
		digits += 2;
	if (!isxdigit(*digits))
		return -EINVAL;

	*value = simple_strtoull(str, &endp, 16);
	return *endp ? -EINVAL : 0;
}

int fdt_set_sn(void *blob)
{
	u32 rev;
	char buf[100];
	int len, ret;
	union {
		struct tag_serialnr	s;
		u64			u;
	} serialnr;

	len = cl_eeprom_read_som_name(buf);
	ret = fdt_setprop(blob, 0, "product-name", buf, len + 1);
	if (ret)
		return ret;

	len = cl_eeprom_read_sb_name(buf);
	ret = fdt_setprop(blob, 0, "baseboard-name", buf, len + 1);
	if (ret)
		return ret;

	cpl_get_som_serial(&serialnr.s);
	snprintf(buf, sizeof(buf), "%llx", serialnr.u);
	ret = fdt_setprop_string(blob, 0, "product-sn", buf);
	if (ret)
		return ret;

	cpl_get_sb_serial(&serialnr.s);
	snprintf(buf, sizeof(buf), "%llx", serialnr.u);
	ret = fdt_setprop_string(blob, 0, "baseboard-sn", buf);
	if (ret)
		return ret;

	rev = cl_eeprom_get_som_revision();
	snprintf(buf, sizeof(buf), "%u.%02u", rev / 100, rev % 100);
	ret = fdt_setprop_string(blob, 0, "product-revision", buf);
	if (ret)
		return ret;

	rev = cl_eeprom_get_sb_revision();
	snprintf(buf, sizeof(buf), "%u.%02u", rev / 100, rev % 100);
	ret = fdt_setprop_string(blob, 0, "baseboard-revision", buf);
	if (ret)
		return ret;

	len = cl_eeprom_read_som_options(buf);
	ret = fdt_setprop(blob, 0, "product-options", buf, len + 1);
	if (ret)
		return ret;

	len = cl_eeprom_read_sb_options(buf);
	ret = fdt_setprop(blob, 0, "baseboard-options", buf, len + 1);

	return ret;
}

int fdt_set_env_addr(void *blob)
{
#ifndef CONFIG_SYS_REDUNDAND_ENVIRONMENT
	char tmp[64];
	const char *src = default_environment;
	char *env_to_export, *dst;
	size_t remaining;
	int env_dev, env_part;
	int nodeoff, ret;

	nodeoff = fdt_add_subnode(blob, 0, "fw_env");
	if (nodeoff == -FDT_ERR_EXISTS)
		nodeoff = fdt_path_offset(blob, "/fw_env");
	if (nodeoff < 0)
		return nodeoff;

	snprintf(tmp, sizeof(tmp), "0x%x", CONFIG_ENV_OFFSET);
	ret = fdt_setprop_string(blob, nodeoff, "env_off", tmp);
	if (ret)
		return ret;

	snprintf(tmp, sizeof(tmp), "0x%x", CONFIG_ENV_SIZE);
	ret = fdt_setprop_string(blob, nodeoff, "env_size", tmp);
	if (ret)
		return ret;

	env_dev = get_env_dev();
	env_part = get_env_part();

	if (env_dev != -1) {
		switch (env_part) {
		case 2:
		case 1:
			snprintf(tmp, sizeof(tmp), "/dev/mmcblk%iboot%i",
				 env_dev, env_part - 1);
			ret = fdt_setprop_string(blob, nodeoff, "env_dev", tmp);
			if (ret)
				return ret;
			snprintf(tmp, sizeof(tmp),
				 "/dev/mmcblk%iboot%i\t0x%x\t0x%x\n", env_dev,
				 env_part - 1, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE);
			break;
		default:
			snprintf(tmp, sizeof(tmp), "/dev/mmcblk%i", env_dev);
			ret = fdt_setprop_string(blob, nodeoff, "env_dev", tmp);
			if (ret)
				return ret;
			snprintf(tmp, sizeof(tmp), "/dev/mmcblk%i\t0x%x\t0x%x\n",
				 env_dev, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE);
			break;
		}

		ret = fdt_setprop_string(blob, nodeoff, "fw_env.config", tmp);
		if (ret)
			return ret;
	}

	env_to_export = malloc(CONFIG_ENV_SIZE);
	if (!env_to_export)
		return -ENOMEM;
	dst = env_to_export;
	remaining = CONFIG_ENV_SIZE;

	while (*src) {
		size_t len = strnlen(src, remaining);

		if (len == remaining || len + 1 > remaining) {
			ret = -E2BIG;
			goto out;
		}

		memcpy(dst, src, len);
		dst[len] = '\n';
		dst += len + 1;
		remaining -= len + 1;
		src += len + 1;
	}

	ret = fdt_setprop(blob, nodeoff, "default_env", env_to_export,
			  dst - env_to_export);
out:
	free(env_to_export);
	return ret;
#endif
	return 0;
}

int fdt_fixup_jailhouse_memory(void *blob)
{
	const char *value;
	char *copy, *cursor, *token;
	u64 base[CONFIG_NR_DRAM_BANKS] = { 0 };
	u64 size[CONFIG_NR_DRAM_BANKS] = { 0 };
	int banks = 0;
	int ret = 0;

	value = env_get("jh_root_mem");
	if (!value)
		return 0;
	if (!*value)
		return -EINVAL;

	copy = strdup(value);
	if (!copy)
		return -ENOMEM;

	cursor = copy;
	while ((token = strsep(&cursor, ","))) {
		char *base_str;

		if (banks >= CONFIG_NR_DRAM_BANKS) {
			printf("Error: The number of size@base exceeds CONFIG_NR_DRAM_BANKS.\n");
			ret = -EINVAL;
			goto out;
		}

		base_str = strchr(token, '@');
		if (!base_str || base_str == token || !base_str[1] ||
		    strchr(base_str + 1, '@')) {
			printf("The format of jh_root_mem is size@base[,size@base...].\n");
			ret = -EINVAL;
			goto out;
		}

		*base_str++ = '\0';
		if (parse_hex_u64(token, &size[banks])) {
			ret = -EINVAL;
			goto bad_value;
		}

		if (parse_hex_u64(base_str, &base[banks])) {
			ret = -EINVAL;
			goto bad_value;
		}

		banks++;
	}

	ret = fdt_fixup_memory_banks(blob, base, size, banks);
	goto out;

bad_value:
	printf("Invalid hexadecimal value in jh_root_mem.\n");
out:
	free(copy);
	return ret;
}
