#include <string.h>

#include "device.h"

struct prop *device_find_property(struct device *dev, const char *propname)
{
	struct prop *prop = dev->props;

	if (!propname)
		return NULL;

	while (prop->name) {
		if (!strncmp(propname, prop->name, 50))
			return prop;
		prop++;
	}

	return NULL;
}

int device_property_read_u32_array(struct device *dev, const char *propname, u32 *val, size_t nval)
{
	struct prop *prop;
	u32 *data;

//pr_info("%s: propname=%s, nval=%zu\n", __func__, propname, nval);
	prop = device_find_property(dev, propname);
	if (!prop)
		return -EINVAL;

	if (!prop->data)
		return -ENODATA;

	if ((nval * sizeof(u32)) > prop->len)
		return -EOVERFLOW;

	data = prop->data;
	while (nval--)
		*val++ = *data++;

	return 0;
}
