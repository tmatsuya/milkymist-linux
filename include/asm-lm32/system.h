/*
 * (C) Copyright 2007
 *     Theobroma Systems <www.theobroma-systems.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef _LM32_ASM_SYSTEM_H
#define _LM32_ASM_SYSTEM_H

#include <linux/linkage.h>

#ifndef __ASSEMBLY__

#ifdef CONFIG_IPIPE

#include <linux/ipipe_trace.h>

static __inline__ unsigned long __ipipe_test_and_stall_root(void);

void __ipipe_unstall_root(void);

void __ipipe_restore_root(unsigned long flags);

#define __all_masked_irq_flags 0x1
#define irqs_enabled_from_flags_hw(x)	((x) & __all_masked_irq_flags)
#define raw_irqs_disabled_flags(flags)	(!irqs_enabled_from_flags_hw(flags))
#define local_test_iflag_hw(x)		irqs_enabled_from_flags_hw(x)

#define local_save_flags(x)						\
	do {								\
		(x) = __ipipe_test_root(); \
	} while(0)

#define local_irq_save(x)				\
	do {						\
		(x) = __ipipe_test_and_stall_root();	\
	} while(0)

#define local_irq_restore(x) \
	__ipipe_restore_root(x)

#define local_irq_disable()	\
	do { \
		ipipe_check_context(ipipe_root_domain); \
		__ipipe_stall_root(); \
	} while(0)

#define local_irq_enable() \
	__ipipe_unstall_root()

#define irqs_disabled() \
	__ipipe_test_root()

#define local_save_flags_hw(x) \
	asm volatile ("rcsr %0, IE\n" : "=r"(x))

#define	irqs_disabled_hw()				\
	({						\
		unsigned long flags;			\
		local_save_flags_hw(flags);		\
		!irqs_enabled_from_flags_hw(flags);	\
	})

static inline unsigned long raw_mangle_irq_bits(int virt, unsigned long real)
{
	/* Merge virtual and real interrupt mask bits into a single
	   32bit word. */
	return (real & ~(1 << 31)) | ((virt != 0) << 31);
}

static inline int raw_demangle_irq_bits(unsigned long *x)
{
	int virt = (*x & (1 << 31)) != 0;
	*x &= ~(1L << 31);
	return virt;
}

/* TODO LM32 IPIPE_TRACE_IRQSOFF */

static inline void local_irq_enable_hw(void)
{
	unsigned int ie;
	asm volatile (
		"rcsr %0, IE\n" 
		"ori %0, %0, 1\n"
		"wcsr IE, %0\n"
		: "=r"(ie));
}

static unsigned int local_irq_disable_hw(void)
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

#define local_irq_save_hw(x) { x = local_irq_disable_hw(); }

#define local_irq_restore_hw(x) do {			\
		if (irqs_enabled_from_flags_hw(x))	\
			local_irq_enable_hw();		\
	} while (0)

#define local_irq_disable_hw_notrace()	local_irq_disable_hw()
#define local_irq_enable_hw_notrace()	local_irq_enable_hw()
#define local_irq_save_hw_notrace(x)	local_irq_save_hw(x)
#define local_irq_restore_hw_notrace(x)	local_irq_restore_hw(x)

#else /* !CONFIG_IPIPE */

static inline int local_irq_disable(void)
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

static inline void local_irq_enable(void)
{
	unsigned int ie;
	asm volatile (
		"rcsr %0, IE\n" 
		"ori %0, %0, 1\n"
		"wcsr IE, %0\n"
		: "=r"(ie));
}

#define local_save_flags(x) asm volatile ("rcsr %0, IE\n" : "=r"(x))

#define local_irq_save(x) { x = local_irq_disable(); }

#define local_irq_restore(x) { unsigned int ie; \
	asm volatile ( \
		"rcsr %0, IE\n" \
		"or %0, %0, %1\n" \
		"wcsr IE, %0\n": \
		 "=&r"(ie): "r"(x) ); }

static inline int irqs_disabled(void)
{
	unsigned long flags;
	local_save_flags(flags);
	return ((flags & 0x1) == 0);
}

#define local_irq_save_hw(x)		local_irq_save(x)
#define local_irq_restore_hw(x)		local_irq_restore(x)
#define local_irq_enable_hw()		local_irq_enable()
#define local_irq_disable_hw()		local_irq_disable()
#define irqs_disabled_hw()		irqs_disabled()

#endif /* !CONFIG_IPIPE */

extern asmlinkage struct task_struct* resume(struct task_struct* last, struct task_struct* next);

#define switch_to(prev,next,last)				\
do {								\
	local_irq_disable_hw_cond(); \
  lm32_current_thread = task_thread_info(next);  	\
  last = resume(prev, next);	\
	local_irq_enable_hw_cond(); \
} while (0)

#define nop()  asm volatile ("nop"::)
#define mb()   asm volatile (""   : : :"memory")
#define rmb()  asm volatile (""   : : :"memory")
#define wmb()  asm volatile (""   : : :"memory")
#define set_rmb(var, value)    do { xchg(&var, value); } while (0)
#define set_mb(var, value)     set_rmb(var, value)

#ifdef CONFIG_SMP
#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#define smp_read_barrier_depends()	read_barrier_depends()
#else
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#define smp_read_barrier_depends()	do { } while(0)
#endif

#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))

static inline unsigned long __xchg(unsigned long x, volatile void *ptr, int size)
{
	unsigned long ret;
	unsigned long flags;

	switch (size) {
	case 1:
		local_irq_save_hw(flags);
		ret = *(volatile unsigned char *)ptr;
		*(volatile unsigned char *)ptr = x;
		local_irq_restore_hw(flags);
		break;

	case 2:
		local_irq_save_hw(flags);
		ret = *(volatile unsigned short *)ptr;
		*(volatile unsigned short *)ptr = x;
		local_irq_restore_hw(flags);
		break;

	case 4:
		local_irq_save_hw(flags);
		ret = *(volatile unsigned long *)ptr;
		*(volatile unsigned long *)ptr = x;
		local_irq_restore_hw(flags);
		break;

	}

	return ret;
}


/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */

static inline unsigned long __cmpxchg_u32(volatile int *p, unsigned long old,
					  unsigned long nnew)
{
	 unsigned long flags;
	 int prev;

	 local_irq_save(flags);
	 if ((prev = *p) == old)
					 *p = nnew;
	 local_irq_restore(flags);
	 return(prev);
}

static inline unsigned long long __cmpxchg_u64(volatile long long *p, unsigned long long old,
					  unsigned long long nnew)
{
	 unsigned long flags;
	 int prev;

	 local_irq_save(flags);
	 if ((prev = *p) == old)
					 *p = nnew;
	 local_irq_restore(flags);
	 return(prev);
}

/* This function doesn't exist, so you'll get a linker error
   if something tries to do an invalid cmpxchg().  */
extern void __cmpxchg_called_with_bad_pointer(void);

#define __HAVE_ARCH_CMPXCHG 1

static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
				      unsigned long new, int size)
{
	switch (size) {
	case 4:
		return __cmpxchg_u32(ptr, old, new);
	case 8:
		return __cmpxchg_u64(ptr, old, new);
	}

	__cmpxchg_called_with_bad_pointer();
	return old;
}

#define cmpxchg(ptr, old, new)					\
	((typeof(*(ptr)))__cmpxchg((ptr), (unsigned long)(old),	\
				   (unsigned long)(new),	\
				   sizeof(*(ptr))))

//TODO: implement reset
//#define HARD_RESET_NOW() {}

#define arch_align_stack(x) (x)

#endif /* !__ASSEMBLY__ */

#endif 
