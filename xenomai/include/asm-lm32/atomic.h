/*
 * Copyright (C) 2003,2004 Philippe Gerum <rpm@xenomai.org>.
 *
 * LM32 port
 *   Copyright (C) 2007 Theobroma Systems <mico32@theobroma-systems.com>
 *
 * Xenomai is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _XENO_ASM_LM32_ATOMIC_H
#define _XENO_ASM_LM32_ATOMIC_H

#ifdef __KERNEL__

#include <linux/bitops.h>
#include <asm/atomic.h>
#include <asm/system.h>
//#include <asm/xenomai/features.h>

#define xnarch_atomic_xchg(ptr,v)       xchg(ptr,v)
#define xnarch_memory_barrier()  	smp_mb()

//static inline void atomic_set_mask(unsigned long mask, unsigned long *addr)
//{
//    unsigned long flags;
//
//    local_irq_save_hw(flags);
//    *addr |= mask;
//    local_irq_restore_hw(flags);
//}

#define xnarch_atomic_set(pcounter,i)          atomic_set(pcounter,i)
#define xnarch_atomic_get(pcounter)            atomic_read(pcounter)
#define xnarch_atomic_inc(pcounter)            atomic_inc(pcounter)
#define xnarch_atomic_dec(pcounter)            atomic_dec(pcounter)
#define xnarch_atomic_inc_and_test(pcounter)   atomic_inc_and_test(pcounter)
#define xnarch_atomic_dec_and_test(pcounter)   atomic_dec_and_test(pcounter)
#define xnarch_atomic_set_mask(pflags,mask)    atomic_set_mask(mask,pflags)
#define xnarch_atomic_clear_mask(pflags,mask)  atomic_clear_mask(mask,pflags)

typedef atomic_t atomic_counter_t;

#else /* !__KERNEL__ */

//#include <asm/xenomai/features.h>
//#include <asm/xenomai/syscall.h>

//typedef struct { volatile int counter; } atomic_counter_t;

/* copy and paste from asm-lm32/system.h start */
static unsigned int xeno_local_irq_disable(void)
{
	unsigned int old_ie, new_ie;
	asm volatile (
		"mvi %0,0xfffffffe\n" \
		"rcsr %1, IE\n" \
		"and %0, %1, %0\n" \
		"wcsr IE, %0\n" \
		"andi %1, %1, 1\n" \
		: "=r"(new_ie), "=r"(old_ie) \
	);
	return old_ie;
}

static inline void xeno_local_irq_enable(void)
{
	unsigned int ie;
	asm volatile (
		"rcsr %0, IE\n" 
		"ori %0, %0, 1\n"
		"wcsr IE, %0\n"
		: "=r"(ie));
}

#define xeno_local_irq_save(x) { x = xeno_local_irq_disable_hw(); }

#define xeno_local_irq_restore(x) do {			\
		if (x)	\
			xeno_local_irq_enable_hw();		\
	} while (0)

static inline unsigned long __xchg(unsigned long x, volatile void *ptr, int size)
{
	unsigned long ret;
	unsigned long flags;

	switch (size) {
	case 1:
		xeno_local_irq_save(flags);
		ret = *(volatile unsigned char *)ptr;
		*(volatile unsigned char *)ptr = x;
		xeno_local_irq_restore(flags);
		break;

	case 2:
		xeno_local_irq_save(flags);
		ret = *(volatile unsigned short *)ptr;
		*(volatile unsigned short *)ptr = x;
		xeno_local_irq_restore(flags);
		break;

	case 4:
		xeno_local_irq_save(flags);
		ret = *(volatile unsigned long *)ptr;
		*(volatile unsigned long *)ptr = x;
		xeno_local_irq_restore(flags);
		break;

	}

	return ret;
}
/* copy and paste from asm-lm32/system.h end */

#define xnarch_atomic_xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))

#define xnarch_memory_barrier()                 __asm__ __volatile__("": : :"memory")

#define xnarch_atomic_inc(pcounter)             (void) atomic_add_return(1, pcounter)
#define xnarch_atomic_dec_and_test(pcounter)    (atomic_sub_return(1, pcounter) == 0)
#define xnarch_atomic_set_mask(pflags,mask)     atomic_set_mask(mask,pflags)
#define xnarch_atomic_clear_mask(pflags,mask)   atomic_clear_mask(mask,pflags)

#define cpu_relax()                             xnarch_memory_barrier()
#define xnarch_read_memory_barrier()		xnarch_memory_barrier()
#define xnarch_write_memory_barrier()		xnarch_memory_barrier()

#endif /* __KERNEL__ */

typedef unsigned long atomic_flags_t;

#endif /* !_XENO_ASM_LM32_ATOMIC_H */

// vim: ts=4 et sw=4 sts=4
