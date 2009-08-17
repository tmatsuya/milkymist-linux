/* -*- linux-c -*-
 * include/linux/ipipe_trace.h
 *
 * Copyright (C) 2005 Luotao Fu.
 *               2005-2007 Jan Kiszka.
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

#ifndef _LINUX_IPIPE_TRACE_H
#define _LINUX_IPIPE_TRACE_H

#ifdef CONFIG_IPIPE_TRACE

#include <linux/types.h>

void ipipe_trace_begin(unsigned long v);
void ipipe_trace_end(unsigned long v);
void ipipe_trace_freeze(unsigned long v);
void ipipe_trace_special(unsigned char special_id, unsigned long v);
void ipipe_trace_pid(pid_t pid, short prio);
int ipipe_trace_max_reset(void);
int ipipe_trace_frozen_reset(void);

#else /* !CONFIG_IPIPE_TRACE */

#define ipipe_trace_begin(v)		do { (void)(v); } while(0)
#define ipipe_trace_end(v)		do { (void)(v); } while(0)
#define ipipe_trace_freeze(v)		do { (void)(v); } while(0)
#define ipipe_trace_special(id, v)	do { (void)(id); (void)(v); } while(0)
#define ipipe_trace_pid(pid, prio)	do { (void)(pid); (void)(prio); } while(0)
#define ipipe_trace_max_reset()		do { } while(0)
#define ipipe_trace_froze_reset()	do { } while(0)

#endif /* !CONFIG_IPIPE_TRACE */

#ifdef CONFIG_IPIPE_TRACE_PANIC
void ipipe_trace_panic_freeze(void);
void ipipe_trace_panic_dump(void);
#else
static inline void ipipe_trace_panic_freeze(void) { }
static inline void ipipe_trace_panic_dump(void) { }
#endif

#ifdef CONFIG_IPIPE_TRACE_IRQSOFF
#define ipipe_trace_irq_entry(irq)	ipipe_trace_begin(irq)
#define ipipe_trace_irq_exit(irq)	ipipe_trace_end(irq)
#define ipipe_trace_irqsoff()		ipipe_trace_begin(0x80000000UL)
#define ipipe_trace_irqson()		ipipe_trace_end(0x80000000UL)
#else
#define ipipe_trace_irq_entry(irq)	do { (void)(irq);} while(0)
#define ipipe_trace_irq_exit(irq)	do { (void)(irq);} while(0)
#define ipipe_trace_irqsoff()		do { } while(0)
#define ipipe_trace_irqson()		do { } while(0)
#endif

#endif	/* !__LINUX_IPIPE_TRACE_H */
