#ifndef _SPI_H
#define _SPI_H

#include <linux/spi/spidev.h>

#include "device.h"
#include "udrm.h"

struct spi_device {
	struct device		dev;
	char			*fname;
	int			fd;
	u32			max_speed_hz;
	u8			chip_select;
	u8			bits_per_word;

	u32			bits_per_word_mask;
#define SPI_BPW_MASK(bits) BIT((bits) - 1)
#define SPI_BIT_MASK(bits) (((bits) == 32) ? ~0U : (BIT(bits) - 1))
#define SPI_BPW_RANGE_MASK(min, max) (SPI_BIT_MASK(max) - SPI_BIT_MASK(min - 1))

	size_t			max_len;
	size_t			max_dma_len;
};


int spi_add_device(struct spi_device *spi);
void spi_unregister_device(struct spi_device *spi);

size_t spi_max_transfer_size(struct spi_device *spi, size_t max_len);

static inline void spi_set_drvdata(struct spi_device *spi, void *data)
{
	dev_set_drvdata(&spi->dev, data);
}

static inline void *spi_get_drvdata(struct spi_device *spi)
{
	return dev_get_drvdata(&spi->dev);
}

static inline bool spi_bpw_supported(struct spi_device *spi, u8 bpw)
{
	return spi->bits_per_word_mask & SPI_BPW_MASK(bpw);
}


void _spi_message_dbg(struct spi_device *spi, struct spi_ioc_transfer *msg, unsigned int num_msgs);
static inline void spi_message_dbg(struct spi_device *spi, struct spi_ioc_transfer *m, unsigned int num_msgs)
{
	if (udrm_debug & DRM_UT_CORE)
		_spi_message_dbg(spi, m, num_msgs);
}

int spi_transfer(struct spi_device *spi, u32 speed_hz, struct spi_ioc_transfer *header, u8 bpw,
		 const void *buf, size_t len, u16 *swap_buf, size_t max_chunk);

int spi_sync(struct spi_device *spi, struct spi_ioc_transfer *msg, unsigned int num_msgs);



/* FIXME */
void tinydrm_debug_reg_write(const void *reg, size_t reg_len, const void *val, size_t val_len, size_t val_width);
#define TINYDRM_DEBUG_REG_WRITE(reg, reg_len, val, val_len, val_width)			\
	do {										\
		if (udrm_debug & DRM_UT_CORE)						\
			tinydrm_debug_reg_write(reg, reg_len, val, val_len, val_width);	\
	} while (0)

#endif
