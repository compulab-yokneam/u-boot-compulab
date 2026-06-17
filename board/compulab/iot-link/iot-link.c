#include <common.h>
#include <i2c.h>
#include <env.h>
#include <init.h>
#include <asm/arch/sys_proto.h>
#include "../common/eeprom.h"
#include <dm/of.h>

int board_late_init(void)
{
	u8 eeprom_buf[256];
	struct udevice *dev;
	int ret;
	ret = i2c_get_chip_for_busnum(2, 0x50, 1, &dev);
	if (ret) {
		printf("EEPROM: Failed to find I2C device\n");
		goto do_boot;
	}
	ret = dm_i2c_read(dev, 0x00, eeprom_buf, sizeof(eeprom_buf));
	if (ret) {
		printf("EEPROM: Read failed\n");
		goto do_boot;
	}
	char overlays[128] = "";
	for (int i = 0x90; i < sizeof(eeprom_buf) - 5; i++) {
		if (memcmp(&eeprom_buf[i], "FARS4", 5) == 0) {
			strcat(overlays, "#conf-iot-link-fars485.dtbo");
			i += 5;
		} else if (memcmp(&eeprom_buf[i], "FACAN", 5) == 0) {
			strcat(overlays, "#conf-iot-link-facan.dtbo");
			i += 5;
		} else if (memcmp(&eeprom_buf[i], "FBCAN", 5) == 0) {
			strcat(overlays, "#conf-iot-link-fbcan.dtbo");
			break;
		} else if (memcmp(&eeprom_buf[i], "FBRS4", 5) == 0) {
			strcat(overlays, "#conf-iot-link-fbrs485.dtbo");
			break;
		}
	}

do_boot:
#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif
	env_set("overlays", overlays);
	env_set("sec_boot", "no");
#ifdef CONFIG_AHAB_BOOT
	env_set("sec_boot", "yes");
#endif

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", CONFIG_SYS_BOARD);
	env_set("board_rev", "iMX93");
#endif
	board_get_mac_from_eeprom(0);
	board_get_mac_from_eeprom(1);
	return 0;
}
