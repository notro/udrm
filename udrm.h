#ifndef _UDRM_H
#define _UDRM_H

// drm_clip_rect
#include <drm/drm.h>

#include <drm/drm_fourcc.h>
#include <drm/drm_mode.h>
#include <drm/udrm.h>


#include "base.h"
#include "dmabuf.h"




struct udrm_device;

struct udrm_framebuffer {
	struct udrm_device *udev;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	uint32_t pixel_format; /* fourcc format */

	unsigned int id;
	unsigned int handle;
	struct dma_buf *dmabuf;

	struct udrm_framebuffer *next;
};


struct udrm_funcs {
	void (*enable)(struct udrm_device *udev);
	void (*disable)(struct udrm_device *udev);

	int (*dirtyfb)(struct udrm_framebuffer *ufb,
		       unsigned int flags, unsigned int color,
		       struct drm_clip_rect *clips,
		       unsigned int num_clips);
};

struct udrm_device {
	struct device *dev;

	int index;
	int fd;
	int control_fd;

	const char *name;
	const struct drm_mode_modeinfo *mode;

	bool prepared;
	bool enabled;
	const struct udrm_funcs *funcs;

	struct udrm_framebuffer *fbs;

	struct dma_buf *dmabuf;
};


int udrm_register(struct udrm_device *udev, const char *name, const struct drm_mode_modeinfo *mode,
		  const uint32_t *formats, unsigned int num_formats, u32 buf_mode);
void udrm_unregister(struct udrm_device *udev);

int udrm_event_loop(struct udrm_device *udev);



#define UDRM_MODE(hd, vd, hd_mm, vd_mm) \
	.hdisplay = (hd), \
	.hsync_start = (hd), \
	.hsync_end = (hd), \
	.htotal = (hd), \
	.vdisplay = (vd), \
	.vsync_start = (vd), \
	.vsync_end = (vd), \
	.vtotal = (vd), \
	.type = DRM_MODE_TYPE_DRIVER, \
	.clock = 1 /* pass validation */






#define DRM_UT_NONE		0x00
#define DRM_UT_CORE 		0x01
#define DRM_UT_DRIVER		0x02
#define DRM_UT_KMS		0x04
#define DRM_UT_PRIME		0x08
#define DRM_UT_ATOMIC		0x10
#define DRM_UT_VBL		0x20

extern int udrm_debug;

#define drm_printk(level, category, fmt, ...)				\
({									\
	if (!(category != DRM_UT_NONE && !(udrm_debug & category)))	\
		printk(level, "[udrm:%s] " fmt, __func__, ##__VA_ARGS__);	\
})

#define DRM_DEBUG_KMS(fmt, ...)						\
	drm_printk(KERN_DEBUG, DRM_UT_KMS, fmt, ##__VA_ARGS__)

#define DRM_ERROR(fmt, ...)						\
	drm_printk(KERN_ERR, DRM_UT_NONE, fmt, ##__VA_ARGS__)

#define DRM_INFO(fmt, ...)						\
	drm_printk(KERN_INFO, DRM_UT_NONE, fmt, ##__VA_ARGS__)

#define DRM_DEBUG(fmt, ...)						\
	drm_printk(KERN_DEBUG, DRM_UT_CORE, fmt, ##__VA_ARGS__)

#define DRM_DEBUG_DRIVER(fmt, ...)					\
	drm_printk(KERN_DEBUG, DRM_UT_DRIVER, fmt, ##__VA_ARGS__)

#endif
