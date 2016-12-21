/*
 * DRM driver for Multi-Inno MI0283QT panels
 *
 * Copyright 2016 Noralf Trønnes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

// printf
#include <stdio.h>


// calloc
#include <stdlib.h>

#include "backlight.h"


#include "ili9341.h"
#include "udrm.h"
#include "mipi-dbi-spi.h"
//#include <drm/tinydrm/tinydrm.h>
//#include <drm/tinydrm/tinydrm-helpers.h>
//#include <linux/delay.h>
#include "gpio.h"
//#include <linux/module.h>
//#include <linux/property.h>
//#include <linux/regulator/consumer.h>
#include "spi.h"

static void mi0283qt_enable(struct udrm_device *udev)
{
	struct mipi_dbi *mipi = mipi_dbi_from_tinydrm(udev);
	//struct device *dev = tdev->drm.dev;
	struct regmap *reg = mipi->reg;
	u8 addr_mode;
	int ret;

	DRM_DEBUG_DRIVER("\n");

	//mutex_lock(&udev->dev_lock);

	if (udev->prepared)
		return; //goto out_unlock;

	/* Avoid flicker by skipping setup if the bootloader has done it */
	if (0 && mipi_dbi_display_is_on(reg)) {
		udev->prepared = true;
		return; //goto out_unlock;
	}

	mipi_dbi_hw_reset(mipi);
	ret = mipi_dbi_write(reg, MIPI_DCS_SOFT_RESET);
	if (ret) {
		DRM_ERROR("Error writing command %d\n", ret);
		return; //goto out_unlock;
	}

	msleep(20);

	mipi_dbi_write(reg, MIPI_DCS_SET_DISPLAY_OFF);

	mipi_dbi_write(reg, ILI9341_PWCTRLB, 0x00, 0x83, 0x30);
	mipi_dbi_write(reg, ILI9341_PWRSEQ, 0x64, 0x03, 0x12, 0x81);
	mipi_dbi_write(reg, ILI9341_DTCTRLA, 0x85, 0x01, 0x79);
	mipi_dbi_write(reg, ILI9341_PWCTRLA, 0x39, 0x2c, 0x00, 0x34, 0x02);
	mipi_dbi_write(reg, ILI9341_PUMPCTRL, 0x20);
	mipi_dbi_write(reg, ILI9341_DTCTRLB, 0x00, 0x00);

	/* Power Control */
	mipi_dbi_write(reg, ILI9341_PWCTRL1, 0x26);
	mipi_dbi_write(reg, ILI9341_PWCTRL2, 0x11);
	/* VCOM */
	mipi_dbi_write(reg, ILI9341_VMCTRL1, 0x35, 0x3e);
	mipi_dbi_write(reg, ILI9341_VMCTRL2, 0xbe);

	/* Memory Access Control */
	mipi_dbi_write(reg, MIPI_DCS_SET_PIXEL_FORMAT, 0x55);

	DRM_DEBUG_KMS("Rotation=%u\n", mipi->rotation);
	switch (mipi->rotation) {
	default:
		addr_mode = ILI9341_MADCTL_MV | ILI9341_MADCTL_MY | ILI9341_MADCTL_MX;
		break;
	case 90:
		addr_mode = ILI9341_MADCTL_MY;
		break;
	case 180:
		addr_mode = ILI9341_MADCTL_MV;
		break;
	case 270:
		addr_mode = ILI9341_MADCTL_MX;
		break;
	}
	addr_mode |= ILI9341_MADCTL_BGR;
	mipi_dbi_write(reg, MIPI_DCS_SET_ADDRESS_MODE, addr_mode);

	/* Frame Rate */
	mipi_dbi_write(reg, ILI9341_FRMCTR1, 0x00, 0x1b);

	/* Gamma */
	mipi_dbi_write(reg, ILI9341_EN3GAM, 0x08);
	mipi_dbi_write(reg, MIPI_DCS_SET_GAMMA_CURVE, 0x01);
	mipi_dbi_write(reg, ILI9341_PGAMCTRL,
		       0x1f, 0x1a, 0x18, 0x0a, 0x0f, 0x06, 0x45, 0x87,
		       0x32, 0x0a, 0x07, 0x02, 0x07, 0x05, 0x00);
	mipi_dbi_write(reg, ILI9341_NGAMCTRL,
		       0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3a, 0x78,
		       0x4d, 0x05, 0x18, 0x0d, 0x38, 0x3a, 0x1f);

	/* DDRAM */
	mipi_dbi_write(reg, ILI9341_ETMOD, 0x07);

	/* Display */
	mipi_dbi_write(reg, ILI9341_DISCTRL, 0x0a, 0x82, 0x27, 0x00);
	mipi_dbi_write(reg, MIPI_DCS_EXIT_SLEEP_MODE);
	msleep(100);

	mipi_dbi_write(reg, MIPI_DCS_SET_DISPLAY_ON);
	msleep(100);

	udev->prepared = true;

//out_unlock:
//	mutex_unlock(&udev->dev_lock);
}

static const struct udrm_funcs mi0283qt_pipe_funcs = {
	.enable = mi0283qt_enable,
	.disable = mipi_dbi_disable,
	.dirtyfb = mipi_dbi_dirtyfb,
};

static const struct drm_mode_modeinfo mi0283qt_mode = {
	UDRM_MODE(320, 240, 58, 43),
};

#if 0
static const struct of_device_id mi0283qt_of_match[] = {
	{ .compatible = "multi-inno,mi0283qt" },
	{},
};
MODULE_DEVICE_TABLE(of, mi0283qt_of_match);

static const struct spi_device_id mi0283qt_id[] = {
	{ "mi0283qt", 0 },
	{ },
};
MODULE_DEVICE_TABLE(spi, mi0283qt_id);

static struct drm_driver mi0283qt_driver = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_PRIME |
				  DRIVER_ATOMIC,
	TINYDRM_GEM_DRIVER_OPS,
	.lastclose		= tinydrm_lastclose,
	.debugfs_init		= mipi_dbi_debugfs_init,
	.debugfs_cleanup	= mipi_dbi_debugfs_cleanup,
	.name			= "mi0283qt",
	.desc			= "Multi-Inno MI0283QT",
	.date			= "20160614",
	.major			= 1,
	.minor			= 0,
};
#endif

static int mi0283qt_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct udrm_device *udev;
	struct mipi_dbi *mipi;
	struct gpio_desc *dc;
	u32 rotation = 0;
	bool writeonly;
	int ret;

printf("%s\n", __func__);

	mipi = calloc(1, sizeof(*mipi));
	if (!mipi)
		return -ENOMEM;

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

	mipi->enable_delay_ms = 50;
	mipi->backlight = backlight_get(dev);
	if (IS_ERR(mipi->backlight))
		return PTR_ERR(mipi->backlight);

	writeonly = device_property_read_bool(dev, "write-only");
	device_property_read_u32(dev, "rotation", &rotation);

	mipi->reg = mipi_dbi_spi_init(spi, dc, writeonly);
	if (IS_ERR(mipi->reg))
		return PTR_ERR(mipi->reg);

	ret = mipi_dbi_register(dev, mipi, "mi0283qt", &mi0283qt_pipe_funcs, &mi0283qt_mode, rotation);
	if (ret)
		return ret;

	udev = &mipi->udev;
	spi_set_drvdata(spi, udev);

	DRM_INFO("Initialized %s:%s @%uMHz on minor %d\n", udev->name, dev_name(dev), spi->max_speed_hz / 1000000, udev->index);

	return 0;
}

static int mi0283qt_remove(struct spi_device *spi)
{
	struct udrm_device *udev = spi_get_drvdata(spi);
	struct mipi_dbi *mipi = mipi_dbi_from_tinydrm(udev);

	udrm_unregister(udev);

	//if (mipi->dc)
	//	gpiod_put(mipi->dc);

	if (mipi->reset)
		gpiod_put(mipi->reset);

	free(mipi);

	return 0;
}

#if 0
static struct spi_driver mi0283qt_spi_driver = {
	.driver = {
		.name = "mi0283qt",
		.of_match_table = mi0283qt_of_match,
	},
	.id_table = mi0283qt_id,
	.probe = mi0283qt_probe,
	.shutdown = tinydrm_spi_shutdown,
};
module_spi_driver(mi0283qt_spi_driver);
#endif

static u32 reset_gpios[] = { 0, 23 };
static u32 dc_gpios[] = { 0, 24 };
static u32 led_gpios[] = { 0, 18 };
static u32 rotation = 180;

struct prop rpi_display_props[] = {
	{
		.name = "reset-gpios",
		.data = reset_gpios,
		.len = sizeof(reset_gpios),
	},
	{
		.name = "dc-gpios",
		.data = dc_gpios,
		.len = sizeof(dc_gpios),
	},
	{
		.name = "led-gpios",
		.data = led_gpios,
		.len = sizeof(led_gpios),
	},
	{
		.name = "rotation",
		.data = &rotation,
		.len = sizeof(rotation),
	},
	{ /* sentinel */ },
};


int main(int argc, char const *argv[])
{
	struct udrm_device *udev;
	struct spi_device *spi;
	int ret;

printk_level = 6;
udrm_debug = 0;

printf("%s\n", __func__);
	spi = calloc(1, sizeof(*spi));
	if (!spi) {
		pr_err("alloc error\n");
		return 1;
	}

	spi->dev.props = rpi_display_props;

	spi->dev.name = "spi0.0";
	spi->fname = "/dev/spidev0.0";
	spi->max_speed_hz = 32000000;
	spi->bits_per_word = 8;
	spi->max_dma_len = 320 * 250 * 2 / 5;

	ret = spi_add_device(spi);
	if (ret) {
		pr_err("spi add error %d\n", ret);
		return 1;
	}

	ret = mi0283qt_probe(spi);
	if (ret) {
		pr_err("probe error %d\n", ret);
		return 1;
	}

	udev = spi_get_drvdata(spi);

	udrm_event_loop(udev);


	mi0283qt_remove(spi);

	spi_unregister_device(spi);
	free(spi);

printf("%s: exit\n", __func__);
	return 0;
}
