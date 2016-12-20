#ifndef __REGMAP_H
#define __REGMAP_H

#include "base.h"

enum regcache_type {
	REGCACHE_NONE,
	REGCACHE_RBTREE,
	REGCACHE_COMPRESSED,
	REGCACHE_FLAT,
};

enum regmap_endian {
	/* Unspecified -> 0 -> Backwards compatible default */
	REGMAP_ENDIAN_DEFAULT = 0,
	REGMAP_ENDIAN_BIG,
	REGMAP_ENDIAN_LITTLE,
	REGMAP_ENDIAN_NATIVE,
};

struct regmap;

struct regmap_format {
	size_t buf_size;
	size_t reg_bytes;
	size_t pad_bytes;
	size_t val_bytes;
	void (*format_write)(struct regmap *map,
			     unsigned int reg, unsigned int val);
	void (*format_reg)(void *buf, unsigned int reg, unsigned int shift);
	void (*format_val)(void *buf, unsigned int val, unsigned int shift);
	unsigned int (*parse_val)(const void *buf);
	void (*parse_inplace)(void *buf);
};

struct regmap {
#if 0
	union {
		struct mutex mutex;
		struct {
			spinlock_t spinlock;
			unsigned long spinlock_flags;
		};
	};
	regmap_lock lock;
	regmap_unlock unlock;
	void *lock_arg; /* This is passed to lock/unlock functions */
	gfp_t alloc_flags;
#endif
//	struct device *dev; /* Device we do I/O on */
	void *work_buf;     /* Scratch buffer used to format I/O */
	struct regmap_format format;  /* Buffer format */
	const struct regmap_bus *bus;
	void *bus_context;
#if 0
	const char *name;

	bool async;
	spinlock_t async_lock;
	wait_queue_head_t async_waitq;
	struct list_head async_list;
	struct list_head async_free;
	int async_ret;

	unsigned int max_register;
	bool (*writeable_reg)(struct device *dev, unsigned int reg);
	bool (*readable_reg)(struct device *dev, unsigned int reg);
	bool (*volatile_reg)(struct device *dev, unsigned int reg);
	bool (*precious_reg)(struct device *dev, unsigned int reg);
	const struct regmap_access_table *wr_table;
	const struct regmap_access_table *rd_table;
	const struct regmap_access_table *volatile_table;
	const struct regmap_access_table *precious_table;
#endif
	int (*reg_read)(void *context, unsigned int reg, unsigned int *val);
	int (*reg_write)(void *context, unsigned int reg, unsigned int val);
//	int (*reg_update_bits)(void *context, unsigned int reg,
//			       unsigned int mask, unsigned int val);
//
//	bool defer_caching;
	unsigned long read_flag_mask;
	unsigned long write_flag_mask;

	/* number of bits to (left) shift the reg value when formatting*/
	int reg_shift;
	int reg_stride;
//	int reg_stride_order;

	/* regcache specific members */
//	const struct regcache_ops *cache_ops;
	enum regcache_type cache_type;
#if 0
	/* number of bytes in reg_defaults_raw */
	unsigned int cache_size_raw;
	/* number of bytes per word in reg_defaults_raw */
	unsigned int cache_word_size;
	/* number of entries in reg_defaults */
	unsigned int num_reg_defaults;
	/* number of entries in reg_defaults_raw */
	unsigned int num_reg_defaults_raw;

	/* if set, only the cache is modified not the HW */
	bool cache_only;
	/* if set, only the HW is modified not the cache */
	bool cache_bypass;
	/* if set, remember to free reg_defaults_raw */
	bool cache_free;

	struct reg_default *reg_defaults;
	const void *reg_defaults_raw;
	void *cache;
	/* if set, the cache contains newer data than the HW */
	bool cache_dirty;
	/* if set, the HW registers are known to match map->reg_defaults */
	bool no_sync_defaults;

	struct reg_sequence *patch;
	int patch_regs;
#endif
	/* if set, converts bulk read to single read */
	bool use_single_read;
	/* if set, converts bulk read to single read */
	bool use_single_write;
	/* if set, the device supports multi write mode */
	bool can_multi_write;

	/* if set, raw reads/writes are limited to this size */
	size_t max_raw_read;
	size_t max_raw_write;

//	struct rb_root range_tree;
//	void *selector_work_buf;	/* Scratch buffer used for selector */
};

//struct regmap_async;

typedef int (*regmap_hw_write)(void *context, const void *data,
			       size_t count);
typedef int (*regmap_hw_gather_write)(void *context,
				      const void *reg, size_t reg_len,
				      const void *val, size_t val_len);
//typedef int (*regmap_hw_async_write)(void *context,
//				     const void *reg, size_t reg_len,
//				     const void *val, size_t val_len,
//				     struct regmap_async *async);
typedef int (*regmap_hw_read)(void *context,
			      const void *reg_buf, size_t reg_size,
			      void *val_buf, size_t val_size);
//typedef int (*regmap_hw_reg_read)(void *context, unsigned int reg,
//				  unsigned int *val);
typedef int (*regmap_hw_reg_write)(void *context, unsigned int reg,
				   unsigned int val);
//typedef int (*regmap_hw_reg_update_bits)(void *context, unsigned int reg,
//					 unsigned int mask, unsigned int val);
//typedef struct regmap_async *(*regmap_hw_async_alloc)(void);
//typedef void (*regmap_hw_free_context)(void *context);

struct regmap_bus {
//	bool fast_io;
	regmap_hw_write write;
	regmap_hw_gather_write gather_write;
//	regmap_hw_async_write async_write;
	regmap_hw_reg_write reg_write;
//	regmap_hw_reg_update_bits reg_update_bits;
	regmap_hw_read read;
//	regmap_hw_reg_read reg_read;
//	regmap_hw_free_context free_context;
//	regmap_hw_async_alloc async_alloc;
//	u8 read_flag_mask;
	enum regmap_endian reg_format_endian_default;
	enum regmap_endian val_format_endian_default;
	size_t max_raw_read;
	size_t max_raw_write;
};

struct regmap_config {
//	const char *name;

	int reg_bits;
	int reg_stride;
	int pad_bits;
	int val_bits;

//	bool (*writeable_reg)(struct device *dev, unsigned int reg);
//	bool (*readable_reg)(struct device *dev, unsigned int reg);
//	bool (*volatile_reg)(struct device *dev, unsigned int reg);
//	bool (*precious_reg)(struct device *dev, unsigned int reg);
//	regmap_lock lock;
//	regmap_unlock unlock;
//	void *lock_arg;

//	int (*reg_read)(void *context, unsigned int reg, unsigned int *val);
//	int (*reg_write)(void *context, unsigned int reg, unsigned int val);

//	bool fast_io;

//	unsigned int max_register;
//	const struct regmap_access_table *wr_table;
//	const struct regmap_access_table *rd_table;
//	const struct regmap_access_table *volatile_table;
//	const struct regmap_access_table *precious_table;
//	const struct reg_default *reg_defaults;
//	unsigned int num_reg_defaults;
	enum regcache_type cache_type;
//	const void *reg_defaults_raw;
//	unsigned int num_reg_defaults_raw;

//	unsigned long read_flag_mask;
//	unsigned long write_flag_mask;

	bool use_single_rw;
	bool can_multi_write;

//	enum regmap_endian reg_format_endian;
//	enum regmap_endian val_format_endian;

//	const struct regmap_range_cfg *ranges;
//	unsigned int num_ranges;
};

int regmap_raw_write(struct regmap *map, unsigned int reg, const void *val, size_t val_len);

int regmap_raw_read(struct regmap *map, unsigned int reg, void *val, size_t val_len);

struct regmap *regmap_init(const struct regmap_bus *bus, void *bus_context, const struct regmap_config *config);

static inline enum regmap_endian regmap_get_machine_endian(void)
{
#if defined(__LITTLE_ENDIAN)
	return REGMAP_ENDIAN_LITTLE;
#else
	return REGMAP_ENDIAN_BIG;
#endif
}

#endif
