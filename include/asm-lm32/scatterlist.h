/*
 * Based on
 * include/asm-m68knommu/scatterlist.h
 */

#ifndef _LM32_ASM_SCATTERLIST_H
#define _LM32_ASM_SCATTERLIST_H

#include <linux/mm.h>
#include <asm/types.h>

struct scatterlist {
	struct page	*page;
	unsigned int	offset;
	dma_addr_t	dma_address;
	unsigned int	length;
};

#define sg_address(sg)		(page_address((sg)->page) + (sg)->offset)
#define sg_dma_address(sg)      ((sg)->dma_address)
#define sg_dma_len(sg)          ((sg)->length)

#define ISA_DMA_THRESHOLD	(0xffffffff)

#endif /* _LM32_ASM_SCATTERLIST_H */
