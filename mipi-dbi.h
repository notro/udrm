/*
 * MIPI Display Bus Interface (DBI) LCD controller support
 *
 * Copyright 2016 Noralf Trønnes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MIPI_DBI_H
#define __MIPI_DBI_H

#include "mipi_display.h"
#include "udrm.h"

struct udrm_framebuffer;
struct drm_clip_rect;
struct spi_device;
struct gpio_desc;
struct device;

/**
 * mipi_dbi - MIPI DBI controller

 * @reg: register map
 * @reset: Optional reset gpio
 * @rotation: initial rotation in degress Counter Clock Wise
 * @backlight: backlight device (optional)
 * @enable_delay_ms: Optional delay in milliseconds before turning on backlight
 */
struct mipi_dbi {
	struct udrm_device udev;
	struct regmap *reg;
	struct gpio_desc *reset;
	unsigned int rotation;
	struct backlight_device *backlight;
	unsigned int enable_delay_ms;
};

static inline struct mipi_dbi *
mipi_dbi_from_tinydrm(struct udrm_device *udev)
{
	return container_of(udev, struct mipi_dbi, udev);
}

int mipi_dbi_register(struct device *dev, struct mipi_dbi *mipi, const char *name, const struct udrm_funcs *funcs,
		      const struct drm_mode_modeinfo *mode, unsigned int rotation);
int mipi_dbi_dirtyfb(struct udrm_framebuffer *ufb, unsigned int flags, unsigned int color, struct drm_clip_rect *clips, unsigned int num_clips);
void mipi_dbi_disable(struct udrm_device *udev);
void mipi_dbi_hw_reset(struct mipi_dbi *mipi);
bool mipi_dbi_display_is_on(struct regmap *reg);

/**
 * mipi_dbi_write - Write command and optional parameter(s)
 * @cmd: Command
 * @...: Parameters
 */
#define mipi_dbi_write(reg, cmd, seq...) \
({ \
	u8 d[] = { seq }; \
	mipi_dbi_write_buf(reg, cmd, d, ARRAY_SIZE(d)); \
})

int mipi_dbi_write_buf(struct regmap *reg, unsigned int cmd,
		       const u8 *parameters, size_t num);

#endif /* __LINUX_MIPI_DBI_H */
