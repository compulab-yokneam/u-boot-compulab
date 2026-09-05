#include <common.h>
#include <command.h>
#include <malloc.h>
#include <errno.h>
#include <hang.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <mmc.h>
#include <env.h>
#include "mmc.h"

__weak int mmc_map_to_kernel_blk(int dev_no)
{
	return dev_no;
}

void board_late_mmc_env_init(void)
{
	char cmd[32];
	char mmcblk[32];
	u32 dev_no = mmc_get_env_dev();

	if (!env_get_yesno("mmcautodetect"))
		return;

	env_set_ulong("mmcdev", dev_no);
	snprintf(mmcblk, sizeof(mmcblk), "/dev/mmcblk%dp2 rootwait rw",
		 mmc_map_to_kernel_blk(dev_no));
	env_set("mmcroot", mmcblk);
	snprintf(cmd, sizeof(cmd), "mmc dev %d", dev_no);
	run_command(cmd, 0);
}

static int env_dev = -1;
static int env_part= -1;

int get_env_dev() {
	return env_dev;
}

int get_env_part() {
	return env_part;
}

static int _board_mmc_get_env_dev(int devno)
{
	const ulong user_env_devno = env_get_hex("env_dev", ULONG_MAX);
	if (user_env_devno != ULONG_MAX) {
		printf("User Environment dev# is (%lu)\n", user_env_devno);
		return (int)user_env_devno;
	}
	return devno;
}

int board_mmc_get_env_dev(int devno)
{
	env_dev = _board_mmc_get_env_dev(devno);
	return env_dev;
}

static int _mmc_get_env_part(struct mmc *mmc)
{
	const ulong user_env_part = env_get_hex("env_part", ULONG_MAX);
	if (user_env_part != ULONG_MAX) {
		printf("User Environment part# is (%lu)\n", user_env_part);
		return (int)user_env_part;
	}

	return EXT_CSD_EXTRACT_BOOT_PART(mmc->part_config);
}

uint mmc_get_env_part(struct mmc *mmc)
{
	if (mmc->part_support && mmc->part_config != MMCPART_NOAVAILABLE) {
		uint partno = _mmc_get_env_part(mmc);
		env_part = partno;
		return partno;
	}
	return 0;
}
