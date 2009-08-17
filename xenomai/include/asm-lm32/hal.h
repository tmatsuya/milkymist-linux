/**
 *   @ingroup hal
 *   @file
 *
 *   Real-Time Hardware Abstraction Layer for LM32.
 *
 *   Copyright &copy; 2002-2004 Philippe Gerum.
 *
 *   LM32 port
 *     Copyright (C) 2007 Theobroma Systems <mico32@theobroma-systems.com>
 *
 *   Xenomai is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation, Inc., 675 Mass Ave,
 *   Cambridge MA 02139, USA; either version 2 of the License, or (at
 *   your option) any later version.
 *
 *   Xenomai is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Xenomai; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *   02111-1307, USA.
 */

#ifndef _XENO_ASM_LM32_HAL_H
#define _XENO_ASM_LM32_HAL_H

#include <asm-generic/xenomai/hal.h>	/* Read the generic bits. */
#include <asm/byteorder.h>

typedef unsigned long long rthal_time_t;

static inline __attribute_const__ unsigned long ffnz(unsigned long ul)
{
	return ffs(ul) - 1;
}

#ifndef __cplusplus
#include <asm/system.h>
#include <asm/timex.h>
#include <asm/xenomai/atomic.h>
#include <asm/processor.h>
#include <asm/ipipe.h>
#include <asm/irq.h>

#define RTHAL_TIMER_IRQ   IRQ_SYSTMR
#define RTHAL_TIMER_IDX   0

#define rthal_grab_control()     do { } while(0)
#define rthal_release_control()  do { } while(0)

static inline unsigned long long rthal_rdtsc (void)
{
    unsigned long long t;
    rthal_read_tsc(t);
    return t;
}

static inline struct task_struct *rthal_current_host_task (int cpuid)
{
    return current;
}

void rthal_timer_program(int periodic, cycles_t cyc);

static inline void rthal_timer_program_shot (unsigned long delay)
{
    if(delay == 0)
        rthal_trigger_irq(RTHAL_TIMER_IRQ);
    else
        rthal_timer_program(0, delay);
}

/* Private interface -- Internal use only */

#ifdef CONFIG_XENO_OPT_TIMING_PERIODIC
extern int rthal_periodic_p;
#else /* !CONFIG_XENO_OPT_TIMING_PERIODIC */
#define rthal_periodic_p  0
#endif /* CONFIG_XENO_OPT_TIMING_PERIODIC */

/* TODO LM32 find out if we want task_struct, thread_struct or thread_info here */
asmlinkage void rthal_thread_switch(struct task_struct *prev,
				    struct thread_info *out,
				    struct thread_info *in);
asmlinkage void rthal_thread_trampoline(void);
asmlinkage int rthal_defer_switch_p(void);


static const char *const rthal_fault_labels[] = {
    [0] = "Reset",
    [1] = "Breakpoint",
    [2] = "Instruction Bus Error",
    [3] = "Watchpoint",
    [4] = "Data Bus Error",
    [5] = "Divide By Zero",
    [6] = NULL
};

#endif /* !__cplusplus */

#endif /* !_XENO_ASM_LM32_HAL_H */

// vim: ts=4 et sw=4 sts=4
