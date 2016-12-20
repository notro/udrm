
#if 0
#define printk(level, fmt, ...)						\
({									\
	struct timespec __ts;						\
									\
	clock_gettime(CLOCK_MONOTONIC, &__ts);				\
	if (level <= printk_level)					\
		printf("[%ld.%09ld] " fmt, __ts.tv_sec, __ts.tv_nsec, ##__VA_ARGS__);	\
})
#endif

#include <stdarg.h>
#include <time.h>
#include <stdio.h>

#include "base.h"

unsigned int printk_level = 100;

int printk(int level, const char *format, ...)
{
	struct timespec ts;
	va_list arg;
	int done;

	if (level > printk_level)
		return 0;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	va_start(arg, format);
	done = vfprintf(stdout, format, arg);
	va_end(arg);

	return done;
}
