/*
 * Based on:
 * include/asm-m68knommu/io.h
 *
 * lm32 does not currently support ISA/PCI
 */

#ifndef _LM32_ASM_IO_H
#define _LM32_ASM_IO_H

#ifdef __KERNEL__

#ifndef __ASSEMBLY__

/*
 * These are for ISA/PCI shared memory _only_ and should never be used
 * on any other type of memory, including Zorro memory. They are meant to
 * access the bus in the bus byte order which is little-endian!.
 *
 * readX/writeX() are used to access memory mapped devices. On some
 * architectures the memory mapped IO stuff needs to be accessed
 * differently. On the m68k architecture, we just read/write the
 * memory location directly.
 */
/* ++roman: The assignments to temp. vars avoid that gcc sometimes generates
 * two accesses to memory, which may be undesireable for some devices.
 */

/*
 * swap functions are sometimes needed to interface little-endian hardware
 */
static inline unsigned short _swapw(volatile unsigned short v)
{
    return ((v << 8) | (v >> 8));
}

static inline unsigned int _swapl(volatile unsigned long v)
{
    return ((v << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000) >> 8) | (v >> 24));
}

#define readb(addr) \
    ({ unsigned char __v = (*(volatile unsigned char *) (addr)); __v; })
#define readw(addr) \
    ({ unsigned short __v = (*(volatile unsigned short *) (addr)); __v; })
#define readl(addr) \
    ({ unsigned int __v = (*(volatile unsigned int *) (addr)); __v; })

#define readb_relaxed(addr) readb(addr)
#define readw_relaxed(addr) readw(addr)
#define readl_relaxed(addr) readl(addr)

#define writeb(b,addr) (void)((*(volatile unsigned char *) (addr)) = (b))
#define writew(b,addr) (void)((*(volatile unsigned short *) (addr)) = (b))
#define writel(b,addr) (void)((*(volatile unsigned int *) (addr)) = (b))

#define __raw_readb readb
#define __raw_readw readw
#define __raw_readl readl
#define __raw_writeb writeb
#define __raw_writew writew
#define __raw_writel writel

static inline void outsb(unsigned int addr, const void *buf, int len)
{
	volatile unsigned char *ap = (volatile unsigned char *) addr;
	const unsigned char *bp = (const unsigned char *) buf;
	while (len--)
		*ap = *bp++;
}

static inline void outsw(unsigned int addr, const void *buf, int len)
{
	volatile unsigned short *ap = (volatile unsigned short *) addr;
	const unsigned short *bp = (const unsigned short *) buf;
	while (len--)
		*ap = _swapw(*bp++);
}

static inline void outsl(unsigned int addr, const void *buf, int len)
{
	volatile unsigned int *ap = (volatile unsigned int *) addr;
	const unsigned int *bp = (const unsigned int *) buf;
	while (len--)
		*ap = _swapl(*bp++);
}

static inline void insb(unsigned int addr, void *buf, int len)
{
	volatile unsigned char *ap = (volatile unsigned char *) addr;
	unsigned char *bp = (unsigned char *) buf;
	while (len--)
		*bp++ = *ap;
}

static inline void insw(unsigned int addr, void *buf, int len)
{
	volatile unsigned short *ap = (volatile unsigned short *) addr;
	unsigned short *bp = (unsigned short *) buf;
	while (len--)
		*bp++ = _swapw(*ap);
}

static inline void insl(unsigned int addr, void *buf, int len)
{
	volatile unsigned int *ap = (volatile unsigned int *) addr;
	unsigned int *bp = (unsigned int *) buf;
	while (len--)
		*bp++ = _swapl(*ap);
}

#define mmiowb()

/*
 *	make the short names macros so specific devices
 *	can override them as required
 */

#define memset_io(a,b,c)	memset((void *)(a),(b),(c))
#define memcpy_fromio(a,b,c)	memcpy((a),(void *)(b),(c))
#define memcpy_toio(a,b,c)	memcpy((void *)(a),(b),(c))

#define inb(addr)    readb(addr)
#define inw(addr)    readw(addr)
#define inl(addr)    readl(addr)
#define outb(x,addr) ((void) writeb(x,addr))
#define outw(x,addr) ((void) writew(x,addr))
#define outl(x,addr) ((void) writel(x,addr))

#define inb_p(addr)    inb(addr)
#define inw_p(addr)    inw(addr)
#define inl_p(addr)    inl(addr)
#define outb_p(x,addr) outb(x,addr)
#define outw_p(x,addr) outw(x,addr)
#define outl_p(x,addr) outl(x,addr)

#define DEF_MMIO_IN_BE(name, size, insn)                                \
static inline u##size name(const volatile u##size __iomem *addr)        \
{                                                                       \
        u##size ret;                                                    \
        __asm__ __volatile__(#insn " %0,%1"\
                : "=r" (ret) : "m" (*addr) : "memory");                 \
        return ret;                                                     \
}

#define DEF_MMIO_OUT_BE(name, size, insn)                               \
static inline void name(volatile u##size __iomem *addr, u##size val)    \
{                                                                       \
        __asm__ __volatile__(#insn " %0,%1"                 \
                : "=m" (*addr) : "r" (val) : "memory");                 \
}

#if 0
//#define in_8(addr)     readb(addr)
//#define in_be16(addr)  readw(addr)
//#define out_8(b, addr) ((void) writeb(b,addr))
//#define out_be16(b, addr) ((void)writew(b,addr))
#else
DEF_MMIO_IN_BE(in_8,     8, lb);
DEF_MMIO_IN_BE(in_be16, 16, lh);
DEF_MMIO_OUT_BE(out_8,     8, sb);
DEF_MMIO_OUT_BE(out_be16, 16, sh);
#endif

#define in_le16(addr)  __le16_to_cpu(readw(addr))
#define out_le16(b, addr) ((void)writew(__cpu_to_le16(b),addr))



#define IO_SPACE_LIMIT 0xffff


/* Values for nocacheflag and cmode */
#define IOMAP_FULL_CACHING		0
#define IOMAP_NOCACHE_SER		1
#define IOMAP_NOCACHE_NONSER		2
#define IOMAP_WRITETHROUGH		3

extern void *__ioremap(unsigned long physaddr, unsigned long size, int cacheflag);
extern void __iounmap(void *addr, unsigned long size);

static inline void *ioremap(unsigned long physaddr, unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_NOCACHE_SER);
}
static inline void *ioremap_nocache(unsigned long physaddr, unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_NOCACHE_SER);
}
static inline void *ioremap_writethrough(unsigned long physaddr, unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_WRITETHROUGH);
}
static inline void *ioremap_fullcache(unsigned long physaddr, unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_FULL_CACHING);
}

extern void iounmap(void *addr);

/* Nothing to do */

#define dma_cache_inv(_start,_size)		do { } while (0)
#define dma_cache_wback(_start,_size)		do { } while (0)
#define dma_cache_wback_inv(_start,_size)	do { } while (0)

/* Pages to physical address... */
#define page_to_phys(page)      ((page - mem_map) << PAGE_SHIFT)
#define page_to_bus(page)       ((page - mem_map) << PAGE_SHIFT)

/*
 * Macros used for converting between virtual and physical mappings.
 */
#define mm_ptov(vaddr)		((void *) (vaddr))
#define mm_vtop(vaddr)		((unsigned long) (vaddr))
#define phys_to_virt(addr)	((void *)__phys_to_virt (addr))
#define virt_to_phys(addr)	((unsigned long)__virt_to_phys (addr))

#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt

/*
 * Convert a physical pointer to a virtual kernel pointer for /dev/mem
 * access
 */
#define xlate_dev_mem_ptr(p)	__va(p)

/*
 * Convert a virtual cached pointer to an uncached pointer
 */
#define xlate_dev_kmem_ptr(p)	p

#endif /* __ASSEMBLY__ */

#endif /* __KERNEL__ */

#endif /* _LM32_ASM_IO_H */
