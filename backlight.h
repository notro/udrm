#ifndef __BACKLIGHT_H
#define __BACKLIGHT_H

#include "device.h"
#include "gpio.h"

struct backlight_device {
	struct gpio_desc *gpio;
};

struct backlight_device *backlight_get(struct device *dev);
void backlight_put(struct backlight_device *bl);
int backlight_set(struct backlight_device *bl, unsigned int value);

static inline int backlight_enable(struct backlight_device *bl)
{
	return backlight_set(bl, 1);
}

static inline int backlight_disable(struct backlight_device *bl)
{
	return backlight_set(bl, 0);
}

#endif
