// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 NXP
 */

#include <common.h>
#include <errno.h>
#include <log.h>
#include <asm/io.h>
#include <asm/arch/ddr.h>
#include <asm/arch/clock.h>
#include <asm/arch/ddr.h>
#include <asm/arch/sys_proto.h>

static inline void poll_pmu_message_ready(void)
{
	unsigned int reg;

	do {
		reg = reg32_read(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + ddrphy_addr_remap(0xd0004));
	} while (reg & 0x1);
}

static inline void ack_pmu_message_receive(void)
{
	unsigned int reg;

	reg32_write(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + ddrphy_addr_remap(0xd0031), 0x0);

	do {
		reg = reg32_read(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + ddrphy_addr_remap(0xd0004));
	} while (!(reg & 0x1));

	reg32_write(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + ddrphy_addr_remap(0xd0031), 0x1);
}

static inline unsigned int get_mail(void)
{
	unsigned int reg;

	poll_pmu_message_ready();

	reg = reg32_read(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + ddrphy_addr_remap(0xd0032));

	ack_pmu_message_receive();

	return reg;
}

static inline unsigned int get_stream_message(void)
{
	unsigned int reg, reg2;

	poll_pmu_message_ready();

	reg = reg32_read(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + ddrphy_addr_remap(0xd0032));

	reg2 = reg32_read(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + ddrphy_addr_remap(0xd0034));

	reg2 = (reg2 << 16) | reg;

	ack_pmu_message_receive();

	return reg2;
}

static inline void decode_major_message(unsigned int mail)
{
	debug("[PMU Major message = 0x%08x]\n", mail);
}

static inline void decode_streaming_message(void)
{
	unsigned int string_index, arg __maybe_unused;
	int i = 0;

	string_index = get_stream_message();
	debug("PMU String index = 0x%08x\n", string_index);
	while (i < (string_index & 0xffff)) {
		arg = get_stream_message();
		debug("arg[%d] = 0x%08x\n", i, arg);
		i++;
	}

	debug("\n");
}

int wait_ddrphy_training_complete(void)
{
	unsigned int mail;

	while (1) {
		mail = get_mail();
		decode_major_message(mail);
		if (mail == 0x08) {
			decode_streaming_message();
		} else if (mail == 0x07) {
			debug("Training PASS\n");
			return 0;
		} else if (mail == 0xff) {
			printf("Training FAILED\n");
			return -1;
		}
	}
}

/* A single deadline bounds both mailbox handshakes and streaming messages. */
static int get_pmu_message_timeout(ulong start, bool stream, u32 *message)
{
	u32 reg;

	while (dwc_ddrphy_apb_rd(0xd0004) & 1)
		if (get_timer(start) >= 500)
			return -ETIMEDOUT;
	if (get_timer(start) >= 500)
		return -ETIMEDOUT;

	reg = dwc_ddrphy_apb_rd(0xd0032);
	if (stream)
		reg |= dwc_ddrphy_apb_rd(0xd0034) << 16;
	*message = reg;
	dwc_ddrphy_apb_wr(0xd0031, 0);
	while (!(dwc_ddrphy_apb_rd(0xd0004) & 1)) {
		if (get_timer(start) >= 500) {
			dwc_ddrphy_apb_wr(0xd0031, 1);
			return -ETIMEDOUT;
		}
	}
	dwc_ddrphy_apb_wr(0xd0031, 1);
	return 0;
}

int wait_ddrphy_init_complete_timeout(void)
{
	ulong start = get_timer(0);
	u32 mail, count, arg;
	int ret;

	for (;;) {
		ret = get_pmu_message_timeout(start, false, &mail);
		if (ret)
			return ret;
		if (mail == 0x07)
			return 0;
		if (mail == 0xff)
			return -EIO;
		if (mail != 0x08)
			continue;
		ret = get_pmu_message_timeout(start, true, &count);
		if (ret)
			return ret;
		count &= 0xffff;
		while (count--) {
			ret = get_pmu_message_timeout(start, true, &arg);
			if (ret)
				return ret;
		}
	}
}

void ddrphy_init_set_dfi_clk(unsigned int drate)
{
	switch (drate) {
	case 4000:
		dram_pll_init(MHZ(1000));
		dram_disable_bypass();
		break;
	case 3733:
	case 3732:
		dram_pll_init(MHZ(933));
		dram_disable_bypass();
		break;
	case 3200:
		dram_pll_init(MHZ(800));
		dram_disable_bypass();
		break;
	case 3000:
		dram_pll_init(MHZ(750));
		dram_disable_bypass();
		break;
	case 2800:
		dram_pll_init(MHZ(700));
		dram_disable_bypass();
		break;
	case 2400:
		dram_pll_init(MHZ(600));
		dram_disable_bypass();
		break;
	case 1866:
		dram_pll_init(MHZ(466));
		dram_disable_bypass();
		break;
	case 1600:
		dram_pll_init(MHZ(400));
		dram_disable_bypass();
		break;
	case 1066:
		dram_pll_init(MHZ(266));
		dram_disable_bypass();
		break;
	case 667:
		dram_pll_init(MHZ(167));
		dram_disable_bypass();
		break;
	case 625:
		dram_enable_bypass(MHZ(625));
		break;
	case 400:
		dram_enable_bypass(MHZ(400));
		break;
	case 333:
		dram_enable_bypass(MHZ(333));
		break;
	case 200:
		dram_enable_bypass(MHZ(200));
		break;
	case 100:
		dram_enable_bypass(MHZ(100));
		break;
	default:
		return;
	}
}

void ddrphy_init_read_msg_block(enum fw_type type)
{
}
