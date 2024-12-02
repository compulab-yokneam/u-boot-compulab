// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2016 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <backlight.h>

struct fake_backlight_priv {
	bool enabled;
};


static int fake_backlight_enable(struct udevice *dev)
{
	return 0;
}

static int fake_backlight_set_brightness(struct udevice *dev, int percent)
{
	return 0;
}


static int fake_backlight_probe(struct udevice *dev)
{
	return 0;
}

static int fake_backlight_of_to_plat(struct udevice *dev)
{
	return 0;
}

static const struct backlight_ops fake_backlight_ops = {
	.enable		= fake_backlight_enable,
	.set_brightness	= fake_backlight_set_brightness,
};

static const struct udevice_id fake_backlight_ids[] = {
	{ .compatible = "clab,fake-backlight" },
	{ }
};

U_BOOT_DRIVER(fake_backlight) = {
	.name	= "fake_backlight",
	.id	= UCLASS_PANEL_BACKLIGHT,
	.of_match = fake_backlight_ids,
	.ops	= &fake_backlight_ops,
	.of_to_plat	= fake_backlight_of_to_plat,
	.probe		= fake_backlight_probe,
	.priv_auto	= sizeof(struct fake_backlight_priv),
};
