// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2023 NXP
 */
#include <common.h>
#include <clk.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <display.h>
#include <panel.h>
#include <video.h>
#include <video_bridge.h>
#include <video_link.h>
#include <asm/io.h>
#include <dm/device-internal.h>
#include <linux/iopoll.h>
#include <linux/err.h>
#include <clk.h>

#include <power-domain.h>
#include <regmap.h>
#include <syscon.h>

#define DRIVER_NAME "imx93-ldb"


#define LVDS_CTRL		0x24
#define HS_DISABLE		(0 << 3)
#define SPARE_IN(n)		(((n) & 0x7) << 25)
#define SPARE_IN_MASK		0xe000000
#define TEST_RANDOM_NUM_EN	BIT(24)
#define TEST_MUX_SRC(n)		(((n) & 0x3) << 22)
#define TEST_MUX_SRC_MASK	0xc00000
#define TEST_EN			BIT(21)
#define TEST_DIV4_EN		BIT(20)
#define VBG_ADJ(n)		(((n) & 0x7) << 17)
#define VBG_ADJ_MASK		0xe0000
#define SLEW_ADJ(n)		(((n) & 0x7) << 14)
#define SLEW_ADJ_MASK		0x1c000
#define CC_ADJ(n)		(((n) & 0x7) << 11)
#define CC_ADJ_MASK		0x3800
#define CM_ADJ(n)		(((n) & 0x7) << 8)
#define CM_ADJ_MASK		0x700
#define PRE_EMPH_ADJ(n)		(((n) & 0x7) << 5)
#define PRE_EMPH_ADJ_MASK	0xe0
#define PRE_EMPH_EN		BIT(4)
#define HS_EN			BIT(3)
#define BG_EN			BIT(2)
#define CH_EN		    BIT(0)


#define LDB_CTRL		0x20
#define LDB_CH0_MODE_EN_TO_DI0		(1 << 0)
#define LDB_CH0_MODE_EN_TO_DI1		(3 << 0)
#define LDB_CH0_MODE_EN_MASK		(3 << 0)
#define LDB_CH1_MODE_EN_TO_DI0		(1 << 2)
#define LDB_CH1_MODE_EN_TO_DI1		(3 << 2)
#define LDB_CH1_MODE_EN_MASK		(3 << 2)
#define CH0_DATA_WIDTH_24BIT		(1 << 5)
#define CH0_BIT_MAPPING_JEIDA		(1 << 6)
#define CH0_BIT_MAPPING_SPWG		(0 << 6)
#define LDB_REG_CH0_FIFO_RESET		(1 << 11)
#define LDB_REG_CH1_FIFO_RESET		(1 << 12)
#define LDB_REG_ASYNC_FIFO_EN		(1 << 24)
#define LDB_FIFO_THRESHOLD		(4 << 25)

#define CLK_EN 0x4

#define usleep_range(a, b) udelay((b))
#define msleep(a)	udelay(a * 1000)

struct imx93_ldb_priv {
	struct regmap *regmap;
	struct udevice *conn_dev;
	unsigned int ldb_id;
	struct clk ldb_root_clk;
	struct clk apb_root_clk;
	struct display_timing timings;
	/*void __iomem *base;*/
};

static inline unsigned int media_blk_read(struct imx93_ldb_priv *priv, unsigned int reg)
{
	unsigned int val;
	regmap_read(priv->regmap, reg, &val);
	return val;
}

static inline void media_blk_write(struct imx93_ldb_priv *priv, unsigned int reg, unsigned int value)
{
	regmap_write(priv->regmap, reg, value);
}

static int imx93_lvds_phy_power_on(struct udevice *dev)
{
	struct imx93_ldb_priv *priv = dev_get_priv(dev);

	unsigned int lvds_val, ldb_val;
	bool bg_en;
	debug("%s %d\n", __func__,__LINE__);

	media_blk_write(priv, LVDS_CTRL, HS_DISABLE);

	lvds_val = media_blk_read(priv, LVDS_CTRL);
	bg_en = !!(lvds_val & BG_EN);
	lvds_val |= BG_EN;

	media_blk_write(priv, LVDS_CTRL, lvds_val);

	if (!bg_en){
		usleep_range(15, 20);
	}

	lvds_val = media_blk_read(priv, LVDS_CTRL);
	lvds_val |= CH_EN | CC_ADJ(0x2) | PRE_EMPH_EN | PRE_EMPH_ADJ(0x3);
	media_blk_write(priv, LVDS_CTRL, lvds_val);

	/* read final LVDS_CTRL value for debug*/
	lvds_val = media_blk_read(priv, LVDS_CTRL);
	debug("read lvds_val: 0x%d\n",lvds_val);

	/* LDB_CTRL */
	ldb_val = media_blk_read(priv, LDB_CTRL);
	ldb_val &= ~LDB_CH0_MODE_EN_MASK;
	ldb_val |= LDB_CH0_MODE_EN_TO_DI0 | CH0_DATA_WIDTH_24BIT;
	media_blk_write(priv, LDB_CTRL, ldb_val);

	/* read final LDB_CTRL value for debug*/
	ldb_val = media_blk_read(priv, LDB_CTRL);
	debug("read ldb_val: 0x%d\n",ldb_val);

	usleep_range(5, 10);

	debug("%s %d\n", __func__,__LINE__);

	return 0;

}

int imx93_ldb_read_timing(struct udevice *dev, struct display_timing *timing)
{
	struct imx93_ldb_priv *priv = dev_get_priv(dev);

	if (dev->plat_ == NULL)
		return -EINVAL;

	if (timing) {
		memcpy(timing, &priv->timings, sizeof(struct display_timing));
		return 0;
	}

	return -EINVAL;
}

static int imx93_ldb_probe(struct udevice *dev)
{
	struct imx93_ldb_priv *priv = dev_get_priv(dev);
	int ret;

	debug("%s %d\n", __func__,__LINE__);

	if (dev->plat_ == NULL) {
		priv->regmap = syscon_regmap_lookup_by_phandle(dev, "gpr");
		if (IS_ERR(priv->regmap)) {
			dev_err(dev, "fail to get fsl,imx93-mediamix-blk-ctl regmap\n");
			return PTR_ERR(priv->regmap);
		}
		/* Require to add alias in DTB */
		priv->ldb_id = dev_seq(dev);

		debug("ldb_id %u\n", priv->ldb_id);

	} else {

		priv->conn_dev = video_link_get_next_device(dev);
		if (!priv->conn_dev) {
			debug("can't find next device in video link\n");
		}

		ret = video_link_get_display_timings(&priv->timings);
		if (ret) {
			dev_err(dev, "decode display timing error %d\n", ret);
			return ret;
		}

		if (priv->conn_dev && device_get_uclass_id(priv->conn_dev) == UCLASS_PANEL){
			ret = panel_enable_backlight(priv->conn_dev);
			if (ret) {
					dev_err(dev, "fail to enable panel backlight\n");
					return ret;
			}
			ret = panel_set_backlight(priv->conn_dev, 80);
				if (ret) {
					dev_err(dev, "fail to set panel backlight\n");
					return ret;
			}

		}

		if (IS_ENABLED(CONFIG_VIDEO_BRIDGE)) {
			if (priv->conn_dev &&
				device_get_uclass_id(priv->conn_dev) == UCLASS_VIDEO_BRIDGE) {

				ret = video_bridge_attach(priv->conn_dev);
				if (ret) {
					dev_err(dev, "fail to attach bridge\n");
					return ret;
				}
				ret = video_bridge_set_active(priv->conn_dev, true);
				if (ret) {
					dev_err(dev, "fail to active bridge\n");
					return ret;
				}
			}
		}
	}

	return 0;

}

static int imx93_ldb_bind(struct udevice *dev)
{
	struct imx93_ldb_priv __maybe_unused *priv = dev_get_priv(dev);
	ofnode lvds_ch_node;
	int ret = 0;

	debug("%s %d\n", __func__,__LINE__);

	lvds_ch_node = ofnode_find_subnode(dev_ofnode(dev), "lvds-channel@0");
	if (ofnode_valid(lvds_ch_node)) {
		ret = device_bind(dev, dev->driver, "lvds-channel@0", (void *)1,
			lvds_ch_node, NULL);
		if (ret)
			debug("Error binding driver '%s': %d\n", dev->driver->name,
				ret);
	}

	return ret;
}


int imx93_ldb_enable(struct udevice *dev, int panel_bpp,
		      const struct display_timing *timing)
{
	struct imx93_ldb_priv *priv = dev_get_priv(dev);
	int ret;

	debug("%s %d\n", __func__,__LINE__);

	if (dev->plat_ == NULL) {
		imx93_lvds_phy_power_on(dev);
	} else {
		display_enable(dev->parent, panel_bpp, &priv->timings);

		if (IS_ENABLED(CONFIG_VIDEO_BRIDGE)) {
			if (priv->conn_dev &&
				device_get_uclass_id(priv->conn_dev) == UCLASS_VIDEO_BRIDGE) {
				ret = video_bridge_set_backlight(priv->conn_dev, 80);
				if (ret) {
					dev_err(dev, "fail to set backlight\n");
					return ret;
				}
			}
		}
	}

	return 0;

}

struct dm_display_ops imx93_ldb_ops = {
	.read_timing = imx93_ldb_read_timing,
	.enable = imx93_ldb_enable,
};

static const struct udevice_id imx93_ldb_ids[] = {
	{ .compatible = "fsl,imx93-ldb" },
	{ }
};

U_BOOT_DRIVER(imx93_ldb) = {
	.name				= "imx93_ldb",
	.id				= UCLASS_DISPLAY,
	.of_match			= imx93_ldb_ids,
	.bind				= imx93_ldb_bind,
	.probe				= imx93_ldb_probe,
	.ops				= &imx93_ldb_ops,
	.priv_auto		= sizeof(struct imx93_ldb_priv),
	.flags 			= DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
