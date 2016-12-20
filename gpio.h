#ifndef _GPIO_H
#define _GPIO_H

#include "base.h"

struct device;

#define GPIOD_OUT_LOW	0
#define GPIOD_OUT_HIGH	1

struct gpio_desc {
	unsigned int gpio;
	const char *name;
	int fd;
};

struct gpio_desc *gpiod_get_optional(struct device *dev, const char *con_id, unsigned int flags);

void gpiod_put(struct gpio_desc *desc);

void gpiod_set_value(struct gpio_desc *desc, int value);

#endif
