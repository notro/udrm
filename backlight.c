#include "backlight.h"


struct backlight_device *backlight_get(struct device *dev)
{
	struct backlight_device *bl;
	struct gpio_desc *led;

	led = gpiod_get_optional(dev, "led", GPIOD_OUT_LOW);
	if (IS_ERR(led)) {
		dev_err(dev, "Error getting gpio 'led'\n");
		return ERR_CAST(led);
	}

	if (!led)
		return NULL;

	bl = calloc(1, sizeof(*bl));
	if (!bl) {
		gpiod_put(led);
		return ERR_PTR(-ENOMEM);
	}

	bl->gpio = led;

	return bl;
}

void backlight_put(struct backlight_device *bl)
{
	gpiod_put(bl->gpio);
	free(bl);
}

int backlight_set(struct backlight_device *bl, unsigned int value)
{
	if (bl)
		gpiod_set_value(bl->gpio, value);

	return 0;
}
