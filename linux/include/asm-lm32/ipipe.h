/*
 *   Copyright (C) 2002-2007 Philippe Gerum.
 *
 *   Based on include/asm-blackfin/ipipe.h
 *
 *   (C) Copyright 2007
 *     Theobroma Systems <www.theobroma-systems.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __ASM_BLACKFIN_IPIPE_H
#define __ASM_BLACKFIN_IPIPE_H

#ifdef CONFIG_IPIPE

#include <linux/cpumask.h>
#include <linux/list.h>
#include <linux/threads.h>
#include <linux/irq.h>
#include <linux/timex.h>
#include <linux/ipipe_percpu.h>
#include <asm/ptrace.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/atomic.h>
#include <asm/traps.h>

#define IPIPE_ARCH_STRING     "1.7-00"
#define IPIPE_MAJOR_NUMBER    1
#define IPIPE_MINOR_NUMBER    7
#define IPIPE_PATCH_NUMBER    0

#ifdef CONFIG_SMP
#error "I-pipe/lm32: SMP not implemented"
#else /* !CONFIG_SMP */
#define ipipe_processor_id()	0
#endif	/* CONFIG_SMP */

#define prepare_arch_switch(next)		\
do {						\
	ipipe_schedule_notify(current, next);	\
	local_irq_disable_hw();			\
} while(0)

#define task_hijacked(p)						\
	({								\
		int __x__ = ipipe_current_domain != ipipe_root_domain;	\
	  __clear_bit(IPIPE_SYNC_FLAG, &ipipe_root_cpudom_var(status)); \
		local_irq_enable_hw(); __x__;				\
	})

struct ipipe_domain;

struct ipipe_sysinfo {

	int ncpus;		/* Number of CPUs on board */
	u64 cpufreq;		/* CPU frequency (in Hz) */

	/* Arch-dependent block */

	struct {
		unsigned tmirq;	/* Timer tick IRQ */
		u64 tmfreq;	/* Timer frequency */
	} archdep;
};

#define ipipe_read_tsc(t) do { t = get_cycles(); } while(0)

#define ipipe_cpu_freq()	__ipipe_core_clock
#define ipipe_tsc2ns(_t)	(((unsigned long)(_t)) * __ipipe_freq_scale)
#define ipipe_tsc2us(_t)	(ipipe_tsc2ns(_t) / 1000 + 1)

/* Private interface -- Internal use only */

#define __ipipe_check_platform()	do { } while(0)

#define __ipipe_init_platform()		do { } while(0)

extern atomic_t __ipipe_irq_lvdepth[NR_IRQLVL];

extern int __ipipe_mach_timerstolen;
extern unsigned long __ipipe_irq_lvmask;

extern struct ipipe_domain ipipe_root;

/* enable/disable_irqdesc _must_ be used in pairs. */

void __ipipe_enable_irqdesc(struct ipipe_domain *ipd,
			    unsigned irq);

void __ipipe_disable_irqdesc(struct ipipe_domain *ipd,
			     unsigned irq);

#define __ipipe_enable_irq(irq)		irq_desc[irq].chip->unmask(irq)

#define __ipipe_disable_irq(irq)	irq_desc[irq].chip->mask(irq)

#define __ipipe_lock_root()					\
	set_bit(IPIPE_ROOTLOCK_FLAG, &ipipe_root_domain->flags)

#define __ipipe_unlock_root()					\
	clear_bit(IPIPE_ROOTLOCK_FLAG, &ipipe_root_domain->flags)

void __ipipe_enable_pipeline(void);

#define __ipipe_hook_critical_ipi(ipd) do { } while(0)

#define __ipipe_sync_pipeline(syncmask)					\
	do {								\
		struct ipipe_domain *ipd = ipipe_current_domain;	\
		if (likely(ipd != ipipe_root_domain || !test_bit(IPIPE_ROOTLOCK_FLAG, &ipd->flags))) \
			__ipipe_sync_stage(syncmask);			\
	} while(0)

int __ipipe_handle_irq(unsigned irq, struct pt_regs *regs);

int __ipipe_get_irq_priority(unsigned irq);

int __ipipe_get_irqthread_priority(unsigned irq);

void __ipipe_stall_root_raw(void);

void __ipipe_unstall_root_raw(void);

void __ipipe_serial_debug(const char *fmt, ...);

DECLARE_PER_CPU(struct pt_regs, __ipipe_tick_regs);

extern unsigned long __ipipe_core_clock;

extern unsigned long __ipipe_freq_scale;

extern unsigned long __ipipe_irq_tail_hook;

unsigned long get_cclk(void);	/* Core clock freq (HZ) */

unsigned long get_sclk(void);	/* System clock freq (HZ) */

static inline unsigned long __ipipe_ffnz(unsigned long ul)
{
	return ffs(ul) - 1;
}

#if 0
#define __ipipe_run_irqtail()  /* Must be a macro */ \
	do { \
		if( current_thread_info()->flags & _TIF_NEED_RESCHED ) { \
			local_irq_enable_hw(); \
			/* schedule -> enables interrupts */ \
			schedule(); \
		} \
	} while(0)
#else
#define __ipipe_run_irqtail()  /* Must be a macro */ do{ } while(0)
#endif

#if 0
	do {								\
		asmlinkage void __ipipe_call_irqtail(void);		\
		unsigned long __pending;				\
		asm volatile("rcsr %0, IP": "=r"(__pending)); \
		if (__pending) {				\
			__ipipe_call_irqtail();			\
			/*__pending &= ~0x8010; */				\
			/*if (__pending && (__pending & (__pending - 1)) == 0) */ \
				/* __ipipe_call_irqtail(); */			\
		}							\
	} while(0)
#endif

#define __ipipe_run_isr(ipd, irq)					\
	do {								\
		local_irq_enable_nohead(ipd);		\
		if (ipd == ipipe_root_domain) {				\
			if(likely(!ipipe_virtual_irq_p(irq))) {			\
				ipd->irqs[irq].handler(irq, &__raw_get_cpu_var(__ipipe_tick_regs)); \
			} else { \
				irq_enter(); \
				ipd->irqs[irq].handler(irq, ipd->irqs[irq].cookie); \
				irq_exit(); \
			} \
		} else {						\
			__clear_bit(IPIPE_SYNC_FLAG, &ipipe_cpudom_var(ipd, status)); \
			ipd->irqs[irq].handler(irq, ipd->irqs[irq].cookie); \
			__set_bit(IPIPE_SYNC_FLAG, &ipipe_cpudom_var(ipd, status)); \
		}							\
		local_irq_disable_nohead(ipd);		\
	} while(0)

#define __ipipe_syscall_watched_p(p, sc)	\
	(((p)->flags & PF_EVNOTIFY) || (unsigned long)sc >= NR_syscalls)

//#if defined(CONFIG_BF533)
//#define IRQ_SYSTMR		IRQ_TMR0
//#define IRQ_PRIOTMR		CONFIG_TIMER0
//#define PRIO_GPIODEMUX(irq)	CONFIG_PFA
//#elif defined(CONFIG_BF537)
//#define IRQ_SYSTMR		IRQ_TMR0
//#define IRQ_PRIOTMR		CONFIG_IRQ_TMR0
//#define PRIO_GPIODEMUX(irq)	CONFIG_IRQ_PROG_INTA
//#elif defined(CONFIG_BF561)
//#define IRQ_SYSTMR		IRQ_TIMER0
//#define IRQ_PRIOTMR		CONFIG_IRQ_TIMER0	
//#define PRIO_GPIODEMUX(irq)	((irq) == IRQ_PROG0_INTA ? CONFIG_IRQ_PROG0_INTA :  (irq) == IRQ_PROG1_INTA ? CONFIG_IRQ_PROG1_INTA :  CONFIG_IRQ_PROG2_INTA)
//#define bfin_write_TIMER_DISABLE(val)	bfin_write_TMRS8_DISABLE(val)
//#define bfin_write_TIMER_ENABLE(val)	bfin_write_TMRS8_ENABLE(val)
//#define bfin_write_TIMER_STATUS(val)	bfin_write_TMRS8_STATUS(val)
//#endif

#define IRQ_SYSTMR			IRQ_LINUX_TIMER
#define IRQ_MASK_SYSTMR	(1 << IRQ_SYSTMR)

#else /* !CONFIG_IPIPE */

#define task_hijacked(p)		0
#define ipipe_trap_notify(t,r)  	0

#define __ipipe_stall_root_raw()	do { } while(0)
#define __ipipe_unstall_root_raw()	do { } while(0)

#define ipipe_init_irq_threads()		do { } while(0)
#define ipipe_start_irq_thread(irq, desc)	0

#define IRQ_SYSTMR		IRQ_LINUX_TIMER

#endif /* !CONFIG_IPIPE */

#endif	/* !__ASM_BLACKFIN_IPIPE_H */
