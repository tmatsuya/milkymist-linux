/* -*- linux-c -*-
 * include/linux/ipipe_base.h
 *
 * Copyright (C) 2002-2007 Philippe Gerum.
 *               2007 Jan Kiszka.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 * USA; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __LINUX_IPIPE_BASE_H
#define __LINUX_IPIPE_BASE_H

#ifdef CONFIG_IPIPE

#include <linux/bitops.h>
#include <asm/ipipe_base.h>

/* Number of virtual IRQs */
#define IPIPE_NR_VIRQS		BITS_PER_LONG
/* First virtual IRQ # */
#define IPIPE_VIRQ_BASE		(((IPIPE_NR_XIRQS + BITS_PER_LONG - 1) / BITS_PER_LONG) * BITS_PER_LONG)
/* Total number of IRQ slots */
#define IPIPE_NR_IRQS		(IPIPE_VIRQ_BASE + IPIPE_NR_VIRQS)
/* Number of indirect words needed to map the whole IRQ space. */
#define IPIPE_IRQ_IWORDS	((IPIPE_NR_IRQS + BITS_PER_LONG - 1) / BITS_PER_LONG)
#define IPIPE_IRQ_IMASK		(BITS_PER_LONG - 1)
#define IPIPE_IRQMASK_ANY	(~0L)
#define IPIPE_IRQMASK_VIRT	(IPIPE_IRQMASK_ANY << (IPIPE_VIRQ_BASE / BITS_PER_LONG))

/* Per-cpu pipeline status */
#define IPIPE_STALL_FLAG	0	/* Stalls a pipeline stage -- guaranteed at bit #0 */
#define IPIPE_SYNC_FLAG		1	/* The interrupt syncer is running for the domain */
#define IPIPE_NOSTACK_FLAG	2	/* Domain currently runs on a foreign stack */

#define IPIPE_STALL_MASK	(1L << IPIPE_STALL_FLAG)
#define IPIPE_SYNC_MASK		(1L << IPIPE_SYNC_FLAG)

extern struct ipipe_domain ipipe_root;

#define ipipe_root_domain (&ipipe_root)

void __ipipe_unstall_root(void);

void __ipipe_restore_root(unsigned long x);

#define ipipe_preempt_disable(flags)	local_irq_save_hw(flags)
#define ipipe_preempt_enable(flags)	local_irq_restore_hw(flags)
 
#ifdef CONFIG_IPIPE_DEBUG_CONTEXT
void ipipe_check_context(struct ipipe_domain *border_ipd);
#else /* !CONFIG_IPIPE_DEBUG_CONTEXT */
static inline void ipipe_check_context(struct ipipe_domain *border_ipd) { }
#endif /* !CONFIG_IPIPE_DEBUG_CONTEXT */

#else /* !CONFIG_IPIPE */
#define ipipe_preempt_disable(flags)	do { \
						preempt_disable(); \
						(void)(flags); \
					while (0)
#define ipipe_preempt_enable(flags)	preempt_enable()
#define ipipe_check_context(ipd)	do { } while(0)
#endif	/* CONFIG_IPIPE */

#endif	/* !__LINUX_IPIPE_BASE_H */
