
#ifndef _BASE_H
#define _BASE_H


#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

// offsetof
#include <stddef.h>

#include <stdint.h>
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;


#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#include <unistd.h>
#define msleep(x) usleep(x * 1000)

#define KERN_EMERG      0    /* system is unusable */
#define KERN_ALERT      1    /* action must be taken immediately */
#define KERN_CRIT       2    /* critical conditions */
#define KERN_ERR        3    /* error conditions */
#define KERN_WARNING    4    /* warning conditions */
#define KERN_NOTICE     5    /* normal but significant condition */
#define KERN_INFO       6    /* informational */
#define KERN_DEBUG      7    /* debug-level messages */

extern unsigned int printk_level;

int printk(int level, const char *fmt, ...);

#define pr_err(fmt, ...) \
	printk(KERN_ERR, fmt, ##__VA_ARGS__)
#define pr_warning(fmt, ...) \
	printk(KERN_WARNING, fmt, ##__VA_ARGS__)
#define pr_warn pr_warning
#define pr_notice(fmt, ...) \
	printk(KERN_NOTICE, fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...) \
	printk(KERN_INFO, fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) \
	printk(KERN_DEBUG, fmt, ##__VA_ARGS__)





#define BIT(nr)		(1UL << (nr))

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#define min(x, y) ({                            \
	typeof(x) _min1 = (x);                  \
	typeof(y) _min2 = (y);                  \
	(void) (&_min1 == &_min2);              \
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({                            \
	typeof(x) _max1 = (x);                  \
	typeof(y) _max2 = (y);                  \
	(void) (&_max1 == &_max2);              \
	_max1 > _max2 ? _max1 : _max2; })

#define min_t(type, x, y) ({                    \
	type __min1 = (x);                      \
	type __min2 = (y);                      \
	__min1 < __min2 ? __min1: __min2; })

#define max_t(type, x, y) ({                    \
	type __max1 = (x);                      \
	type __max2 = (y);                      \
	__max1 > __max2 ? __max1: __max2; })

#define swab16(x) ((__u16)(                     \
	(((__u16)(x) & (__u16)0x00ffU) << 8) |  \
	(((__u16)(x) & (__u16)0xff00U) >> 8)))


#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))


#define MAX_ERRNO       4095

#define IS_ERR_VALUE(x) ((unsigned long)(void *)(x) >= (unsigned long)-MAX_ERRNO)

static inline void * ERR_PTR(long error)
{
	return (void *) error;
}

static inline long PTR_ERR(const void *ptr)
{
	return (long) ptr;
}

static inline bool IS_ERR(const void *ptr)
{
	return IS_ERR_VALUE((unsigned long)ptr);
}

static inline void *ERR_CAST(const void *ptr)
{
	/* cast away the const */
	return (void *) ptr;
}

#endif
