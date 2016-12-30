#ifndef _DEVICE_H
#define _DEVICE_H

#include <limits.h>

#include "base.h"

struct prop {
	char *name;
	void *data;
	size_t len;
};

struct device {
	char name[32];
	char sysfs[PATH_MAX];
	struct prop *props;
	void *driver_data;
};

int dev_set_name(struct device *dev, const char *fmt, ...);
int dev_set_sysfs(struct device *dev, const char *fmt, ...);
int device_add(struct device *dev);

static inline const char *dev_name(const struct device *dev)
{
	return dev->name;
}

static inline void *dev_get_drvdata(const struct device *dev)
{
	return dev->driver_data;
}

static inline void dev_set_drvdata(struct device *dev, void *data)
{
	dev->driver_data = data;
}


struct prop *device_find_property(struct device *dev, const char *propname);
int device_property_read_u32_array(struct device *dev, const char *propname, u32 *val, size_t nval);
int device_property_read_string(struct device *dev, const char *propname, const char **val);

static inline bool device_property_present(struct device *dev, const char *propname)
{
	return device_find_property(dev, propname);
}

static inline bool device_property_read_bool(struct device *dev, const char *propname)
{
	return device_find_property(dev, propname);
}

static inline int device_property_read_u32(struct device *dev, const char *propname, u32 *val)
{
	return device_property_read_u32_array(dev, propname, val, 1);
}


#define dev_err(dev, fmt, ...)			\
	printk(KERN_ERR, fmt, ##__VA_ARGS__)

#define dev_warn(dev, fmt, ...)			\
	printk(KERN_WARNING, fmt, ##__VA_ARGS__)

#define dev_info(dev, fmt, ...)			\
	printk(KERN_INFO, fmt, ##__VA_ARGS__)

#define dev_dbg(dev, fmt, ...)			\
	printk(KERN_DEBUG, fmt, ##__VA_ARGS__)

#endif
