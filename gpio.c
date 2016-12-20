#include <stdio.h>
#include <string.h>

// PATH_MAX
#include <linux/limits.h>

// file
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gpio.h"
#include "device.h"

static int file_write_string(const char *pathname, const char *buf)
{
	int fd, ret;

	fd = open(pathname, O_WRONLY);
	if (fd == -1) {
		printf("%s: Failed to open '%s': %s\n", __func__, pathname, strerror(errno));
		return -errno;
	}

	ret = write(fd, buf, strlen(buf));
	if (ret == -1) {
		printf("%s: Failed to write '%s': %s\n", __func__, pathname, strerror(errno));
		close(fd);
		return -errno;
	}

	close(fd);

	return 0;
}

struct gpio_desc *gpiod_get_optional(struct device *dev, const char *con_id, unsigned int flags)
{
	struct gpio_desc *desc;
	char fname[PATH_MAX];
	int ret, fd, gpio;
	char buf[32];
	u32 data[2];

	snprintf(buf, sizeof(buf), "%s-gpios", con_id);

	ret = device_property_read_u32_array(dev, buf, data, 2);
	if (ret)
		return NULL;

	gpio = data[1];
	snprintf(fname, sizeof(fname), "/sys/class/gpio/gpio%d/value", gpio);

	if(access(fname, F_OK) == -1) {
		char fname2[PATH_MAX];

		snprintf(fname2, sizeof(fname2), "%d", gpio);
		ret = file_write_string("/sys/class/gpio/export", fname2);
		if (ret)
			return ERR_PTR(ret);

		snprintf(fname2, sizeof(fname2), "/sys/class/gpio/gpio%d/direction", gpio);
		ret = file_write_string(fname2, "out");
		if (ret)
			return ERR_PTR(ret);
	}

	fd = open(fname, O_WRONLY);
	if (fd == -1) {
		printf("%s: Failed to open '%s': %s\n", __func__, fname, strerror(errno));
		return ERR_PTR(-errno);
	}

	desc = calloc(1, sizeof(*desc));
	if (!desc)
		return ERR_PTR(-ENOMEM);

	desc->fd = fd;
	desc->name = con_id;
	desc->gpio = gpio;

	if (flags == GPIOD_OUT_HIGH)
		gpiod_set_value(desc, 1);
	else if (flags == GPIOD_OUT_LOW)
		gpiod_set_value(desc, 0);

	return desc;
}

void gpiod_put(struct gpio_desc *desc)
{
	pr_debug("%s(%s, %d)\n", __func__, desc->name, desc->gpio);
	close(desc->fd);
	free(desc);
}

void gpiod_set_value(struct gpio_desc *desc, int value)
{
	char buf[2];

	pr_debug("%s(gpio=%d, value=%d)\n", __func__, desc->gpio, value);

	snprintf(buf, sizeof(buf), "%d", value ? 1 : 0);
	if (write(desc->fd, buf, strlen(buf)) == -1)
		printf("%s(%d, %d): Failed to write gpio: %s\n",
		       __func__, desc->gpio, value, strerror(errno));
}
