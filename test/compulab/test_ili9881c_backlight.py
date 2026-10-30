#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
"""Check the production ILI9881C backlight lifecycle with mocked devices.

Run with python3 test/compulab/test_ili9881c_backlight.py.
This host check covers optional backlights and error propagation; it does
not validate a physical panel or its DSI initialization sequence.
"""

from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'drivers/video/startek-ili9881c.c').read_text()


def function(name):
    start = source.index('static int ' + name + '(')
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


prefix = r'''
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
struct gpio_desc { int value; };
struct udevice { int unused; };
struct mipi_dsi_device { int unused; };
struct mipi_dsi_panel_plat { struct mipi_dsi_device *device; };
struct ili9881c_panel_priv {
 struct gpio_desc reset;
 struct udevice *backlight;
 unsigned int lanes, format, mode_flags;
};
static struct ili9881c_panel_priv priv;
static struct mipi_dsi_panel_plat plat;
static struct udevice panel, light;
static int property_error, attach_error, panel_error, light_error;
static int attaches, enables, lights, brightness, shutdowns, resets;
static void *dev_get_priv(struct udevice *dev) { (void)dev; return &priv; }
static void *dev_get_plat(struct udevice *dev) { (void)dev; return &plat; }
static int mipi_dsi_attach(struct mipi_dsi_device *dev) {
 (void)dev; attaches++; return attach_error;
}
static int ili9881c_enable(struct udevice *dev) {
 (void)dev; assert(attaches == 1); enables++; return panel_error;
}
static int video_link_shut_down(void) { shutdowns++; return 0; }
static int backlight_enable(struct udevice *dev) {
 assert(dev == &light && enables == 1); lights++; return light_error;
}
static int backlight_set_brightness(struct udevice *dev, int percent) {
 assert(dev == &light); brightness = percent; return light_error;
}
#define UCLASS_PANEL_BACKLIGHT 1
static int uclass_get_device_by_phandle(int id, struct udevice *dev,
 const char *name, struct udevice **result) {
 assert(id == UCLASS_PANEL_BACKLIGHT && dev == &panel);
 assert(!strcmp(name, "backlight"));
 if (!property_error) *result = &light;
 return property_error;
}
#define MIPI_DSI_FMT_RGB888 1
#define MIPI_DSI_MODE_VIDEO_SYNC_PULSE 2
#define MIPI_DSI_MODE_VIDEO 4
#define GPIOD_IS_OUT 1
#define dev_warn(...) ((void)0)
static int dev_read_u32(struct udevice *dev, const char *name, unsigned int *out) {
 (void)dev; assert(!strcmp(name, "dsi-lanes")); *out = 4; return 0;
}
static int gpio_request_by_name(struct udevice *dev, const char *name,
 int index, struct gpio_desc *gpio, int flags) {
 (void)dev; (void)name; (void)index; (void)gpio; (void)flags; return 0;
}
static int dm_gpio_set_value(struct gpio_desc *gpio, int value) {
 gpio->value = value; resets++; return 0;
}
static void mdelay(int ms) { (void)ms; }
static void clear(void) {
 memset(&priv, 0, sizeof(priv));
 property_error = attach_error = panel_error = light_error = 0;
 attaches = enables = lights = shutdowns = resets = 0;
 brightness = -1;
}
'''

main = r'''
int main(void) {
 clear();
 assert(!ili9881c_panel_probe(&panel));
 assert(priv.backlight == &light && priv.lanes == 4 && resets == 2);
 assert(!ili9881c_panel_enable_backlight(&panel));
 assert(attaches == 1 && enables == 1 && lights == 1);
 assert(!ili9881c_panel_set_backlight(&panel, 35) && brightness == 35);
 assert(!ili9881c_panel_disable(&panel));
 assert(brightness == 0 && priv.reset.value == 1);

 clear(); property_error = -ENOENT;
 assert(!ili9881c_panel_probe(&panel) && !priv.backlight);
 assert(!ili9881c_panel_enable_backlight(&panel) && lights == 0);
 assert(!ili9881c_panel_set_backlight(&panel, 35) && brightness == -1);
 assert(!ili9881c_panel_disable(&panel) && priv.reset.value == 1);

 clear(); property_error = -EIO;
 assert(ili9881c_panel_probe(&panel) == -EIO && resets == 0);

 clear(); priv.backlight = &light; attach_error = -EIO;
 assert(ili9881c_panel_enable_backlight(&panel) == -EIO);
 assert(enables == 0 && lights == 0);

 clear(); priv.backlight = &light; panel_error = -ETIMEDOUT;
 assert(ili9881c_panel_enable_backlight(&panel) == -ETIMEDOUT);
 assert(lights == 0 && shutdowns == 1);

 clear(); priv.backlight = &light; light_error = -EIO;
 assert(ili9881c_panel_enable_backlight(&panel) == -EIO);
 assert(ili9881c_panel_set_backlight(&panel, 35) == -EIO);
 assert(ili9881c_panel_disable(&panel) == -EIO);
 puts("ILI9881C backlight lifecycle and failure checks passed");
 return 0;
}
'''

names = ['ili9881c_panel_enable_backlight', 'ili9881c_panel_set_backlight',
         'ili9881c_panel_probe', 'ili9881c_panel_disable']
with tempfile.TemporaryDirectory(prefix='ili9881c-backlight-') as directory:
    directory = Path(directory)
    harness = directory / 'test.c'
    harness.write_text(prefix + ''.join(function(name) for name in names) + main)
    binary = directory / 'test'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-fno-pie', '-no-pie', str(harness), '-o', str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True)
