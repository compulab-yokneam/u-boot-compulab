#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <hang.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <mmc.h>
#include "mmc.h"
#include "eeprom.h"

void fdt_set_sn(void *blob)
{
	u32 rev;
	char buf[100];
	int len;
	union {
		struct tag_serialnr	s;
		u64			u;
	} serialnr;

	len = cl_eeprom_read_som_name(buf);
	fdt_setprop(blob, 0, "product-name", buf, len);

	len = cl_eeprom_read_sb_name(buf);
	fdt_setprop(blob, 0, "baseboard-name", buf, len);

	cpl_get_som_serial(&serialnr.s);
	fdt_setprop(blob, 0, "product-sn", buf, sprintf(buf, "%llx", serialnr.u) + 1);

	cpl_get_sb_serial(&serialnr.s);
	fdt_setprop(blob, 0, "baseboard-sn", buf, sprintf(buf, "%llx", serialnr.u) + 1);

	rev = cl_eeprom_get_som_revision();
	fdt_setprop(blob, 0, "product-revision", buf,
		sprintf(buf, "%u.%02u", rev/100 , rev%100 ) + 1);

	rev = cl_eeprom_get_sb_revision();
	fdt_setprop(blob, 0, "baseboard-revision", buf,
		sprintf(buf, "%u.%02u", rev/100 , rev%100 ) + 1);

	len = cl_eeprom_read_som_options(buf);
	fdt_setprop(blob, 0, "product-options", buf, len);

	len = cl_eeprom_read_sb_options(buf);
	fdt_setprop(blob, 0, "baseboard-options", buf, len);

	return;
}

int fdt_set_env_addr(void *blob)
{
	char tmp[32];
	int nodeoff = fdt_add_subnode(blob, 0, "fw_env");
	int env_dev = get_env_dev();
	int env_part = get_env_part();
	if(0 > nodeoff)
		return nodeoff;

	fdt_setprop(blob, nodeoff, "env_off", tmp, sprintf(tmp, "0x%x", CONFIG_ENV_OFFSET));
	fdt_setprop(blob, nodeoff, "env_size", tmp, sprintf(tmp, "0x%x", CONFIG_ENV_SIZE));
	if(0 < env_dev) {
		switch(env_part) {
		case 1 ... 2:
			fdt_setprop(blob, nodeoff, "env_dev", tmp, sprintf(tmp, "/dev/mmcblk%iboot%i", env_dev, env_part - 1));
			break;
		default:
			fdt_setprop(blob, nodeoff, "env_dev", tmp, sprintf(tmp, "/dev/mmcblk%i", env_dev));
			break;
		}
	}
	return 0;
}
