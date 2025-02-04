#include <common.h>
#include <asm/io.h>

#define PIN9 0x200
#define GPIO1_PORT_DATA_DIRECTION_REG 0x47400054

void reset_board(void) {
	writel(PIN9, GPIO1_PORT_DATA_DIRECTION_REG);
}
