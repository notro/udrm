/*
 * Copyright (C) 2013 Noralf Tronnes
 *
 * This driver is inspired by:
 *   st7735fb.c, Copyright (C) 2011, Matt Porter
 *   broadsheetfb.c, Copyright (C) 2008, Jaya Kumar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <ctype.h>
#include <string.h>

#include "backlight.h"
#include "fbtft.h"
#include "gpio.h"
#include "mipi-dbi-spi.h"

static unsigned long debug;

static void fbtft_reset(struct fbtft_par *par)
{
	if (!par->gpio.reset)
		return;
	fbtft_par_dbg(DEBUG_RESET, par, "%s()\n", __func__);
	gpiod_set_value(par->gpio.reset, 0);
	mdelay(1);
	gpiod_set_value(par->gpio.reset, 1);
	mdelay(120);
}

static
char *skip_spaces(const char *str)
{
	while (isspace(*str))
		++str;
	return (char *)str;
}

static
char *strim(char *s)
{
	size_t size;
	char *end;

	size = strlen(s);
	if (!size)
		return s;

	end = s + size - 1;
	while (end >= s && isspace(*end))
		end--;
	*(end + 1) = '\0';

	return skip_spaces(s);
}

static int get_next_ulong(char **str_p, unsigned long *val, char *sep, int base)
{
	char *p_val;

	if (!str_p || !(*str_p))
		return -EINVAL;

	p_val = strsep(str_p, sep);

	if (!p_val)
		return -EINVAL;

	*val = strtoul(p_val, NULL, base);

	return 0;
}

static
int fbtft_gamma_parse_str(struct fbtft_par *par, unsigned long *curves,
						const char *str, int size)
{
	char *str_p, *curve_p = NULL;
	char *tmp;
	unsigned long val = 0;
	int ret = 0;
	int curve_counter, value_counter;

	fbtft_par_dbg(DEBUG_SYSFS, par, "%s() str=\n", __func__);

	if (!str || !curves)
		return -EINVAL;

	fbtft_par_dbg(DEBUG_SYSFS, par, "%s\n", str);

	tmp = malloc(size + 1);
	if (!tmp)
		return -ENOMEM;

	memcpy(tmp, str, size + 1);

	/* replace optional separators */
	str_p = tmp;
	while (*str_p) {
		if (*str_p == ',')
			*str_p = ' ';
		if (*str_p == ';')
			*str_p = '\n';
		str_p++;
	}

	str_p = strim(tmp);

	curve_counter = 0;
	while (str_p) {
		if (curve_counter == par->gamma.num_curves) {
			dev_err(par->info->device, "Gamma: Too many curves\n");
			ret = -EINVAL;
			goto out;
		}
		curve_p = strsep(&str_p, "\n");
		value_counter = 0;
		while (curve_p) {
			if (value_counter == par->gamma.num_values) {
				dev_err(par->info->device,
					"Gamma: Too many values\n");
				ret = -EINVAL;
				goto out;
			}
			ret = get_next_ulong(&curve_p, &val, " ", 16);
			if (ret)
				goto out;
			curves[curve_counter * par->gamma.num_values + value_counter] = val;
			value_counter++;
		}
		if (value_counter != par->gamma.num_values) {
			dev_err(par->info->device, "Gamma: Too few values\n");
			ret = -EINVAL;
			goto out;
		}
		curve_counter++;
	}
	if (curve_counter != par->gamma.num_curves) {
		dev_err(par->info->device, "Gamma: Too few curves\n");
		ret = -EINVAL;
		goto out;
	}

out:
	free(tmp);
	return ret;
}

static
void fbtft_expand_debug_value(unsigned long *_debug)
{
	switch (*_debug & 0x7) {
	case 1:
		*_debug |= DEBUG_LEVEL_1;
		break;
	case 2:
		*_debug |= DEBUG_LEVEL_2;
		break;
	case 3:
		*_debug |= DEBUG_LEVEL_3;
		break;
	case 4:
		*_debug |= DEBUG_LEVEL_4;
		break;
	case 5:
		*_debug |= DEBUG_LEVEL_5;
		break;
	case 6:
		*_debug |= DEBUG_LEVEL_6;
		break;
	case 7:
		*_debug = 0xFFFFFFFF;
		break;
	}
}

static u32 fbtft_of_value(struct device *dev, const char *propname)
{
	int ret;
	u32 val = 0;

	ret = device_property_read_u32(dev, propname, &val);
	if (ret == 0)
		pr_info("%s: %s = %u\n", __func__, propname, val);

	return val;
}

static inline struct fbtft_par *fbtft_par_from_mipi_dbi(struct mipi_dbi *mipi)
{
	return container_of(mipi, struct fbtft_par, mipi);
}

static void fbtft_enable(struct udrm_device *udev)
{
	struct mipi_dbi *mipi = mipi_dbi_from_tinydrm(udev);
	struct fbtft_par *par = fbtft_par_from_mipi_dbi(mipi);

	DRM_DEBUG_DRIVER("\n");

	if (!par->fbtftops.init_display(par))
		udev->prepared = true;

	if (par->fbtftops.set_var)
		par->fbtftops.set_var(par);

	if (par->fbtftops.set_gamma && par->gamma.curves)
		par->fbtftops.set_gamma(par, par->gamma.curves);
}

static const struct udrm_funcs fbtft_pipe_funcs = {
	.enable = fbtft_enable,
	.disable = mipi_dbi_disable,
	.dirtyfb = mipi_dbi_dirtyfb,
};

static void fbtft_set_mode(struct drm_mode_modeinfo *mode, unsigned int width, unsigned int height)
{
	struct drm_mode_modeinfo fbtft_mode = {
		UDRM_MODE(width, height, 0, 0),
	};

	*mode = fbtft_mode;
}

int fbtft_mipi_probe(const char *name, struct fbtft_display *display, struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct fb_info *info;
	struct fbtft_par *par;
	int ret;
	unsigned int width;
	unsigned int height;
	u32 rotate;
	char *gamma = display->gamma;
	unsigned long *gamma_curves = NULL;
	struct udrm_device *udev;
	struct mipi_dbi *mipi;
	struct gpio_desc *dc;
	struct drm_mode_modeinfo fbtft_mode;

pr_info("%s\n", __func__);

printk_level = 6;
udrm_debug = 0;

	if (display->debug & DEBUG_DRIVER_INIT_FUNCTIONS)
		dev_info(dev, "%s()\n", __func__);

	display->regwidth = fbtft_of_value(dev, "regwidth") ? : display->regwidth;
	display->buswidth = fbtft_of_value(dev, "buswidth") ? : display->buswidth;
	display->backlight = fbtft_of_value(dev, "backlight") ? : display->backlight;
	//fbtft_of_value(node, "fps");
	//fbtft_of_value(node, "txbuflen");

	/* defaults */
	if (!display->fps)
		display->fps = 20;
	if (!display->bpp)
		display->bpp = 16;

	if (display->buswidth == 0) {
		dev_err(dev, "buswidth is not set\n");
		return -EINVAL;
	}

	if (display->buswidth != 8) {
		dev_err(dev, "buswidth is not supported %u\n", display->buswidth);
		return -EINVAL;
	}

	/* sanity check */
	if (display->gamma_num * display->gamma_len > FBTFT_GAMMA_MAX_VALUES_TOTAL) {
		dev_err(dev, "FBTFT_GAMMA_MAX_VALUES_TOTAL=%d is exceeded\n", FBTFT_GAMMA_MAX_VALUES_TOTAL);
		return -EINVAL;
	}

	if (display->gamma_num && display->gamma_len) {
		gamma_curves = calloc(display->gamma_num * display->gamma_len, sizeof(gamma_curves[0]));
		if (!gamma_curves)
			return -ENOMEM;
	}

	par = calloc(1, sizeof(*par));
	if (!par)
		return -ENOMEM;

	info = &par->fb_info;
	par->info = info;
	info->par = par;

	par->spi = spi;

	par->debug = display->debug | debug | fbtft_of_value(dev, "debug");
	fbtft_expand_debug_value(&par->debug);

	par->fbtftops = display->fbtftops;
	par->fbtftops.reset = display->fbtftops.reset ? : fbtft_reset;

	par->bgr = device_property_read_bool(dev, "bgr");
	//par->init_sequence = init_sequence;
	par->gamma.curves = gamma_curves;
	par->gamma.num_curves = display->gamma_num;
	par->gamma.num_values = display->gamma_len;

	/* FIXME not supported */
	device_property_read_string(dev, "gamma", (const char **)&gamma);

	if (par->gamma.curves && gamma) {
		if (fbtft_gamma_parse_str(par,
			par->gamma.curves, gamma, strlen(gamma)))
			return -EINVAL;
	}

	rotate = fbtft_of_value(dev, "rotate");
	switch (rotate) {
	case 90:
	case 270:
		width =  display->height;
		height = display->width;
		break;
	default:
		width =  display->width;
		height = display->height;
	}

	info->device = dev;
	info->var.rotate =         rotate;
	info->var.xres =           fbtft_of_value(dev, "width") ? : width;
	info->var.yres =           fbtft_of_value(dev, "height") ? : height;
	info->var.xres_virtual =   info->var.xres;
	info->var.yres_virtual =   info->var.yres;
	info->var.bits_per_pixel = fbtft_of_value(dev, "bpp") ? : display->bpp;

	if (device_property_present(dev, "led-gpios"))
		display->backlight = 1;
//	if (device_property_present(dev, "init", NULL))
//		pdata->display.fbtftops.init_display = fbtft_init_display_dt;

	fbtft_set_mode(&fbtft_mode, width, height);




spi->max_speed_hz = 32000000;
spi->bits_per_word = 8;
spi->max_dma_len = 65536 - 4096; // master restriction 65535

	mipi = &par->mipi;

	mipi->reset = gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(mipi->reset)) {
		dev_err(dev, "Failed to get gpio 'reset'\n");
		return PTR_ERR(mipi->reset);
	}

	dc = gpiod_get_optional(dev, "dc", GPIOD_OUT_LOW);
	if (IS_ERR(dc)) {
		dev_err(dev, "Failed to get gpio 'dc'\n");
		return PTR_ERR(dc);
	}

	par->gpio.reset = mipi->reset;
	par->gpio.dc = dc;

	mipi->enable_delay_ms = 50;
	mipi->backlight = backlight_get(dev);
	if (IS_ERR(mipi->backlight))
		return PTR_ERR(mipi->backlight);

	mipi->reg = mipi_dbi_spi_init(spi, dc, false);
	if (IS_ERR(mipi->reg))
		return PTR_ERR(mipi->reg);

	ret = mipi_dbi_register(dev, mipi, name, &fbtft_pipe_funcs, &fbtft_mode, info->var.rotate);
	if (ret)
		return ret;

	udev = &mipi->udev;
	spi_set_drvdata(spi, udev);

	DRM_INFO("Initialized %s:%s @%uMHz on minor %d\n", udev->name, dev_name(dev), spi->max_speed_hz / 1000000, udev->index);

	return 0;
}

int fbtft_mipi_remove(struct spi_device *spi)
{

	struct udrm_device *udev = spi_get_drvdata(spi);
	struct mipi_dbi *mipi = mipi_dbi_from_tinydrm(udev);
	struct fbtft_par *par = fbtft_par_from_mipi_dbi(mipi);

	fbtft_par_dbg(DEBUG_DRIVER_INIT_FUNCTIONS, par, "%s()\n", __func__);

	udrm_unregister(udev);

	//if (mipi->dc)
	//	gpiod_put(mipi->dc);

	if (mipi->reset)
		gpiod_put(mipi->reset);

	free(par);

	return 0;
}
