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

#ifndef _XENO_ASM_LM32_SYSTEM_H
#define _XENO_ASM_LM32_SYSTEM_H

#ifdef __KERNEL__

#include <linux/ptrace.h>
#include <asm-generic/xenomai/system.h>
#include <asm/xenomai/syscall.h>
//#include <asm/processor.h>

#define XNARCH_THREAD_STACKSZ   4096

#define xnarch_stack_size(tcb)  ((tcb)->stacksize)
#define xnarch_user_task(tcb)   ((tcb)->user_task)
#define xnarch_user_pid(tcb)    ((tcb)->user_task->pid)

struct xnthread;
struct task_struct;

typedef struct xnarchtcb {  /* Per-thread arch-dependent block */
    /* Kernel mode side */
    unsigned stacksize;         /* Aligned size of stack (bytes) */
    unsigned long *stackbase;   /* Stack space */
    int isroot;

    /* User mode side */
    struct task_struct *user_task;      /* Shadowed user-space task */
    struct task_struct *active_task;    /* Active user-space task */
    struct thread_struct ts;	        /* Holds kernel-based thread context. */
    struct thread_struct* tsp;	        /* pointer to active thread struct (&ts or user) */

} xnarchtcb_t;

#define xnarch_fpu_ptr(tcb)     NULL /* No FPU handling at all. */

typedef struct xnarch_fltinfo {

    unsigned exception;
    struct pt_regs *regs;

} xnarch_fltinfo_t;

#define xnarch_fault_trap(fi)   ((fi)->exception)
#define xnarch_fault_code(fi)   (0)
#define xnarch_fault_pc(fi)     ((fi)->regs->ea)
#define xnarch_fault_fpu_p(fi)  (0)
/* The following predicates are only usable over a regular Linux stack
   context. */
#define xnarch_fault_pf_p(fi)   (0)
#define xnarch_fault_bp_p(fi)   ((current->ptrace & PT_PTRACED) && \
                                ((fi)->exception == IPIPE_TRAP_BREAK))

#define xnarch_fault_notify(fi) (!xnarch_fault_bp_p(fi))

#ifdef __cplusplus
extern "C" {
#endif

static inline void *xnarch_alloc_host_mem (u_long bytes)
{
    return kmalloc(bytes,GFP_KERNEL);
}

static inline void xnarch_free_host_mem (void *chunk, u_long bytes)
{
	kfree(chunk);
}

#ifdef __cplusplus
}
#endif

#else /* !__KERNEL__ */

#include <nucleus/system.h>
#include <bits/local_lim.h>

#endif /* __KERNEL__ */

#endif /* !_XENO_ASM_LM32_SYSTEM_H */

// vim: ts=4 et sw=4 sts=4
