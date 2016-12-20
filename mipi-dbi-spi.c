#include <string.h>

#include "mipi-dbi-spi.h"
#include "gpio.h"
#include "regmap.h"

#define MIPI_DBI_DEFAULT_SPI_READ_SPEED 2000000 /* 2MHz */

struct mipi_dbi_spi {
	struct spi_device *spi;
	struct regmap *map;
	unsigned int ram_reg;
	struct gpio_desc *dc;
	bool write_only;
	u16 *tx_buf;
	size_t chunk_size;
};






/* MIPI DBI Type C Option 3 */

static int mipi_dbi_spi3_gather_write(void *context, const void *reg,
				      size_t reg_len, const void *val,
				      size_t val_len)
{
	struct mipi_dbi_spi *mspi = context;
	struct spi_device *spi = mspi->spi;
	size_t val_width;
	int ret;

	if (reg_len != 1)
		return -EINVAL;

	/* FIXME */
	if (1)
		val_width = 8;
	else
		val_width = (*(u8 *)reg == mspi->ram_reg) ? 16 : 8;
	TINYDRM_DEBUG_REG_WRITE(reg, reg_len, val, val_len, val_width);

	gpiod_set_value(mspi->dc, 0);
	ret = spi_transfer(spi, 0, NULL, 8, reg, 1, mspi->tx_buf, mspi->chunk_size);
	if (ret)
		return ret;

	if (val && val_len) {
		gpiod_set_value(mspi->dc, 1);
		ret = spi_transfer(spi, 0, NULL, val_width, val, val_len, mspi->tx_buf, mspi->chunk_size);
	}

	return ret;
}

static int mipi_dbi_spi3_write(void *context, const void *data, size_t count)
{
	return mipi_dbi_spi3_gather_write(context, data, 1,
					  data + 1, count - 1);
}

static int mipi_dbi_spi3_read(void *context, const void *reg, size_t reg_len,
			      void *val, size_t val_len)
{
	struct mipi_dbi_spi *mspi = context;
	struct spi_device *spi = mspi->spi;
	u32 speed_hz = min_t(u32, MIPI_DBI_DEFAULT_SPI_READ_SPEED,
			     spi->max_speed_hz / 2);
	struct spi_ioc_transfer tr[2] = {
		{
			.speed_hz = speed_hz,
			.tx_buf = (unsigned long)reg,
			.len = 1,
		}, {
			.speed_hz = speed_hz,
			.len = val_len,
		},
	};
	u8 cmd = *(u8 *)reg;
	u8 *buf;
	int ret;

	if (mspi->write_only)
		return -EACCES;

	if (udrm_debug & DRM_UT_CORE)
		pr_debug("[drm:%s] regnr=0x%02x, len=%zu, transfers:\n",
			 __func__, cmd, val_len);

	/*
	 * Support non-standard 24-bit and 32-bit Nokia read commands which
	 * start with a dummy clock, so we need to read an extra byte.
	 */
	if (cmd == MIPI_DCS_GET_DISPLAY_ID ||
	    cmd == MIPI_DCS_GET_DISPLAY_STATUS) {
		if (!(val_len == 3 || val_len == 4))
			return -EINVAL;

		tr[1].len = val_len + 1;
	}

	buf = malloc(tr[1].len);
	if (!buf)
		return -ENOMEM;

	tr[1].rx_buf = (unsigned long)buf;
	gpiod_set_value(mspi->dc, 0);

	/*
	 * Can't use spi_write_then_read() because reading speed is slower
	 * than writing speed and that is set on the transfer.
	 */
	ret = spi_sync(spi, tr, 2);
	spi_message_dbg(spi, tr, 2);

	if (tr[1].len == val_len) {
		memcpy(val, buf, val_len);
	} else {
		u8 *data = val;
		unsigned int i;

		for (i = 0; i < val_len; i++)
			data[i] = (buf[i] << 1) | !!(buf[i + 1] & BIT(7));
	}
	free(buf);

	return ret;
}

/* MIPI DBI Type C Option 3 */
static const struct regmap_bus mipi_dbi_regmap_bus3 = {
	.write = mipi_dbi_spi3_write,
	.gather_write = mipi_dbi_spi3_gather_write,
	.read = mipi_dbi_spi3_read,
	.reg_format_endian_default = REGMAP_ENDIAN_DEFAULT,
	.val_format_endian_default = REGMAP_ENDIAN_DEFAULT,
};

struct regmap *mipi_dbi_spi_init(struct spi_device *spi, struct gpio_desc *dc, bool write_only)
{
	struct regmap_config config = {
		.reg_bits = 8,
		.val_bits = 8,
		.cache_type = REGCACHE_NONE,
	};
	struct mipi_dbi_spi *mspi;

	mspi = calloc(1, sizeof(*mspi));
	if (!mspi)
		return ERR_PTR(-ENOMEM);

	mspi->chunk_size = spi_max_transfer_size(spi, 0);
	mspi->ram_reg = MIPI_DCS_WRITE_MEMORY_START;
	mspi->write_only = write_only;
	mspi->spi = spi;
	mspi->dc = dc;

//	if (regmap_get_machine_endian() == REGMAP_ENDIAN_LITTLE &&
//	    dc && !spi_bpw_supported(spi, 16)) {
//		mspi->tx_buf = calloc(1, mspi->chunk_size);
//		if (!mspi->tx_buf)
//			return ERR_PTR(-ENOMEM);
//	}

	if (dc)
		mspi->map = regmap_init(&mipi_dbi_regmap_bus3, mspi, &config);
//	else
//		mspi->map = regmap_init(&mipi_dbi_regmap_bus1, mspi, &config);

	return mspi->map;
}
