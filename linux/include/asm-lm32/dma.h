/*
 * Based on:
 * include/asm-m68k/dma.h
 */

#ifndef _LM32_ASM_DMA_H
#define _LM32_ASM_DMA_H
 
#define MAX_DMA_CHANNELS 8

/* Don't define MAX_DMA_ADDRESS; it's useless on the m68k/coldfire and any
   occurrence should be flagged as an error.  */
#define MAX_DMA_ADDRESS PAGE_OFFSET

/* These are in kernel/dma.c: */
//extern int request_dma(unsigned int dmanr, const char *device_id);	/* reserve a DMA channel */
//extern void free_dma(unsigned int dmanr);	/* release it again */
 
#endif /* _LM32_ASM_DMA_H */
