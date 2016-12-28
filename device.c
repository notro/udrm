#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "device.h"

int dev_set_name(struct device *dev, const char *fmt, ...)
{
	va_list vargs;
	int err;

	va_start(vargs, fmt);
	err = vsnprintf(dev->name, 32, fmt, vargs);
	va_end(vargs);
	return err;
}

int dev_set_sysfs(struct device *dev, const char *fmt, ...)
{
	va_list vargs;
	int err;

	va_start(vargs, fmt);
	err = vsnprintf(dev->sysfs, PATH_MAX, fmt, vargs);
	va_end(vargs);
	return err;
}

struct prop *device_find_property(struct device *dev, const char *name)
{
	struct prop *prop = dev->props;

	if (!prop || !name)
		return NULL;

	while (prop->name) {
		if (!strncmp(name, prop->name, 50))
			return prop;
		prop++;
	}

	return NULL;
}

static struct prop *device_new_property(struct device *dev)
{
	struct prop *prop;
	unsigned int i;

	if (!dev->props) {
		dev->props = calloc(2, sizeof(*prop));
		if (!dev->props)
			return NULL;

		return dev->props;
	}

	for (prop = dev->props, i = 0; prop->name; prop++, i++)
		;;

	prop = realloc(dev->props, (i + 2) * sizeof(*prop));
	if (!prop)
		return NULL;

	dev->props = prop;
	prop[i + 1].name = NULL;

	return &prop[i];
}

static int device_set_property(struct device *dev, const char *name, void *data, size_t len)
{
	struct prop *prop;
	char *str;

	str = strdup(name);
	if (!str)
		return -ENOMEM;

	prop = device_find_property(dev, name);
	if (!prop)
		prop = device_new_property(dev);
	if (!prop)
		return -ENOMEM;

	prop->name = str;
	prop->data = data;
	prop->len = len;

pr_info("%s: name='%s', data=%p, len=%zu\n", __func__, prop->name, prop->data, prop->len);

	return 0;
}

/* big endian */
static void *device_read_property_file(const char *fname, size_t len)
{
	int fd, ret;
	void *buf;

	fd = open(fname, O_RDONLY);
	if (fd == -1)
		return ERR_PTR(-errno);

	buf = malloc(len);
	if (!buf) {
		buf = ERR_PTR(-ENOMEM);
		goto out;
	}

	ret = read(fd, buf, len);
	if (ret != len) {
		free(buf);
		buf = ERR_PTR(ret < 0 ? -errno : -EIO);
	}

out:
	close(fd);

	return buf;
}

int device_add(struct device *dev)
{
	char dirname[PATH_MAX], fname[PATH_MAX];
	struct dirent* f;
	struct stat st;
	DIR *dp;
	int ret = 0;

	snprintf(dirname, PATH_MAX, "%s/of_node", dev->sysfs);

	if (!(dp = opendir(dirname)))
		return errno != ENOENT ? -errno : 0;

	while ((f = readdir(dp))) {
		snprintf(fname, PATH_MAX, "%s/%s", dirname, f->d_name);
		if (stat(fname, &st) == -1) {
			ret = -errno;
			goto out_close;
		}

		if (S_ISREG(st.st_mode)) {
			void *buf;

			buf = device_read_property_file(fname, st.st_size);
			if (IS_ERR(buf))
				ret = PTR_ERR(buf);
			else
				ret = device_set_property(dev, f->d_name, buf, st.st_size);

			if (ret)
				pr_err("Failed to set property '%s': %d\n", f->d_name, ret);
		}
	}

out_close:
	closedir(dp);

	return ret;
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
		*val++ = ntohl(*data++);

	return 0;
}
