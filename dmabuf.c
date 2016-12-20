#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base.h"
#include "dmabuf.h"

#define DMABUF_MAGIC 0xB00FB00F

struct dma_buf *dma_buf_get(int fd)
{
	struct dma_buf *dmabuf;

	dmabuf = calloc(1, sizeof(*dmabuf));
	if (!dmabuf)
		return ERR_PTR(-ENOMEM);

	dmabuf->fd = fd;
	dmabuf->magic = DMABUF_MAGIC;

	return dmabuf;
}

void dma_buf_put(struct dma_buf *dmabuf)
{
	if (dmabuf->vaddr)
		dma_buf_vunmap(dmabuf, dmabuf->vaddr);
	dmabuf->magic = 0xDEADBEEF;
	free(dmabuf);
}

bool dma_buf_check(void *addr)
{
	struct dma_buf *dmabuf = addr;

	return dmabuf->magic == DMABUF_MAGIC ? true : false;
}

void *dma_buf_vmap(struct dma_buf *dmabuf)
{
	void *vaddr;

	vaddr = mmap(NULL, dmabuf->size, PROT_READ, MAP_SHARED, dmabuf->fd, 0);
	if (vaddr == MAP_FAILED) {
		pr_err("%s: Failed to mmap %d\n", -errno);
		return NULL;
	}

	dmabuf->vaddr = vaddr;

	return vaddr;
}

void dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	if (vaddr != dmabuf->vaddr) {
		pr_err("%s: Failed to munmap, pointer mismatch: %p != %p\n", vaddr, dmabuf->vaddr);
		return;
	}

	if (munmap(dmabuf->vaddr, dmabuf->size) < 0)
		pr_err("%s: Failed to munmap %d\n", -errno);

	dmabuf->vaddr = NULL;
}

static int dma_buf_cpu_access(struct dma_buf *dma_buf, unsigned int flags)
{
	struct dma_buf_sync sync_args = {
		.flags = flags,
	};
	int ret;

	ret = ioctl(dma_buf->fd, DMA_BUF_IOCTL_SYNC, &sync_args);
	if (ret < 0)
		return ret;

	return 0;
}

int dma_buf_begin_cpu_access(struct dma_buf *dma_buf)
{
	return dma_buf_cpu_access(dma_buf, DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE);
}

int dma_buf_end_cpu_access(struct dma_buf *dma_buf)
{
	return dma_buf_cpu_access(dma_buf, DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE);
}
