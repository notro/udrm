#ifndef __DMABUF_H
#define __DMABUF_H

#include <linux/dma-buf.h>
#include <sys/types.h>

struct dma_buf {
	size_t size;
	int fd;
	void *vaddr;
	u32 magic;
};

struct dma_buf *dma_buf_get(int fd);
void dma_buf_put(struct dma_buf *dmabuf);

bool dma_buf_check(void *addr);

void *dma_buf_vmap(struct dma_buf *dmabuf);
void dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr);

int dma_buf_begin_cpu_access(struct dma_buf *dma_buf);
int dma_buf_end_cpu_access(struct dma_buf *dma_buf);

#endif
