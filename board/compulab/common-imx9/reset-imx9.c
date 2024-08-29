#include <common.h>
#include <asm/io.h>

#define PIN12 0x1000
#define GPIO2_PORT_DATA_DIRECTION_REG 0x43810054

void reset_board(void) {
	writel(PIN12, GPIO2_PORT_DATA_DIRECTION_REG);
}
