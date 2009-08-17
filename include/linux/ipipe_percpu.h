/*   -*- linux-c -*-
 *   include/linux/ipipe_percpu.h
 *
 *   Copyright (C) 2007 Philippe Gerum.
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

#ifndef __LINUX_IPIPE_PERCPU_H
#define __LINUX_IPIPE_PERCPU_H

#include <asm/percpu.h>
#include <asm/ptrace.h>

struct ipipe_domain;

struct ipipe_percpu_domain_data {
	unsigned long status;	/* <= Must be first in struct. */
	unsigned long irqpend_himask;
	unsigned long irqpend_lomask[IPIPE_IRQ_IWORDS];
	unsigned long irqheld_mask[IPIPE_IRQ_IWORDS];
	unsigned long irqall[IPIPE_NR_IRQS];
	u64 evsync;
};

#ifdef CONFIG_SMP
#define ipipe_percpudom(ipd, var, cpu)	\
	(per_cpu(ipipe_percpu_darray, cpu)[(ipd)->slot].var)
#define ipipe_cpudom_var(ipd, var)	\
	(__raw_get_cpu_var(ipipe_percpu_darray)[(ipd)->slot].var)
#else
DECLARE_PER_CPU(struct ipipe_percpu_domain_data *, ipipe_percpu_daddr[CONFIG_IPIPE_DOMAINS]);
#define ipipe_percpudom(ipd, var, cpu)	\
	(per_cpu(ipipe_percpu_daddr, cpu)[(ipd)->slot]->var)
#define ipipe_cpudom_var(ipd, var)	\
	(__raw_get_cpu_var(ipipe_percpu_daddr)[(ipd)->slot]->var)
#endif

DECLARE_PER_CPU(struct ipipe_percpu_domain_data, ipipe_percpu_darray[CONFIG_IPIPE_DOMAINS]);

DECLARE_PER_CPU(struct ipipe_domain *, ipipe_percpu_domain);

#ifdef CONFIG_IPIPE_DEBUG_CONTEXT
DECLARE_PER_CPU(int, ipipe_percpu_context_check);
#endif

#define ipipe_percpu(var, cpu)		per_cpu(var, cpu)
#define ipipe_cpu_var(var)		__raw_get_cpu_var(var)

#define ipipe_root_cpudom_var(var)	\
	__raw_get_cpu_var(ipipe_percpu_darray)[0].var

#define ipipe_this_cpudom_var(var)	\
	ipipe_cpudom_var(ipipe_current_domain, var)

#endif	/* !__LINUX_IPIPE_PERCPU_H */
