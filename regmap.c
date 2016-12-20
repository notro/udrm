// memcpy
#include <string.h>

#include "udrm.h"
#include "regmap.h"

struct device;

static enum regmap_endian regmap_get_reg_endian(const struct regmap_bus *bus,
					const struct regmap_config *config)
{
	enum regmap_endian endian;

	/* Retrieve the endianness specification from the bus config */
	if (bus && bus->reg_format_endian_default)
		endian = bus->reg_format_endian_default;

	/* If the bus specified a non-default value, use that */
	if (endian != REGMAP_ENDIAN_DEFAULT)
		return endian;

	/* Use this if no other value was found */
	return REGMAP_ENDIAN_BIG;
}

static enum regmap_endian regmap_get_val_endian(struct device *dev,
					 const struct regmap_bus *bus,
					 const struct regmap_config *config)
{
	enum regmap_endian endian;

	/* Retrieve the endianness specification from the bus config */
	if (bus && bus->val_format_endian_default)
		endian = bus->val_format_endian_default;

	/* If the bus specified a non-default value, use that */
	if (endian != REGMAP_ENDIAN_DEFAULT)
		return endian;

	/* Use this if no other value was found */
	return REGMAP_ENDIAN_BIG;
}

static void regmap_set_work_buf_flag_mask(struct regmap *map, int max_bytes,
					  unsigned long mask)
{
	u8 *buf;
	int i;

	if (!mask || !map->work_buf)
		return;

	buf = map->work_buf;

	for (i = 0; i < max_bytes; i++)
		buf[i] |= (mask >> (8 * i)) & 0xff;
}

static int _regmap_raw_write(struct regmap *map, unsigned int reg,
		      const void *val, size_t val_len)
{
	//unsigned long flags;
	void *work_val = map->work_buf + map->format.reg_bytes +
		map->format.pad_bytes;
	void *buf;
	int ret = -EOPNOTSUPP;
	size_t len;
	//int i;

	map->format.format_reg(map->work_buf, reg, map->reg_shift);
	regmap_set_work_buf_flag_mask(map, map->format.reg_bytes,
				      map->write_flag_mask);

	/*
	 * Essentially all I/O mechanisms will be faster with a single
	 * buffer to write.  Since register syncs often generate raw
	 * writes of single registers optimise that case.
	 */
	if (val != work_val && val_len == map->format.val_bytes) {
		memcpy(work_val, val, map->format.val_bytes);
		val = work_val;
	}

	/* If we're doing a single register write we can probably just
	 * send the work_buf directly, otherwise try to do a gather
	 * write.
	 */
	if (val == work_val)
		ret = map->bus->write(map->bus_context, map->work_buf,
				      map->format.reg_bytes +
				      map->format.pad_bytes +
				      val_len);
	else if (map->bus->gather_write)
		ret = map->bus->gather_write(map->bus_context, map->work_buf,
					     map->format.reg_bytes +
					     map->format.pad_bytes,
					     val, val_len);

	/* If that didn't work fall back on linearising by hand. */
	if (ret == -EOPNOTSUPP) {
		len = map->format.reg_bytes + map->format.pad_bytes + val_len;
		buf = calloc(1, len);
		if (!buf)
			return -ENOMEM;

		memcpy(buf, map->work_buf, map->format.reg_bytes);
		memcpy(buf + map->format.reg_bytes + map->format.pad_bytes,
		       val, val_len);
		ret = map->bus->write(map->bus_context, buf, len);

		free(buf);
	}

	return ret;
}

static bool regmap_can_raw_write(struct regmap *map)
{
	return map->bus && map->bus->write && map->format.format_val &&
		map->format.format_reg;
}

int regmap_raw_write(struct regmap *map, unsigned int reg,
		     const void *val, size_t val_len)
{
	int ret;

	DRM_DEBUG("reg=0x%02x, val_len=%zu\n", reg, val_len);

	if (!regmap_can_raw_write(map))
		return -EINVAL;
	if (val_len % map->format.val_bytes)
		return -EINVAL;
	//if (map->max_raw_write && map->max_raw_write > val_len)
	//	return -E2BIG;

	//map->lock(map->lock_arg);

	ret = _regmap_raw_write(map, reg, val, val_len);

	//map->unlock(map->lock_arg);

	return ret;
}

static int _regmap_raw_read(struct regmap *map, unsigned int reg, void *val,
			    unsigned int val_len)
{
	int ret;

	if (!map->bus || !map->bus->read)
		return -EINVAL;

	map->format.format_reg(map->work_buf, reg, map->reg_shift);
	regmap_set_work_buf_flag_mask(map, map->format.reg_bytes,
				      map->read_flag_mask);

	ret = map->bus->read(map->bus_context, map->work_buf,
			     map->format.reg_bytes + map->format.pad_bytes,
			     val, val_len);

	return ret;
}

#define IS_ALIGNED(x, a)                (((x) & ((typeof(x))(a) - 1)) == 0)

int regmap_raw_read(struct regmap *map, unsigned int reg, void *val, size_t val_len)
{
	size_t val_bytes = map->format.val_bytes;
	size_t val_count = val_len / val_bytes;
	int ret;

	DRM_DEBUG("reg=0x%02x, val_len=%zu\n", reg, val_len);

	if (!map->bus)
		return -EINVAL;
	if (val_len % map->format.val_bytes)
		return -EINVAL;
	if (!IS_ALIGNED(reg, map->reg_stride))
		return -EINVAL;
	if (val_count == 0)
		return -EINVAL;

		if (!map->bus->read) {
			ret = -EOPNOTSUPP;
			goto out;
		}
		if (map->max_raw_read && map->max_raw_read < val_len) {
			ret = -E2BIG;
			goto out;
		}

		/* Physical block read if there's no cache involved */
		ret = _regmap_raw_read(map, reg, val, val_len);

out:
	return ret;
}

static int _regmap_bus_read(void *context, unsigned int reg,
			    unsigned int *val)
{
	int ret;
	struct regmap *map = context;

	if (!map->format.parse_val)
		return -EINVAL;

	ret = _regmap_raw_read(map, reg, map->work_buf, map->format.val_bytes);
	if (ret == 0)
		*val = map->format.parse_val(map->work_buf);

	return ret;
}

static void regmap_format_8(void *buf, unsigned int val, unsigned int shift)
{
	u8 *b = buf;

	b[0] = val << shift;
}

static void regmap_parse_inplace_noop(void *buf)
{
}

static unsigned int regmap_parse_8(const void *buf)
{
	const u8 *b = buf;

	return b[0];
}

static int _regmap_bus_formatted_write(void *context, unsigned int reg,
				       unsigned int val)
{
	int ret;
	struct regmap *map = context;

	map->format.format_write(map, reg, val);

	ret = map->bus->write(map->bus_context, map->work_buf,
			      map->format.buf_size);

	return ret;
}

static int _regmap_bus_raw_write(void *context, unsigned int reg,
				 unsigned int val)
{
	struct regmap *map = context;

	map->format.format_val(map->work_buf + map->format.reg_bytes
			       + map->format.pad_bytes, val, 0);
	return _regmap_raw_write(map, reg,
				 map->work_buf +
				 map->format.reg_bytes +
				 map->format.pad_bytes,
				 map->format.val_bytes);
}

static struct regmap *__regmap_init(struct device *dev,
			     const struct regmap_bus *bus,
			     void *bus_context,
			     const struct regmap_config *config)
//			     struct lock_class_key *lock_key,
//			     const char *lock_name)
{
	struct regmap *map;
	int ret = -EINVAL;
	enum regmap_endian reg_endian, val_endian;
	//int i, j;

	if (!config)
		goto err;

	map = calloc(1, sizeof(*map));
	if (map == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	map->format.reg_bytes = DIV_ROUND_UP(config->reg_bits, 8);
	map->format.pad_bytes = config->pad_bits / 8;
	map->format.val_bytes = DIV_ROUND_UP(config->val_bits, 8);
	map->format.buf_size = DIV_ROUND_UP(config->reg_bits +
			config->val_bits + config->pad_bits, 8);
	map->reg_shift = config->pad_bits % 8;
	if (config->reg_stride)
		map->reg_stride = config->reg_stride;
	else
		map->reg_stride = 1;

	map->use_single_read = config->use_single_rw || !bus || !bus->read;
	map->use_single_write = config->use_single_rw || !bus || !bus->write;
	map->can_multi_write = config->can_multi_write && bus && bus->write;
	if (bus) {
		map->max_raw_read = bus->max_raw_read;
		map->max_raw_write = bus->max_raw_write;
	}
//	map->dev = dev;
	map->bus = bus;
	map->bus_context = bus_context;
//	map->max_register = config->max_register;
	//map->wr_table = config->wr_table;
	//map->rd_table = config->rd_table;
	//map->volatile_table = config->volatile_table;
	//map->precious_table = config->precious_table;
	//map->writeable_reg = config->writeable_reg;
	//map->readable_reg = config->readable_reg;
	//map->volatile_reg = config->volatile_reg;
	//map->precious_reg = config->precious_reg;
	map->cache_type = config->cache_type;
	//map->name = config->name;

	//spin_lock_init(&map->async_lock);
	//INIT_LIST_HEAD(&map->async_list);
	//INIT_LIST_HEAD(&map->async_free);
	//init_waitqueue_head(&map->async_waitq);

	//if (config->read_flag_mask || config->write_flag_mask) {
	//	map->read_flag_mask = config->read_flag_mask;
	//	map->write_flag_mask = config->write_flag_mask;
	//} else if (bus) {
	//	map->read_flag_mask = bus->read_flag_mask;
	//}

//	if (!bus) {
//		map->reg_read  = config->reg_read;
//		map->reg_write = config->reg_write;
//
//		map->defer_caching = false;
//		goto skip_format_initialization;
//	} else
//	if (!bus->read || !bus->write) {
//		map->reg_read = _regmap_bus_reg_read;
//		map->reg_write = _regmap_bus_reg_write;
//
//		map->defer_caching = false;
//		goto skip_format_initialization;
//	} else {
		map->reg_read  = _regmap_bus_read;
//		map->reg_update_bits = bus->reg_update_bits;
//	}

	reg_endian = regmap_get_reg_endian(bus, config);
	val_endian = regmap_get_val_endian(dev, bus, config);

	switch (config->reg_bits + map->reg_shift) {
#if 0
	case 2:
		switch (config->val_bits) {
		case 6:
			map->format.format_write = regmap_format_2_6_write;
			break;
		default:
			goto err_map;
		}
		break;

	case 4:
		switch (config->val_bits) {
		case 12:
			map->format.format_write = regmap_format_4_12_write;
			break;
		default:
			goto err_map;
		}
		break;

	case 7:
		switch (config->val_bits) {
		case 9:
			map->format.format_write = regmap_format_7_9_write;
			break;
		default:
			goto err_map;
		}
		break;

	case 10:
		switch (config->val_bits) {
		case 14:
			map->format.format_write = regmap_format_10_14_write;
			break;
		default:
			goto err_map;
		}
		break;
#endif
	case 8:
		map->format.format_reg = regmap_format_8;
		break;
#if 0
	case 16:
		switch (reg_endian) {
		case REGMAP_ENDIAN_BIG:
			map->format.format_reg = regmap_format_16_be;
			break;
		case REGMAP_ENDIAN_LITTLE:
			map->format.format_reg = regmap_format_16_le;
			break;
		case REGMAP_ENDIAN_NATIVE:
			map->format.format_reg = regmap_format_16_native;
			break;
		default:
			goto err_map;
		}
		break;

	case 24:
		if (reg_endian != REGMAP_ENDIAN_BIG)
			goto err_map;
		map->format.format_reg = regmap_format_24;
		break;

	case 32:
		switch (reg_endian) {
		case REGMAP_ENDIAN_BIG:
			map->format.format_reg = regmap_format_32_be;
			break;
		case REGMAP_ENDIAN_LITTLE:
			map->format.format_reg = regmap_format_32_le;
			break;
		case REGMAP_ENDIAN_NATIVE:
			map->format.format_reg = regmap_format_32_native;
			break;
		default:
			goto err_map;
		}
		break;
#endif
	default:
		goto err_map;
	}

	if (val_endian == REGMAP_ENDIAN_NATIVE)
		map->format.parse_inplace = regmap_parse_inplace_noop;

	switch (config->val_bits) {
	case 8:
		map->format.format_val = regmap_format_8;
		map->format.parse_val = regmap_parse_8;
		map->format.parse_inplace = regmap_parse_inplace_noop;
		break;
#if 0
	case 16:
		switch (val_endian) {
		case REGMAP_ENDIAN_BIG:
			map->format.format_val = regmap_format_16_be;
			map->format.parse_val = regmap_parse_16_be;
			map->format.parse_inplace = regmap_parse_16_be_inplace;
			break;
		case REGMAP_ENDIAN_LITTLE:
			map->format.format_val = regmap_format_16_le;
			map->format.parse_val = regmap_parse_16_le;
			map->format.parse_inplace = regmap_parse_16_le_inplace;
			break;
		case REGMAP_ENDIAN_NATIVE:
			map->format.format_val = regmap_format_16_native;
			map->format.parse_val = regmap_parse_16_native;
			break;
		default:
			goto err_map;
		}
		break;
	case 24:
		if (val_endian != REGMAP_ENDIAN_BIG)
			goto err_map;
		map->format.format_val = regmap_format_24;
		map->format.parse_val = regmap_parse_24;
		break;
	case 32:
		switch (val_endian) {
		case REGMAP_ENDIAN_BIG:
			map->format.format_val = regmap_format_32_be;
			map->format.parse_val = regmap_parse_32_be;
			map->format.parse_inplace = regmap_parse_32_be_inplace;
			break;
		case REGMAP_ENDIAN_LITTLE:
			map->format.format_val = regmap_format_32_le;
			map->format.parse_val = regmap_parse_32_le;
			map->format.parse_inplace = regmap_parse_32_le_inplace;
			break;
		case REGMAP_ENDIAN_NATIVE:
			map->format.format_val = regmap_format_32_native;
			map->format.parse_val = regmap_parse_32_native;
			break;
		default:
			goto err_map;
		}
		break;
#endif
	}

	if (map->format.format_write) {
		if ((reg_endian != REGMAP_ENDIAN_BIG) ||
		    (val_endian != REGMAP_ENDIAN_BIG))
			goto err_map;
		map->use_single_write = true;
	}

	if (!map->format.format_write &&
	    !(map->format.format_reg && map->format.format_val))
		goto err_map;

	map->work_buf = calloc(1, map->format.buf_size);
	if (map->work_buf == NULL) {
		ret = -ENOMEM;
		goto err_map;
	}

	if (map->format.format_write) {
//		map->defer_caching = false;
		map->reg_write = _regmap_bus_formatted_write;
	} else if (map->format.format_val) {
//		map->defer_caching = true;
		map->reg_write = _regmap_bus_raw_write;
	}

//skip_format_initialization:

//	if (dev) {
//		ret = regmap_attach_dev(dev, map, config);
//		if (ret != 0)
//			goto err_regcache;
//	}

	return map;

err_map:
	free(map);
err:
	return ERR_PTR(ret);
}

struct regmap *regmap_init(const struct regmap_bus *bus, void *bus_context, const struct regmap_config *config)
{

	return __regmap_init(NULL, bus, bus_context, config);
}
