// memset
#include <string.h>

// snprintf
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/dma-buf-dev.h>
#include <linux/limits.h>
#include <sys/ioctl.h>
#include <poll.h>

// munmap
#include <sys/mman.h>

#include "udrm.h"

int udrm_debug = 0xff;

static int udrm_create_dma_buf(size_t size)
{
	struct dma_buf_dev_create create = {
		.attrs = DMA_BUF_DEV_ATTR_WRITE_COMBINE,
		.size = size,
		.flags = O_RDWR,
	};
	int fd, ret;

	fd = open("/dev/dma-buf", O_RDWR);
	if (fd < 0) {
		pr_err("%s: Failed to open /dev/dma-buf: %s\n", __func__, strerror(errno));
		return -errno;
	}

	ret = ioctl(fd, DMA_BUF_DEV_IOCTL_CREATE, &create);
	if (ret < 0) {
		pr_err("%s: Failed to create dma-buf: %s\n", __func__, strerror(errno));
		ret = -errno;
		goto out;
	}

	ret = create.fd;
out:
	close(fd);

	return ret;
}

int udrm_register(struct udrm_device *udev, const char *name, const struct drm_mode_modeinfo *mode,
		  const uint32_t *formats, unsigned int num_formats, u32 buf_mode)
{
	struct udrm_dev_create udev_create;
	char ctrl_fname[PATH_MAX];
	int ret, dmabuf_fd;

	udev->name = name;
	udev->mode = mode;

	/* FIXME */
	dmabuf_fd = udrm_create_dma_buf(320 * 240 * 2);
	if (dmabuf_fd < 0)
		return dmabuf_fd;

	memset(&udev_create, 0, sizeof(udev_create));
	udev_create.buf_fd = dmabuf_fd;
	udev_create.mode = *mode;
	udev_create.formats = (unsigned long)formats;
	udev_create.num_formats = num_formats;
	udev_create.buf_mode = buf_mode;
	strncpy(udev_create.name, name, UDRM_MAX_NAME_SIZE);

	udev->fd = open("/dev/udrm", O_RDWR);
	if (udev->fd < 0) {
		pr_err("%s: Failed to open /dev/udrm: %s\n", __func__, strerror(errno));
		return -errno;
	}

	ret = ioctl(udev->fd, UDRM_DEV_CREATE, &udev_create);
	if (ret < 0) {
		pr_err("%s: Failed to create device: %s\n", __func__, strerror(errno));
		close(udev->fd);
		return -errno;
	}

	snprintf(ctrl_fname, sizeof(ctrl_fname), "/dev/dri/controlD%d", udev_create.index + 64);
	DRM_DEBUG("DRM index: %d, ctrl_fname=%s\n", udev_create.index, ctrl_fname);

	udev->control_fd = open(ctrl_fname, O_RDWR);
	if (udev->control_fd == -1) {
		pr_err("%s: Failed to open /dev/dri/...: %s\n", __func__, strerror(errno));
		close(udev->fd);
		return -errno;
	}

	if (udev_create.buf_fd >= 0) {
		udev->dmabuf = dma_buf_get(udev_create.buf_fd);
		if (IS_ERR(udev->dmabuf)) {
			close(udev->control_fd);
			close(udev->fd);
			return PTR_ERR(udev->dmabuf);
		}
	}

	DRM_DEBUG_KMS("buf_fd=%d\n", udev_create.buf_fd);

	return 0;
}

void udrm_unregister(struct udrm_device *udev)
{
	if (udev->dmabuf)
		dma_buf_put(udev->dmabuf);
	close(udev->control_fd);
	close(udev->fd);
}








static int udrm_enable(struct udrm_device *udev, struct udrm_event *ev)
{
	DRM_DEBUG("Enable\n");

	if (udev->funcs && udev->funcs->enable)
		udev->funcs->enable(udev);

	return 0;
}

static int udrm_disable(struct udrm_device *udev, struct udrm_event *ev)
{
	DRM_DEBUG("Disable\n");

	if (udev->funcs && udev->funcs->disable)
		udev->funcs->disable(udev);

	return 0;
}

static uint32_t drm_mode_legacy_fb_format(uint32_t bpp, uint32_t depth)
{
	uint32_t fmt;

	switch (bpp) {
	case 8:
		fmt = DRM_FORMAT_C8;
		break;
	case 16:
		if (depth == 15)
			fmt = DRM_FORMAT_XRGB1555;
		else
			fmt = DRM_FORMAT_RGB565;
		break;
	case 24:
		fmt = DRM_FORMAT_RGB888;
		break;
	case 32:
		if (depth == 24)
			fmt = DRM_FORMAT_XRGB8888;
		else if (depth == 30)
			fmt = DRM_FORMAT_XRGB2101010;
		else
			fmt = DRM_FORMAT_ARGB8888;
		break;
	default:
		DRM_ERROR("bad bpp, assuming x8r8g8b8 pixel format\n");
		fmt = DRM_FORMAT_XRGB8888;
		break;
	}

	return fmt;
}

static int udrm_fb_create(struct udrm_device *udev, struct udrm_event_fb *ev)
{
	struct drm_mode_fb_cmd info = {
		.fb_id = ev->fb_id,
	};
	int ret;
//	struct drm_prime_handle prime;
	struct udrm_framebuffer *ufb;

	ufb = calloc(1, sizeof(*ufb));
	if (!ufb)
		return -ENOMEM;

	ufb->id = info.fb_id;

	ret = ioctl(udev->control_fd, DRM_IOCTL_MODE_GETFB, &info);
	if (ret == -1) {
		DRM_ERROR("[FB:%u] Failed to get framebuffer info: %s\n", ev->fb_id, strerror(errno));
		ret = -errno;
		goto error;
	}

	ufb->udev = udev;
	ufb->handle = info.handle;
	ufb->pitch = info.pitch;
	ufb->width = info.width;
	ufb->height = info.height;
	//fb->bpp = info.bpp;
	//fb->depth = info.depth
	ufb->pixel_format = drm_mode_legacy_fb_format(info.bpp, info.depth);

	/* FIXME: emulation */
	if (ufb->pixel_format == DRM_FORMAT_XRGB8888)
		ufb->pixel_format = DRM_FORMAT_RGB565;

	DRM_DEBUG("[FB:%u] Create: %ux%u, handle=%u\n", ufb->id, ufb->width, ufb->height, ufb->handle);

	ufb->next = udev->fbs;
	udev->fbs = ufb;

	return 0;

error:
	free(ufb);

	return ret;
}

static int udrm_fb_destroy(struct udrm_device *udev, struct udrm_event_fb *ev)
{
	struct udrm_framebuffer **prev, **curr, *ufb = NULL;

	DRM_DEBUG("[FB:%u] Destroy\n", ev->fb_id);

	for (prev = NULL, curr = &udev->fbs; *curr != NULL; prev = curr, curr = &(*curr)->next) {
		//printf("(*curr)->id=%u, *curr=%p, *curr->next=%p\n", (*curr)->id, *curr, (*curr)->next);
		if ((*curr)->id == ev->fb_id) {
			ufb = *curr;
			if (prev)
				(*prev)->next = (*curr)->next;
			else
				(*curr) = (*curr)->next;
			break;
		}
	}

	if (!ufb) {
		pr_err("%s: Failed to find framebuffer %u\n", __func__, ev->fb_id);
		return -EINVAL;
	}

	free(ufb);

	return 0;
}

static int udrm_fb_dirty(struct udrm_device *udev, struct udrm_event_fb_dirty *ev)
{
	struct drm_mode_fb_dirty_cmd *dirty = &ev->fb_dirty_cmd;
	struct udrm_framebuffer *ufb;
	int ret = 0;

	for (ufb = udev->fbs; ufb != NULL; ufb = ufb->next) {
		if (ufb->id == ev->fb_dirty_cmd.fb_id)
			break;
	}

	if (!ufb) {
		DRM_ERROR("[FB:%u] Framebuffer not found\n", ev->fb_dirty_cmd.fb_id);
		return -EINVAL;
	}

	DRM_DEBUG("[FB:%u] Dirty\n", ufb->id);

	if (udev->funcs && udev->funcs->dirtyfb)
		ret = udev->funcs->dirtyfb(ufb, dirty->flags, dirty->color, ev->clips, dirty->num_clips);

	return ret;
}

static int udrm_event(struct udrm_device *udev, struct udrm_event *ev)
{
	int ret;

//	usleep(100000);

	switch (ev->type) {
	case UDRM_EVENT_PIPE_ENABLE:
		ret = udrm_enable(udev, ev);
		break;
	case UDRM_EVENT_PIPE_DISABLE:
		ret = udrm_disable(udev, ev);
		break;
	case UDRM_EVENT_FB_CREATE:
		ret = udrm_fb_create(udev, (struct udrm_event_fb *)ev);
		break;
	case UDRM_EVENT_FB_DESTROY:
		ret = udrm_fb_destroy(udev, (struct udrm_event_fb *)ev);
		break;
	case UDRM_EVENT_FB_DIRTY:
		ret = udrm_fb_dirty(udev, (struct udrm_event_fb_dirty *)ev);
		break;
	default:
		printf("Unknown event: %u\n", ev->type);
		ret = -ENOSYS;
		break;
	}

//	usleep(100000);

	return ret;
}


int udrm_event_loop(struct udrm_device *udev)
{
	struct udrm_event *ev;
	struct pollfd pfd;
	int ret;

	ev = malloc(1024);
	if (!ev) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	pfd.fd = udev->fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	while (!(pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
		int event_ret;

		ret = read(udev->fd, ev, 1024);
		if (ret < 0) {
			pr_err("%s: Failed to read from /dev/udrm: %s\n", __func__, strerror(errno));
			ret = -errno;
			goto out;
		}

		event_ret = udrm_event(udev, ev);

		ret = write(udev->fd, &event_ret, sizeof(int));
		if (ret < 0) {
			pr_err("%s: Failed to write to /dev/udrm: %s\n", __func__, strerror(errno));
			ret = -errno;
			goto out;
		}

		poll(&pfd, 1, -1);
	}

out:
	free(ev);

	return ret;
}
