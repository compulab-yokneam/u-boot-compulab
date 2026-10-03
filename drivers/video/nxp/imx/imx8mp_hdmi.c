// SPDX-License-Identifier: GPL-2.0+
/*
 * i.MX8MP HDMI display support
 *
 * Copyright 2026 CompuLab Ltd.
 * Copyright 2020 NXP
 * Copyright 2022 Pengutronix, Lucas Stach <kernel@pengutronix.de>
 */

#include <common.h>
#include <clk.h>
#include <display.h>
#include <dm.h>
#include <dm/read.h>
#include <dw_hdmi.h>
#include <edid.h>
#include <env.h>
#include <media_bus_format.h>
#include <power-domain.h>
#include <video_link.h>
#include <asm/io.h>
#include <linux/bitfield.h>
#include <linux/delay.h>

#define HDMI_RTX_RESET_CTL0		0x20
#define HDMI_RTX_CLK_CTL0		0x40
#define HDMI_RTX_CLK_CTL1		0x50
#define HDMI_TX_CONTROL0		0x200

#define HDMI_LCDIF_NOC_HURRY_MASK	GENMASK(14, 12)

#define HTX_PVI_CTRL			0x0
#define PVI_CTRL_OP_VSYNC_POL		BIT(18)
#define PVI_CTRL_OP_HSYNC_POL		BIT(17)
#define PVI_CTRL_OP_DE_POL		BIT(16)
#define PVI_CTRL_INP_VSYNC_POL		BIT(14)
#define PVI_CTRL_INP_HSYNC_POL		BIT(13)
#define PVI_CTRL_INP_DE_POL		BIT(12)
#define PVI_CTRL_MODE_MASK		GENMASK(2, 1)
#define PVI_CTRL_MODE_LCDIF		2
#define PVI_CTRL_EN			BIT(0)

#define PHY_REG(reg)			((reg) * 4)
#define PHY_REG12_CK_DIV_MASK		GENMASK(5, 4)
#define PHY_REG14_TOL_MASK		GENMASK(7, 4)
#define PHY_REG14_RP_CODE_MASK		GENMASK(3, 1)
#define PHY_REG14_TG_CODE_HIGH_MASK	BIT(0)
#define PHY_REG21_SEL_TX_CK_INV		BIT(7)
#define PHY_REG21_PMS_S_MASK		GENMASK(3, 0)
#define PHY_REG33_MODE_SET_DONE		BIT(7)
#define PHY_REG33_FIX_DA		BIT(1)
#define PHY_REG34_PLL_LOCK		BIT(6)

#define HDMI_PHY_PLL_REGS		7
#define HDMI_PHY_MIN_PIXEL_CLOCK	22250000
#define HDMI_PHY_MAX_PIXEL_CLOCK	297000000

#define HDMI_CEA_VIC_720P60		4
#define HDMI_CEA_VIC_1080P60		16
#define HDMI_TIMING_720P60		0
#define HDMI_TIMING_1080P60		1

struct imx8mp_hdmi_phy_cfg {
	u32 pixelclock;
	u8 pll[HDMI_PHY_PLL_REGS];
};

/* Samsung PHY fractional-divider settings for the boot display modes. */
static const struct imx8mp_hdmi_phy_cfg imx8mp_hdmi_phy_cfgs[] = {
	{ 25175000, { 0xd1, 0x54, 0xfc, 0xcc, 0x91, 0x80, 0x40 } },
	{ 27027000, { 0xd1, 0x5a, 0xf2, 0xfd, 0x0c, 0x80, 0x40 } },
	{ 74176000, { 0xd1, 0x5d, 0x58, 0xdb, 0xa2, 0x88, 0x41 } },
	{ 74250000, { 0xd1, 0x5c, 0x52, 0x90, 0x0d, 0x84, 0x41 } },
	{ 148352000, { 0xd1, 0x7b, 0x35, 0xdb, 0x39, 0x90, 0x45 } },
	{ 148500000, { 0xd1, 0x7b, 0x35, 0x84, 0x03, 0x90, 0x45 } },
};

struct imx8mp_hdmi_reg_cfg {
	u8 reg;
	u8 val;
};

static const struct imx8mp_hdmi_reg_cfg imx8mp_hdmi_phy_common_cfg[] = {
	{ 0, 0x00 }, { 8, 0x4f }, { 9, 0x30 },
	{ 10, 0x33 }, { 11, 0x65 }, { 15, 0x80 },
	{ 16, 0x6c }, { 17, 0xf2 }, { 18, 0x67 },
	{ 19, 0x00 }, { 20, 0x10 }, { 22, 0x30 },
	{ 23, 0x32 }, { 24, 0x60 }, { 25, 0x8f },
	{ 26, 0x00 }, { 27, 0x00 }, { 28, 0x08 },
	{ 29, 0x00 }, { 30, 0x00 }, { 31, 0x00 },
	{ 32, 0x00 }, { 33, 0x80 }, { 34, 0x00 },
	{ 35, 0x00 }, { 36, 0x00 }, { 37, 0x00 },
	{ 38, 0x00 }, { 39, 0x00 }, { 40, 0x00 },
	{ 41, 0xe0 }, { 42, 0x83 }, { 43, 0x0f },
	{ 44, 0x3e }, { 45, 0xf8 }, { 46, 0x00 },
	{ 47, 0x00 },
};

struct imx8mp_hdmi_priv {
	struct dw_hdmi hdmi;
	struct clk_bulk clocks;
	struct power_domain hdmimix;
	struct power_domain phy_pd;
	void __iomem *phy;
	void __iomem *pvi;
	void __iomem *blk;
};

struct imx8mp_hdmi_mode_match {
	u32 hactive;
	u32 vactive;
	u32 pixelclock;
};

static const struct imx8mp_hdmi_phy_cfg *
imx8mp_hdmi_find_phy_cfg(uint pixelclock)
{
	const struct imx8mp_hdmi_phy_cfg *best = NULL;
	ulong best_delta = ULONG_MAX;
	int i;

	if (pixelclock < HDMI_PHY_MIN_PIXEL_CLOCK ||
	    pixelclock > HDMI_PHY_MAX_PIXEL_CLOCK)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(imx8mp_hdmi_phy_cfgs); i++) {
		ulong delta = abs((long)pixelclock -
				  (long)imx8mp_hdmi_phy_cfgs[i].pixelclock);

		if (delta < best_delta) {
			best_delta = delta;
			best = &imx8mp_hdmi_phy_cfgs[i];
		}
	}

	/* CEA modes permit a 0.5 percent pixel-clock tolerance. */
	if (!best || best_delta > pixelclock / 200)
		return NULL;

	return best;
}

static int imx8mp_hdmi_phy_lock_detector(struct imx8mp_hdmi_priv *priv,
					 const struct imx8mp_hdmi_phy_cfg *cfg)
{
	u32 int_pllclk, target;
	u8 div;

	for (div = 0; div < 4; div++) {
		int_pllclk = cfg->pixelclock / BIT(div);
		if (int_pllclk < 50000000)
			break;
	}

	if (div == 4)
		return -EINVAL;

	target = DIV_ROUND_UP(24000000ULL * 256, int_pllclk);
	writeb(FIELD_PREP(PHY_REG12_CK_DIV_MASK, div),
	       priv->phy + PHY_REG(12));
	writeb(target & 0xff, priv->phy + PHY_REG(13));
	writeb(FIELD_PREP(PHY_REG14_TOL_MASK, 2) |
	       FIELD_PREP(PHY_REG14_RP_CODE_MASK, 2) |
	       FIELD_PREP(PHY_REG14_TG_CODE_HIGH_MASK, target >> 8),
	       priv->phy + PHY_REG(14));

	return 0;
}

static int imx8mp_hdmi_phy_set(struct dw_hdmi *hdmi, uint pixelclock)
{
	struct imx8mp_hdmi_priv *priv =
		container_of(hdmi, struct imx8mp_hdmi_priv, hdmi);
	const struct imx8mp_hdmi_phy_cfg *cfg;
	ulong start;
	int i, ret;

	cfg = imx8mp_hdmi_find_phy_cfg(pixelclock);
	if (!cfg)
		return -EINVAL;

	writeb(PHY_REG33_FIX_DA, priv->phy + PHY_REG(33));

	for (i = 0; i < ARRAY_SIZE(imx8mp_hdmi_phy_common_cfg); i++)
		writeb(imx8mp_hdmi_phy_common_cfg[i].val,
		       priv->phy + PHY_REG(imx8mp_hdmi_phy_common_cfg[i].reg));

	for (i = 0; i < HDMI_PHY_PLL_REGS; i++)
		writeb(cfg->pll[i], priv->phy + PHY_REG(1 + i));

	writeb(PHY_REG21_SEL_TX_CK_INV |
	       FIELD_PREP(PHY_REG21_PMS_S_MASK, cfg->pll[2] >> 4),
	       priv->phy + PHY_REG(21));

	ret = imx8mp_hdmi_phy_lock_detector(priv, cfg);
	if (ret)
		return ret;

	writeb(PHY_REG33_FIX_DA | PHY_REG33_MODE_SET_DONE,
	       priv->phy + PHY_REG(33));

	start = get_timer(0);
	do {
		if (readb(priv->phy + PHY_REG(34)) & PHY_REG34_PLL_LOCK)
			return 0;
		udelay(50);
	} while (get_timer(start) < 20);

	return -ETIMEDOUT;
}

static void imx8mp_hdmi_block_enable(struct imx8mp_hdmi_priv *priv)
{
	setbits_le32(priv->blk + HDMI_RTX_CLK_CTL0,
		     BIT(2) | BIT(4) | BIT(5) | BIT(7) |
		     BIT(16) | BIT(17) | BIT(18) | BIT(19) | BIT(20));
	setbits_le32(priv->blk + HDMI_RTX_CLK_CTL1,
		     BIT(11) | BIT(12) | BIT(13) | BIT(14) | BIT(15) |
		     BIT(16) | BIT(17) | BIT(18) | BIT(19) | BIT(20) |
		     BIT(21) | BIT(22) | BIT(24) | BIT(28));
	setbits_le32(priv->blk + HDMI_RTX_RESET_CTL0,
		     BIT(4) | BIT(5) | BIT(6) | BIT(7) | BIT(10) |
		     BIT(11) | BIT(12) | BIT(22));
	setbits_le32(priv->blk + HDMI_TX_CONTROL0,
		     BIT(1) | FIELD_PREP(HDMI_LCDIF_NOC_HURRY_MASK, 7));
	clrbits_le32(priv->blk + HDMI_TX_CONTROL0, BIT(3));
}

static int imx8mp_hdmi_power_on(struct udevice *dev)
{
	struct imx8mp_hdmi_priv *priv = dev_get_priv(dev);
	int ret;

	writel(0, priv->blk + HDMI_RTX_RESET_CTL0);
	writel(0xffffffff, priv->blk + HDMI_RTX_CLK_CTL0);
	writel(0x7ffff87e, priv->blk + HDMI_RTX_CLK_CTL1);

	ret = power_domain_on(&priv->hdmimix);
	if (ret)
		return ret;

	udelay(20);
	writel(0, priv->blk + HDMI_RTX_CLK_CTL0);
	writel(0, priv->blk + HDMI_RTX_CLK_CTL1);
	writel(0xffffffff, priv->blk + HDMI_RTX_RESET_CTL0);
	writel(0xffffffff, priv->blk + HDMI_RTX_CLK_CTL0);
	writel(0x7ffff87e, priv->blk + HDMI_RTX_CLK_CTL1);
	imx8mp_hdmi_block_enable(priv);

	ret = power_domain_on(&priv->phy_pd);
	if (ret)
		return ret;

	return 0;
}

static int imx8mp_hdmi_read_edid(struct udevice *dev, u8 *buf, int size)
{
	struct imx8mp_hdmi_priv *priv = dev_get_priv(dev);

	return dw_hdmi_read_edid(&priv->hdmi, buf, size);
}

static bool imx8mp_hdmi_edid_mode_valid(void *data,
					const struct display_timing *timing)
{
	const struct imx8mp_hdmi_mode_match *mode = data;
	ulong delta;

	if (timing->flags & DISPLAY_FLAGS_INTERLACED)
		return false;
	if (timing->hactive.typ != mode->hactive ||
	    timing->vactive.typ != mode->vactive)
		return false;

	delta = abs((long)timing->pixelclock.typ - (long)mode->pixelclock);
	return delta <= mode->pixelclock / 200;
}

static bool imx8mp_hdmi_edid_has_detailed_mode(u8 *edid, int size,
					       u32 hactive, u32 vactive,
					       u32 pixelclock)
{
	struct imx8mp_hdmi_mode_match mode = {
		.hactive = hactive,
		.vactive = vactive,
		.pixelclock = pixelclock,
	};
	struct display_timing timing;
	int bpc;

	return !edid_get_timing_validate(edid, size, &timing, &bpc,
					 imx8mp_hdmi_edid_mode_valid, &mode);
}

static bool imx8mp_hdmi_edid_has_vic(u8 *edid, int size, u8 vic)
{
	struct edid_cea861_info *info;
	u8 end, i = 0;

	if (size < EDID_EXT_SIZE)
		return false;

	info = (struct edid_cea861_info *)(edid + EDID_SIZE);
	if (info->extension_tag != EDID_CEA861_EXTENSION_TAG)
		return false;

	end = info->dtd_offset;
	if (!end)
		end = sizeof(info->data);
	if (end < 4 || end > sizeof(info->data))
		return false;
	end -= 4;

	while (i < end) {
		u8 len = EDID_CEA861_DB_LEN(*info, i);
		u8 j;

		if (i + len >= end)
			break;
		if (EDID_CEA861_DB_TYPE(*info, i) == EDID_CEA861_DB_VIDEO) {
			for (j = 1; j <= len; j++)
				if ((info->data[i + j] & 0x7f) == vic)
					return true;
		}
		i += len + 1;
	}

	return false;
}

static int imx8mp_hdmi_auto_timing(struct udevice *dev)
{
	u8 edid[EDID_EXT_SIZE];
	int size;

	size = imx8mp_hdmi_read_edid(dev, edid, sizeof(edid));
	if (size < EDID_SIZE || edid_check_info((struct edid1_info *)edid) ||
	    edid_check_checksum(edid))
		goto fallback;

	if (size >= EDID_EXT_SIZE && edid_check_checksum(edid + EDID_SIZE))
		size = EDID_SIZE;

	if (imx8mp_hdmi_edid_has_vic(edid, size, HDMI_CEA_VIC_1080P60) ||
	    imx8mp_hdmi_edid_has_detailed_mode(edid, size, 1920, 1080,
					       148500000))
		return HDMI_TIMING_1080P60;

	if (imx8mp_hdmi_edid_has_vic(edid, size, HDMI_CEA_VIC_720P60) ||
	    imx8mp_hdmi_edid_has_detailed_mode(edid, size, 1280, 720,
					       74250000))
		return HDMI_TIMING_720P60;

fallback:
	printf("HDMI: EDID has no supported mode, using 720p60\n");
	return HDMI_TIMING_720P60;
}

static int imx8mp_hdmi_read_timing(struct udevice *dev,
				   struct display_timing *timing)
{
	const char *mode = env_get("hdmi_mode");
	int index = HDMI_TIMING_720P60;

	if (!mode || !strcmp(mode, "720p60"))
		index = HDMI_TIMING_720P60;
	else if (!strcmp(mode, "1080p60"))
		index = HDMI_TIMING_1080P60;
	else if (!strcmp(mode, "auto"))
		index = imx8mp_hdmi_auto_timing(dev);
	else
		printf("HDMI: invalid hdmi_mode '%s', using 720p60\n", mode);

	return ofnode_decode_display_timing(dev_ofnode(dev), index, timing);
}

static bool imx8mp_hdmi_mode_valid(struct udevice *dev,
				   const struct display_timing *timing)
{
	bool supported_resolution;

	supported_resolution =
		(timing->hactive.typ == 1280 && timing->vactive.typ == 720) ||
		(timing->hactive.typ == 1920 && timing->vactive.typ == 1080);

	return supported_resolution &&
	       !(timing->flags & DISPLAY_FLAGS_INTERLACED) &&
	       !!imx8mp_hdmi_find_phy_cfg(timing->pixelclock.typ);
}

static int imx8mp_hdmi_enable(struct udevice *dev, int panel_bpp,
			      const struct display_timing *timing)
{
	struct imx8mp_hdmi_priv *priv = dev_get_priv(dev);
	struct display_timing hdmi_timing = *timing;
	u32 val;

	val = FIELD_PREP(PVI_CTRL_MODE_MASK, PVI_CTRL_MODE_LCDIF) |
	      PVI_CTRL_OP_DE_POL | PVI_CTRL_INP_DE_POL | PVI_CTRL_EN;
	if (timing->flags & DISPLAY_FLAGS_VSYNC_HIGH)
		val |= PVI_CTRL_OP_VSYNC_POL | PVI_CTRL_INP_VSYNC_POL;
	if (timing->flags & DISPLAY_FLAGS_HSYNC_HIGH)
		val |= PVI_CTRL_OP_HSYNC_POL | PVI_CTRL_INP_HSYNC_POL;
	writel(val, priv->pvi + HTX_PVI_CTRL);

	hdmi_timing.hdmi_monitor = true;
	return dw_hdmi_enable(&priv->hdmi, &hdmi_timing);
}

static int imx8mp_hdmi_probe(struct udevice *dev)
{
	struct imx8mp_hdmi_priv *priv = dev_get_priv(dev);
	int ret;

	priv->hdmi.ioaddr = (ulong)dev_remap_addr_index(dev, 0);
	priv->phy = dev_remap_addr_index(dev, 1);
	priv->pvi = dev_remap_addr_index(dev, 2);
	priv->blk = dev_remap_addr_index(dev, 3);
	if (!priv->hdmi.ioaddr || !priv->phy || !priv->pvi || !priv->blk)
		return -EINVAL;

	ret = clk_get_bulk(dev, &priv->clocks);
	if (ret)
		return ret;
	ret = clk_enable_bulk(&priv->clocks);
	if (ret)
		return ret;

	ret = power_domain_get_by_index(dev, &priv->hdmimix, 0);
	if (ret)
		return ret;
	ret = power_domain_get_by_index(dev, &priv->phy_pd, 1);
	if (ret)
		return ret;

	ret = imx8mp_hdmi_power_on(dev);
	if (ret)
		return ret;

	priv->hdmi.reg_io_width = 1;
	priv->hdmi.i2c_clk_high = 0x67;
	priv->hdmi.i2c_clk_low = 0x78;
	priv->hdmi.hdmi_data.enc_in_bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	priv->hdmi.hdmi_data.enc_out_bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	priv->hdmi.phy_set = imx8mp_hdmi_phy_set;

	dw_hdmi_init(&priv->hdmi);

	/* The external Samsung PHY uses the DesignWare Gen1 reset polarity. */
	writeb(0, priv->hdmi.ioaddr + HDMI_MC_PHYRSTZ);
	writeb(HDMI_MC_PHYRSTZ_DEASSERT,
	       priv->hdmi.ioaddr + HDMI_MC_PHYRSTZ);
	dw_hdmi_phy_init(&priv->hdmi);

	ret = dw_hdmi_phy_wait_for_hpd(&priv->hdmi);
	if (ret)
		debug("no HDMI hot-plug signal\n");

	return 0;
}

static const struct dm_display_ops imx8mp_hdmi_ops = {
	.read_timing = imx8mp_hdmi_read_timing,
	.read_edid = imx8mp_hdmi_read_edid,
	.enable = imx8mp_hdmi_enable,
	.mode_valid = imx8mp_hdmi_mode_valid,
};

static const struct udevice_id imx8mp_hdmi_ids[] = {
	{ .compatible = "fsl,imx8mp-hdmi" },
	{ }
};

U_BOOT_DRIVER(imx8mp_hdmi) = {
	.name = "imx8mp_hdmi",
	.id = UCLASS_DISPLAY,
	.of_match = imx8mp_hdmi_ids,
	.probe = imx8mp_hdmi_probe,
	.ops = &imx8mp_hdmi_ops,
	.priv_auto = sizeof(struct imx8mp_hdmi_priv),
	.flags = DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
