/*
 * Copyright (C) 2001,2002,2003,2004 Philippe Gerum <rpm@xenomai.org>.
 *
 * LM32 port
 *   Copyright (C) 2007 Theobroma Systems <mico32@theobroma-systems.com>
 *   
 * Xenomai is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef _XENO_ASM_LM32_BITS_POD_H
#define _XENO_ASM_LM32_BITS_POD_H

unsigned xnarch_tsc_scale;
unsigned xnarch_tsc_shift;

long long xnarch_tsc_to_ns(long long ts)
{
	return xnarch_llmulshft(ts, xnarch_tsc_scale, xnarch_tsc_shift);
}
#define XNARCH_TSC_TO_NS

#include <asm-generic/xenomai/bits/pod.h>

void xnpod_welcome_thread(struct xnthread *, int);

void xnpod_delete_thread(struct xnthread *);

#define xnarch_start_timer(tick_handler, cpu) \
	(rthal_timer_request(tick_handler, cpu)?:(1000000000UL/HZ))

#define xnarch_stop_timer(cpu)	rthal_timer_release(cpu)

static inline void xnarch_leave_root(xnarchtcb_t * rootcb)
{
	/* Remember the preempted Linux task pointer. */
	rootcb->user_task = rootcb->active_task = current;
	rootcb->tsp = &current->thread;
}

static inline void xnarch_enter_root(xnarchtcb_t * rootcb)
{
	/* empty */
}

/* use kernel task switch */
extern asmlinkage struct task_struct* resume(struct task_struct* last, struct task_struct* next);

static inline void xnarch_switch_to(xnarchtcb_t * out_tcb, xnarchtcb_t * in_tcb)
{
	struct task_struct *prev = out_tcb->active_task;
	struct task_struct *next = in_tcb->user_task;
	struct task_struct* prevtask;
	struct task_struct* nexttask;

#if 0
	volatile unsigned long* ledbase = (volatile unsigned long*)lm32tag_leds[0]->addr; 
	*ledbase = ~0;
		*ledbase &= ~1;
		*ledbase &= ~2;
		*ledbase &= ~4;
#endif

	if (likely(next != NULL)) {
		in_tcb->active_task = next;
		rthal_clear_foreign_stack(&rthal_domain);
	} else {
		in_tcb->active_task = prev;
		rthal_set_foreign_stack(&rthal_domain);
	}

	/* context switch. */

	/* fake that the input to resume is a task_struct* where *tsp is an element */
	prevtask = (struct task_struct*)
		((unsigned long)out_tcb->tsp - offsetof(struct task_struct, thread.ksp));

	/* fake that the input to resume is a task_struct* where *tsp is an element */
	nexttask = (struct task_struct*)
		((unsigned long)in_tcb->tsp - offsetof(struct task_struct, thread.ksp));

	if( prevtask != nexttask )
		resume(prevtask, nexttask);
}


static inline void xnarch_finalize_and_switch(xnarchtcb_t * dead_tcb,
					      xnarchtcb_t * next_tcb)
{
	xnarch_switch_to(dead_tcb, next_tcb);
}

static inline void xnarch_finalize_no_switch(xnarchtcb_t * dead_tcb)
{
	/* Empty */
}

static inline void xnarch_init_root_tcb(xnarchtcb_t * tcb,
					struct xnthread *thread,
					const char *name)
{
	tcb->user_task = current;
	tcb->active_task = NULL;
	tcb->isroot = 1;
	tcb->tsp = &tcb->ts;
}

asmlinkage static void xnarch_thread_trampoline(unsigned long __unused,
    struct xnthread *self,
    int imask,
    void *cookie,
    void (*entry)(void *cookie))
{
	volatile unsigned long* ledbase = (volatile unsigned long*)lm32tag_leds[0]->addr; 

	xnpod_welcome_thread(self, imask);
	entry(cookie);
	xnpod_delete_thread(self);
}

static inline void xnarch_init_thread(xnarchtcb_t * tcb,
				      void (*entry) (void *),
				      void *cookie,
				      int imask,
				      struct xnthread *thread, char *name)
{
	/* prepare child task switch frame */
	struct pt_regs* regs;

	/* fill stack with zero */
	memset(tcb->stackbase, 0, tcb->stacksize);

	regs = ((struct pt_regs*)(tcb->stackbase + tcb->stacksize)) - 1;

	/* fill registers with ones */
	memset(regs, 0, sizeof(*regs));

	regs->r2 = thread;
	regs->r3 = imask;
	regs->r4 = cookie;
	regs->r5 = entry;
	regs->ra = (unsigned long)&xnarch_thread_trampoline;
	regs->sp = (unsigned long)tcb->stackbase + tcb->stacksize - 4;

	tcb->ts.ksp = ((unsigned long)regs) - 4;
	tcb->ts.which_stack = 0;
	tcb->ts.usp = 0;
}

#define xnarch_fpu_init_p(task) (1)

static inline void xnarch_enable_fpu(xnarchtcb_t * current_tcb)
{
}

static inline void xnarch_init_fpu(xnarchtcb_t * tcb)
{
}

static inline void xnarch_save_fpu(xnarchtcb_t * tcb)
{
}

static inline void xnarch_restore_fpu(xnarchtcb_t * tcb)
{
}

static inline int xnarch_escalate(void)
{
	extern int xnarch_escalation_virq;

	if (rthal_current_domain == rthal_root_domain) {
		rthal_trigger_irq(xnarch_escalation_virq);
		return 1;
	}

	return 0;
}

#endif /* !_XENO_ASM_LM32_BITS_POD_H */
