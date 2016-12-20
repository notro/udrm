
// memcpy
#include <string.h>

#include "backlight.h"
#include "mipi-dbi.h"
#include "mipi_display.h"
#include "regmap.h"

#define DCS_POWER_MODE_DISPLAY			BIT(2)
#define DCS_POWER_MODE_DISPLAY_NORMAL_MODE	BIT(3)
#define DCS_POWER_MODE_SLEEP_MODE		BIT(4)
#define DCS_POWER_MODE_PARTIAL_MODE		BIT(5)
#define DCS_POWER_MODE_IDLE_MODE		BIT(6)
#define DCS_POWER_MODE_RESERVED_MASK		(BIT(0) | BIT(1) | BIT(7))


static const uint32_t mipi_dbi_formats[] = {
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XRGB8888,
};

int mipi_dbi_register(struct device *dev, struct mipi_dbi *mipi, const char *name, const struct udrm_funcs *funcs,
		      const struct drm_mode_modeinfo *mode, unsigned int rotation)
{
	struct udrm_device *udev = &mipi->udev;
	u32 buf_mode = UDRM_BUF_MODE_EMUL_XRGB8888;
	int ret;

	udev->dev = dev;
	udev->funcs = funcs;

	//if (mipi->)
	buf_mode |= UDRM_BUF_MODE_SWAP_BYTES;

	ret = udrm_register(udev, name, mode, mipi_dbi_formats, ARRAY_SIZE(mipi_dbi_formats), buf_mode);

	return ret;
}

int mipi_dbi_dirtyfb(struct udrm_framebuffer *ufb, unsigned int flags, unsigned int color, struct drm_clip_rect *clips, unsigned int num_clips)
{
	struct udrm_device *udev = ufb->udev;
	struct mipi_dbi *mipi = mipi_dbi_from_tinydrm(udev);
	struct regmap *reg = mipi->reg;
	int ret;

	if (num_clips != 1)
		return -EINVAL;

	DRM_DEBUG("Flushing [FB:%d] x1=%u, x2=%u, y1=%u, y2=%u\n", ufb->id,
		  clips->x1, clips->x2, clips->y1, clips->y2);

	mipi_dbi_write(reg, MIPI_DCS_SET_COLUMN_ADDRESS,
		       (clips->x1 >> 8) & 0xFF, clips->x1 & 0xFF,
		       (clips->x2 >> 8) & 0xFF, (clips->x2 - 1) & 0xFF);
	mipi_dbi_write(reg, MIPI_DCS_SET_PAGE_ADDRESS,
		       (clips->y1 >> 8) & 0xFF, clips->y1 & 0xFF,
		       (clips->y2 >> 8) & 0xFF, (clips->y2 - 1) & 0xFF);


	if (ufb->dmabuf) {
		DRM_ERROR("ufb->dmabuf not supported\n");
		return -EINVAL;
	} else if (udev->dmabuf) {
		size_t len = (clips->x2 - clips->x1) * (clips->y2 - clips->y1) * 2;

		DRM_DEBUG("BBBUFFER\n");

		ret = regmap_raw_write(reg, MIPI_DCS_WRITE_MEMORY_START, udev->dmabuf, len);
		if (ret)
			return ret;


	} else {
		DRM_ERROR("No buffer\n");
		return -EINVAL;
	}



//	ret = tinydrm_regmap_flush_rgb565(reg, MIPI_DCS_WRITE_MEMORY_START,
//					  fb, cma_obj->vaddr, &clip);
//	if (ret)
//		return ret;

	if (!udev->enabled) {
		if (mipi->enable_delay_ms)
			msleep(mipi->enable_delay_ms);
		ret = backlight_enable(mipi->backlight);
		if (ret) {
			DRM_ERROR("Failed to enable backlight %d\n", ret);
			return ret;
		}
		udev->enabled = true;
	}

	return 0;
}

void mipi_dbi_disable(struct udrm_device *udev)
{
	struct mipi_dbi *mipi = mipi_dbi_from_tinydrm(udev);

	DRM_DEBUG_KMS("Disable\n");

	if (udev->enabled) {
		if (mipi->backlight)
			backlight_disable(mipi->backlight);
//		else
//			mipi_dbi_blank(mipi);
	}
	udev->enabled = false;
}

void mipi_dbi_hw_reset(struct mipi_dbi *mipi)
{
	if (!mipi->reset)
		return;

	gpiod_set_value(mipi->reset, 0);
	usleep(20);
	gpiod_set_value(mipi->reset, 1);
	msleep(120);
}

bool mipi_dbi_display_is_on(struct regmap *reg)
{
	u8 val;

	if (regmap_raw_read(reg, MIPI_DCS_GET_POWER_MODE, &val, 1))
		return false;

	val &= ~DCS_POWER_MODE_RESERVED_MASK;

	if (val != (DCS_POWER_MODE_DISPLAY |
	    DCS_POWER_MODE_DISPLAY_NORMAL_MODE | DCS_POWER_MODE_SLEEP_MODE))
		return false;

	DRM_DEBUG_DRIVER("Display is ON\n");

	return true;
}

int mipi_dbi_write_buf(struct regmap *reg, unsigned int cmd,
		       const u8 *parameters, size_t num)
{
	u8 *buf;
	int ret;

	buf = malloc(num);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, parameters, num);

	ret = regmap_raw_write(reg, cmd, buf, num);
	free(buf);

	return ret;
}
