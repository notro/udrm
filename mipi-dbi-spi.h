#ifndef __MIPI_DBI_SPI_H
#define __MIPI_DBI_SPI_H

#include "mipi-dbi.h"
#include "spi.h"

struct regmap *mipi_dbi_spi_init(struct spi_device *spi, struct gpio_desc *dc, bool write_only);

#endif
