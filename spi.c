#include <stdio.h>
#include <string.h>

// isascii
#include <ctype.h>

// file
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>

#include "udrm.h"
#include "spi.h"
#include "regmap.h"

int spi_register_driver(struct spi_driver *sdrv)
{
	char buf[128 + 128];
	const char *str;
	int fd, ret;

	pr_debug("%s\n", __func__);

	if (strlen(sdrv->name) > 127)
		return -EINVAL;

	if (sdrv->compatible && strlen(sdrv->compatible) > 127)
		return -EINVAL;

	fd = open("/dev/spidev", O_RDWR);
	if (fd < 0) {
		printf("%s: Failed to open /dev/spidev: %s\n", __func__, strerror(errno));
		return -errno;
	}

	if (sdrv->compatible) {
		str = buf;
		ret = snprintf(buf, sizeof(buf), "%s\n%s", sdrv->name, sdrv->compatible);
		if (ret < 0)
			goto err_close;
	} else {
		str = sdrv->name;
	}

	ret = write(fd, str, strlen(str) + 1);
	if (ret < 0) {
		printf("%s: Failed to write /dev/spidev: %s\n", __func__, strerror(errno));
		ret = -errno;
		goto err_close;
	}

	sdrv->fd = fd;

	return 0;

err_close:
	close(fd);

	return ret;
}

char *spi_driver_event_loop(struct spi_driver *sdrv)
{
	char new_device[32];
	struct pollfd pfd;
	char *device;
	pid_t pid;
	int ret;

	pfd.fd = sdrv->fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	while (!(pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
		ret = read(sdrv->fd, new_device, sizeof(new_device));
		if (ret < 0) {
			pr_err("%s: Failed to read from /dev/spidev: %s\n", __func__, strerror(errno));
			device = ERR_PTR(-errno);
			goto out;
		}

		printf("New device: %d  '%s'\n", strlen(new_device), new_device);

		pid = fork();
		if (!pid) {
			pr_info("%s: child pid=%d\n", __func__, getpid());
			device = strdup(new_device);
			if (!device)
				device = ERR_PTR(-ENOMEM);
			goto out;
		}

		poll(&pfd, 1, -1);
	}

out:

	return device;
}

struct spi_device *spi_alloc_device(const char *spidev_name)
{
	struct spi_device *spi;
	unsigned int busnum, cs;
	char *fname;
	int ret;

	spi = calloc(1, sizeof(*spi));
	fname = malloc(32);
	if (!spi || !fname) {
		ret = -ENOMEM;
		goto err_free;
	}

	ret = sscanf(spidev_name, "spidev%u.%u", &busnum, &cs);
	if (ret != 2) {
		ret = errno ? -errno : -EINVAL;
		goto err_free;
	}

	ret = snprintf(fname, 32, "/dev/%s", spidev_name);
	if (ret < 0 || ret > 31) {
		ret = -EINVAL;
		goto err_free;
	}

	spi->bus_num = busnum;
	spi->chip_select = cs;
	spi->fname = fname;
	dev_set_name(&spi->dev, "spi%u.%u", busnum, cs);
	dev_set_sysfs(&spi->dev, "/sys/bus/spi/devices/spi%u.%u", busnum, cs);
	ret = device_add(&spi->dev);
	if (ret)
		goto err_free;

	return spi;

err_free:
	free(fname);
	free(spi);

	return ERR_PTR(ret);
}

int spi_add_device(struct spi_device *spi)
{
	int fd;

	pr_debug("%s\n", __func__);
	fd = open(spi->fname, O_RDWR);
	if (fd < 0) {
		printf("%s: Failed to open '%s': %s\n", __func__, spi->fname, strerror(errno));
		return -errno;
	}

	spi->fd = fd;
	if (!spi->max_len)
		spi->max_len = 4096;
	if (!spi->max_dma_len)
		spi->max_dma_len = spi->max_len;

	if (!spi->bits_per_word_mask)
		spi->bits_per_word_mask = SPI_BPW_MASK(8);


	return 0;
}

void spi_unregister_device(struct spi_device *spi)
{
	pr_debug("%s\n", __func__);
	close(spi->fd);
}








static u32 spi_max = 0;

size_t spi_max_transfer_size(struct spi_device *spi, size_t max_len)
{
	size_t ret = spi->max_len;

	DRM_INFO("Max SPI transfer size: %zu\n", ret);
	if (max_len)
		ret = min(ret, max_len);
	if (spi_max)
		ret = min_t(size_t, ret, spi_max);
	ret &= ~0x3;
	if (ret < 4)
		ret = 4;

	DRM_DEBUG_DRIVER("Max SPI transfer size: %zu\n", ret);

	return ret;
}




static inline bool is_power_of_2(unsigned long n)
{
	return (n != 0 && ((n & (n - 1)) == 0));
}

static inline u16 __get_unaligned_le16(const u8 *p)
{
        return p[0] | p[1] << 8;
}

static inline u32 __get_unaligned_le32(const u8 *p)
{
        return p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
}

static inline u64 __get_unaligned_le64(const u8 *p)
{
        return (u64)__get_unaligned_le32(p + 4) << 32 |
               __get_unaligned_le32(p);
}

static inline u16 get_unaligned_le16(const void *p)
{
        return __get_unaligned_le16((const u8 *)p);
}

static inline u32 get_unaligned_le32(const void *p)
{
        return __get_unaligned_le32((const u8 *)p);
}

static inline u64 get_unaligned_le64(const void *p)
{
        return __get_unaligned_le64((const u8 *)p);
}

extern void __bad_unaligned_access_size(void);

#define __get_unaligned_le(ptr) ({                     \
        __builtin_choose_expr(sizeof(*(ptr)) == 1, *(ptr),                      \
        __builtin_choose_expr(sizeof(*(ptr)) == 2, get_unaligned_le16((ptr)),   \
        __builtin_choose_expr(sizeof(*(ptr)) == 4, get_unaligned_le32((ptr)),   \
        __builtin_choose_expr(sizeof(*(ptr)) == 8, get_unaligned_le64((ptr)),   \
        __bad_unaligned_access_size()))));                                      \
        })

#define get_unaligned  __get_unaligned_le


static const char hex_asc[] = "0123456789abcdef";
static const char hex_asc_upper[] = "0123456789ABCDEF";
#define hex_asc_lo(x)   hex_asc[((x) & 0x0f)]
#define hex_asc_hi(x)   hex_asc[((x) & 0xf0) >> 4]

static int hex_dump_to_buffer(const void *buf, size_t len, int rowsize, int groupsize,
		       char *linebuf, size_t linebuflen, bool ascii)
{
	const u8 *ptr = buf;
	int ngroups;
	u8 ch;
	int j, lx = 0;
	int ascii_column;
	int ret;

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	if (len > rowsize)		/* limit to one line at a time */
		len = rowsize;
	if (!is_power_of_2(groupsize) || groupsize > 8)
		groupsize = 1;
	if ((len % groupsize) != 0)	/* no mixed size output */
		groupsize = 1;

	ngroups = len / groupsize;
	ascii_column = rowsize * 2 + rowsize / groupsize + 1;

	if (!linebuflen)
		goto overflow1;

	if (!len)
		goto nil;

	if (groupsize == 8) {
		const u64 *ptr8 = buf;

		for (j = 0; j < ngroups; j++) {
			ret = snprintf(linebuf + lx, linebuflen - lx,
				       "%s%16.16llx", j ? " " : "",
				       get_unaligned(ptr8 + j));
			if (ret >= linebuflen - lx)
				goto overflow1;
			lx += ret;
		}
	} else if (groupsize == 4) {
		const u32 *ptr4 = buf;

		for (j = 0; j < ngroups; j++) {
			ret = snprintf(linebuf + lx, linebuflen - lx,
				       "%s%8.8x", j ? " " : "",
				       get_unaligned(ptr4 + j));
			if (ret >= linebuflen - lx)
				goto overflow1;
			lx += ret;
		}
	} else if (groupsize == 2) {
		const u16 *ptr2 = buf;

		for (j = 0; j < ngroups; j++) {
			ret = snprintf(linebuf + lx, linebuflen - lx,
				       "%s%4.4x", j ? " " : "",
				       get_unaligned(ptr2 + j));
			if (ret >= linebuflen - lx)
				goto overflow1;
			lx += ret;
		}
	} else {
		for (j = 0; j < len; j++) {
			if (linebuflen < lx + 2)
				goto overflow2;
			ch = ptr[j];
			linebuf[lx++] = hex_asc_hi(ch);
			if (linebuflen < lx + 2)
				goto overflow2;
			linebuf[lx++] = hex_asc_lo(ch);
			if (linebuflen < lx + 2)
				goto overflow2;
			linebuf[lx++] = ' ';
		}
		if (j)
			lx--;
	}
	if (!ascii)
		goto nil;

	while (lx < ascii_column) {
		if (linebuflen < lx + 2)
			goto overflow2;
		linebuf[lx++] = ' ';
	}
	for (j = 0; j < len; j++) {
		if (linebuflen < lx + 2)
			goto overflow2;
		ch = ptr[j];
		linebuf[lx++] = (isascii(ch) && isprint(ch)) ? ch : '.';
	}
nil:
	linebuf[lx] = '\0';
	return lx;
overflow2:
	linebuf[lx++] = '\0';
overflow1:
	return ascii ? ascii_column + len : (groupsize * 2 + 1) * ngroups - 1;
}

/* hexdump that can do u16, useful on little endian where bytes are swapped */
static void tinydrm_hexdump(char *linebuf, size_t linebuflen, const void *buf,
			    size_t len, size_t bpw, size_t max)
{
	if (bpw > 16) {
		snprintf(linebuf, linebuflen, "bpw not supported");
	} else if (bpw > 8) {
		size_t count = len > max ? max / 2 : (len / 2);
		const u16 *buf16 = buf;
		unsigned int j, lx = 0;
		int ret;

		for (j = 0; j < count; j++) {
			ret = snprintf(linebuf + lx, linebuflen - lx,
				       "%s%4.4x", j ? " " : "", *buf16++);
			if (ret >= linebuflen - lx) {
				snprintf(linebuf, linebuflen, "ERROR");
				break;
			}
			lx += ret;
		}
	} else {
		hex_dump_to_buffer(buf, len, max, 1, linebuf, linebuflen,
				   false);
	}
}

/* called through TINYDRM_DEBUG_REG_WRITE() */
void tinydrm_debug_reg_write(const void *reg, size_t reg_len, const void *val, size_t val_len, size_t val_width)
{
	unsigned int regnr;

	if (reg_len != 1 && reg_len != 2)
		return;

	regnr = (reg_len == 1) ? *(u8 *)reg : *(u16 *)reg;

	if (val && val_len && dma_buf_check((void *)val)) {
		struct dma_buf *dmabuf = (void *)val;

		drm_printk(KERN_DEBUG, DRM_UT_CORE, "regnr=0x%0*x, len=%zu, fd=%d\n",
			   reg_len == 1 ? 2 : 4, regnr, val_len, dmabuf->fd);
	} else if (val && val_len) {
		char linebuf[3 * 32];

		tinydrm_hexdump(linebuf, ARRAY_SIZE(linebuf), val, val_len, val_width, 16);
		drm_printk(KERN_DEBUG, DRM_UT_CORE, "regnr=0x%0*x, data(%zu)= %s%s\n",
			   reg_len == 1 ? 2 : 4, regnr, val_len, linebuf, val_len > 32 ? " ..." : "");
	} else {
		drm_printk(KERN_DEBUG, DRM_UT_CORE, "regnr=0x%0*x\n", reg_len == 1 ? 2 : 4, regnr);
	}
}

/* called through spi_message_dbg() */
void _spi_message_dbg(struct spi_device *spi, struct spi_ioc_transfer *msg, unsigned int num_msgs)
{
	struct spi_ioc_transfer *tmp;
	void *tx_buf, *rx_buf;
	char linebuf[3 * 32];
	int i = 0;

	for (i = 0; i < num_msgs; i++) {
		tmp = &msg[i];
		tx_buf = (void *)(unsigned long)tmp->tx_buf;
		rx_buf = (void *)(unsigned long)tmp->rx_buf;

		if (tx_buf) {
			bool dma = false;

			tinydrm_hexdump(linebuf, ARRAY_SIZE(linebuf), tx_buf, tmp->len, tmp->bits_per_word, 16);
			pr_debug("    tr[%i]: bpw=%i, dma=%u, len=%u, tx_buf(%p)=[%s%s]\n",
				 i, tmp->bits_per_word, dma, tmp->len, tx_buf, linebuf, tmp->len > 16 ? " ..." : "");
		} else if (tmp->tx_dma_fd) {
			pr_debug("    tr[%i]: bpw=%i, len=%u, fd=%d, offset=%u\n",
				 i, tmp->bits_per_word, tmp->len, tmp->tx_dma_fd, tmp->dma_offset);
		}
		if (tmp->rx_buf) {
			tinydrm_hexdump(linebuf, ARRAY_SIZE(linebuf), rx_buf, tmp->len, tmp->bits_per_word, 16);
			pr_debug("    tr[%i]: bpw=%i,        len=%u, rx_buf(%p)=[%s%s]\n",
				 i, tmp->bits_per_word, tmp->len, rx_buf, linebuf, tmp->len > 16 ? " ..." : "");
		}
		i++;
	}
}



/**
 * tinydrm_spi_transfer - SPI transfer helper
 * @spi: SPI device
 * @speed_hz: Override speed (optional)
 * @header: Optional header transfer
 * @bpw: Bits per word
 * @buf: Buffer to transfer
 * @len: Buffer length
 * @swap_buf: Swap buffer used on Little Endian when 16 bpw is not supported
 * @max_chunk: Break up buffer into chunks of this size
 *
 * This SPI transfer helper breaks up the transfer of @buf into @max_chunk
 * chunks. If the machine is Little Endian and the SPI master driver doesn't
 * support @bpw=16, it swaps the bytes using @swap_buf and does a 8-bit
 * transfer. If @header is set, it is prepended to each SPI message.
 *
 * Returns:
 * Zero on success, negative error code on failure.
 */
int spi_transfer(struct spi_device *spi, u32 speed_hz, struct spi_ioc_transfer *header, u8 bpw,
		 const void *buf, size_t len, u16 *swap_buf, size_t max_chunk)
{
	struct spi_ioc_transfer msg[2];
	struct spi_ioc_transfer *tr;
	unsigned int num_msgs = 1;
	struct dma_buf *dmabuf = NULL;
	bool swap = false;
	size_t chunk;
	int ret = 0;

	if (bpw != 8 && bpw != 16)
		return -EINVAL;

	if (!speed_hz)
		speed_hz = spi->max_speed_hz;

	if (udrm_debug & DRM_UT_CORE)
		pr_debug("[drm:%s] @%uMHz, bpw=%u, max_chunk=%zu, transfers:\n",
			 __func__, speed_hz / 1000000, bpw, max_chunk);

	memset(msg, 0, sizeof(msg));
	if (header) {
		msg[0] = *header;
		tr = &msg[1];
		num_msgs++;
	} else {
		tr = &msg[0];
	}

	tr->bits_per_word = bpw;
	tr->speed_hz = speed_hz;

	if (regmap_get_machine_endian() == REGMAP_ENDIAN_LITTLE &&
	    bpw == 16 && !spi_bpw_supported(spi, 16)) {
		if (!swap_buf)
			return -EINVAL;

		swap = true;
		tr->bits_per_word = 8;
	}

	if (dma_buf_check((void *)buf)) {
		dmabuf = (void *)buf;
		tr->tx_dma_fd = dmabuf->fd;
		max_chunk = spi->max_dma_len;
	}

	while (len) {
		chunk = min(len, max_chunk);

		if (!dmabuf)
			tr->tx_buf = (unsigned long)buf;
		tr->len = chunk;

		if (swap) {
			const u16 *buf16 = buf;
			unsigned int i;

			for (i = 0; i < chunk / 2; i++)
				swap_buf[i] = swab16(buf16[i]);

			tr->tx_buf = (unsigned long)swap_buf;
		}

		buf += chunk;
		len -= chunk;

		spi_message_dbg(spi, msg, num_msgs);

		ret = ioctl(spi->fd, SPI_IOC_MESSAGE(num_msgs), msg);
		if (ret < 0)
			return -errno;

		if (dmabuf)
			tr->dma_offset += chunk;
	};

	return 0;
}

int spi_sync(struct spi_device *spi, struct spi_ioc_transfer *msg, unsigned int num_msgs)
{
	int ret;

	ret = ioctl(spi->fd, SPI_IOC_MESSAGE(num_msgs), msg);
	if (ret < 0)
		return -errno;

	return 0;
}
