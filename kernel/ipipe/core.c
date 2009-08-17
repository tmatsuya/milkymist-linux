/* -*- linux-c -*-
 * linux/kernel/ipipe/core.c
 *
 * Copyright (C) 2002-2005 Philippe Gerum.
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
 *
 * Architecture-independent I-PIPE core support.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/tick.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif	/* CONFIG_PROC_FS */
#include <linux/ipipe_trace.h>
#include <linux/ipipe_tickdev.h>

static int __ipipe_ptd_key_count;

static unsigned long __ipipe_ptd_key_map;

static unsigned long __ipipe_domain_slot_map;

struct ipipe_domain ipipe_root;

#ifndef CONFIG_SMP
/*
 * Create an alias to the unique root status, so that arch-dep code
 * may get simple and easy access to this percpu variable.  We also
 * create an array of pointers to the percpu domain data; this tends
 * to produce a better code when reaching non-root domains. We make
 * sure that the early boot code would be able to dereference the
 * pointer to the root domain data safely by statically initializing
 * its value (local_irq*() routines depend on this).
 */
#if __GNUC__ >= 4
extern unsigned long __ipipe_root_status
__attribute__((alias(__stringify(__raw_get_cpu_var(ipipe_percpu_darray)))));
EXPORT_SYMBOL(__ipipe_root_status);
#else /* __GNUC__ < 4 */
/*
 * Work around a GCC 3.x issue making alias symbols unusable as
 * constant initializers.
 */
unsigned long *const __ipipe_root_status_addr = &__raw_get_cpu_var(ipipe_percpu_darray);
EXPORT_SYMBOL(__ipipe_root_status_addr);
#endif /* __GNUC__ < 4 */

DEFINE_PER_CPU(struct ipipe_percpu_domain_data *, ipipe_percpu_daddr[CONFIG_IPIPE_DOMAINS]) =
{ [0] = (struct ipipe_percpu_domain_data *)&__raw_get_cpu_var(ipipe_percpu_darray) };
EXPORT_PER_CPU_SYMBOL(ipipe_percpu_daddr);
#endif /* !CONFIG_SMP */

DEFINE_PER_CPU(struct ipipe_percpu_domain_data, ipipe_percpu_darray[CONFIG_IPIPE_DOMAINS]) =
{ [0] = { .status = IPIPE_STALL_MASK } }; /* Root domain stalled on each CPU at startup. */

DEFINE_PER_CPU(struct ipipe_domain *, ipipe_percpu_domain) = { &ipipe_root };

static IPIPE_DEFINE_SPINLOCK(__ipipe_pipelock);

LIST_HEAD(__ipipe_pipeline);

unsigned long __ipipe_virtual_irq_map;

#ifdef CONFIG_PRINTK
unsigned __ipipe_printk_virq;
#endif /* CONFIG_PRINTK */

int __ipipe_event_monitors[IPIPE_NR_EVENTS];

#ifdef CONFIG_GENERIC_CLOCKEVENTS

DECLARE_PER_CPU(struct tick_device, tick_cpu_device);

static DEFINE_PER_CPU(struct ipipe_tick_device, ipipe_tick_cpu_device);

static void __ipipe_set_tick_mode(enum clock_event_mode mode,
				  struct clock_event_device *cdev)
{
	struct ipipe_tick_device *itd;
	itd = &per_cpu(ipipe_tick_cpu_device, smp_processor_id());
	itd->emul_set_mode(mode, itd);
}

static int __ipipe_set_next_tick(unsigned long evt,
				 struct clock_event_device *cdev)
{
	uint64_t delta_ns = (uint64_t)cdev->delta;
	struct ipipe_tick_device *itd;

	if (delta_ns > ULONG_MAX)
		delta_ns = ULONG_MAX;

	itd = &per_cpu(ipipe_tick_cpu_device, smp_processor_id());
	return itd->emul_set_tick((unsigned long)delta_ns, itd);
}

int ipipe_request_tickdev(const char *devname,
			  void (*emumode)(enum clock_event_mode mode,
					  struct ipipe_tick_device *tdev),
			  int (*emutick)(unsigned long delta,
					 struct ipipe_tick_device *tdev),
			  int cpu)
{
	struct ipipe_tick_device *itd;
	struct tick_device *slave;
	unsigned long flags;
	int status;

	flags = ipipe_critical_enter(NULL);

	itd = &per_cpu(ipipe_tick_cpu_device, cpu);

	if (itd->slave != NULL) {
		status = -EBUSY;
		goto out;
	}

	slave = &per_cpu(tick_cpu_device, cpu);

	if (strcmp(slave->evtdev->name, devname)) {
		/*
		 * No conflict so far with the current tick device,
		 * check whether the requested device is sane and has
		 * been blessed by the kernel.
		 */
		status = __ipipe_check_tickdev(devname) ?
			CLOCK_EVT_MODE_UNUSED : CLOCK_EVT_MODE_SHUTDOWN;
		goto out;
	}

	/*
	 * Our caller asks for using the same clock event device for
	 * ticking than we do, let's create a tick emulation device to
	 * interpose on the set_next_event() method, so that we may
	 * both manage the device in oneshot mode. Only the tick
	 * emulation code will actually program the clockchip hardware
	 * for the next shot, though.
	 *
	 * CAUTION: we still have to grab the tick device even when it
	 * current runs in periodic mode, since the kernel may switch
	 * to oneshot dynamically (highres/no_hz tick mode).
	 */

	itd->slave = slave;
	itd->emul_set_mode = emumode;
	itd->emul_set_tick = emutick;
	itd->real_set_mode = slave->evtdev->set_mode;
	itd->real_set_tick = slave->evtdev->set_next_event;
	slave->evtdev->set_mode = __ipipe_set_tick_mode;
	slave->evtdev->set_next_event = __ipipe_set_next_tick;
	status = slave->evtdev->mode;
out:
	ipipe_critical_exit(flags);

	return status;
}

void ipipe_release_tickdev(int cpu)
{
	struct ipipe_tick_device *itd;
	struct tick_device *slave;
	unsigned long flags;

	flags = ipipe_critical_enter(NULL);

	itd = &per_cpu(ipipe_tick_cpu_device, cpu);

	if (itd->slave != NULL) {
		slave = &per_cpu(tick_cpu_device, cpu);
		slave->evtdev->set_mode = itd->real_set_mode;
		slave->evtdev->set_next_event = itd->real_set_tick;
		itd->slave = NULL;
	}

	ipipe_critical_exit(flags);
}

#endif /* CONFIG_GENERIC_CLOCKEVENTS */

/*
 * ipipe_init() -- Initialization routine of the IPIPE layer. Called
 * by the host kernel early during the boot procedure.
 */
void __init ipipe_init(void)
{
	struct ipipe_domain *ipd = &ipipe_root;

	__ipipe_check_platform();	/* Do platform dependent checks first. */

	/*
	 * A lightweight registration code for the root domain. We are
	 * running on the boot CPU, hw interrupts are off, and
	 * secondary CPUs are still lost in space.
	 */

	/* Reserve percpu data slot #0 for the root domain. */
	ipd->slot = 0;
	set_bit(0, &__ipipe_domain_slot_map);

	ipd->name = "Linux";
	ipd->domid = IPIPE_ROOT_ID;
	ipd->priority = IPIPE_ROOT_PRIO;

	__ipipe_init_stage(ipd);

	INIT_LIST_HEAD(&ipd->p_link);
	list_add_tail(&ipd->p_link, &__ipipe_pipeline);

	__ipipe_init_platform();

#ifdef CONFIG_PRINTK
	__ipipe_printk_virq = ipipe_alloc_virq();	/* Cannot fail here. */
	ipd->irqs[__ipipe_printk_virq].handler = &__ipipe_flush_printk;
	ipd->irqs[__ipipe_printk_virq].cookie = NULL;
	ipd->irqs[__ipipe_printk_virq].acknowledge = NULL;
	ipd->irqs[__ipipe_printk_virq].control = IPIPE_HANDLE_MASK;
#endif /* CONFIG_PRINTK */

	__ipipe_enable_pipeline();

	printk(KERN_INFO "I-pipe %s: pipeline enabled.\n",
	       IPIPE_VERSION_STRING);
}

void __ipipe_init_stage(struct ipipe_domain *ipd)
{
	int cpu, n;

	for_each_online_cpu(cpu) {

		ipipe_percpudom(ipd, irqpend_himask, cpu) = 0;

		for (n = 0; n < IPIPE_IRQ_IWORDS; n++) {
			ipipe_percpudom(ipd, irqpend_lomask, cpu)[n] = 0;
			ipipe_percpudom(ipd, irqheld_mask, cpu)[n] = 0;
		}

		for (n = 0; n < IPIPE_NR_IRQS; n++)
			ipipe_percpudom(ipd, irqall, cpu)[n] = 0;

		ipipe_percpudom(ipd, evsync, cpu) = 0;
	}

	for (n = 0; n < IPIPE_NR_IRQS; n++) {
		ipd->irqs[n].acknowledge = NULL;
		ipd->irqs[n].handler = NULL;
		ipd->irqs[n].control = IPIPE_PASS_MASK;	/* Pass but don't handle */
	}

	for (n = 0; n < IPIPE_NR_EVENTS; n++)
		ipd->evhand[n] = NULL;

	ipd->evself = 0LL;
	mutex_init(&ipd->mutex);

	__ipipe_hook_critical_ipi(ipd);
}

void __ipipe_cleanup_domain(struct ipipe_domain *ipd)
{
	ipipe_unstall_pipeline_from(ipd);

#ifdef CONFIG_SMP
	{
		int cpu;

		for_each_online_cpu(cpu) {
			while (ipipe_percpudom(ipd, irqpend_himask, cpu) != 0)
				cpu_relax();
		}
	}
#else
	__raw_get_cpu_var(ipipe_percpu_daddr)[ipd->slot] = NULL;
#endif

	clear_bit(ipd->slot, &__ipipe_domain_slot_map);
}

void __ipipe_unstall_root(void)
{
	BUG_ON(!ipipe_root_domain_p);

        local_irq_disable_hw();

        __clear_bit(IPIPE_STALL_FLAG, &ipipe_root_cpudom_var(status));

        if (unlikely(ipipe_root_cpudom_var(irqpend_himask) != 0))
                __ipipe_sync_pipeline(IPIPE_IRQMASK_ANY);

        local_irq_enable_hw();
}

void __ipipe_restore_root(unsigned long x)
{
	BUG_ON(!ipipe_root_domain_p);

	if (x)
		__ipipe_stall_root();
	else
		__ipipe_unstall_root();
}

void fastcall ipipe_stall_pipeline_from(struct ipipe_domain *ipd)
{
	set_bit_safe(IPIPE_STALL_FLAG, &ipipe_cpudom_var(ipd, status));

	if (__ipipe_pipeline_head_p(ipd))
		local_irq_disable_hw();
}

unsigned long fastcall ipipe_test_and_stall_pipeline_from(struct ipipe_domain *ipd)
{
	unsigned long x;

	x = test_and_set_bit_safe(IPIPE_STALL_FLAG, &ipipe_cpudom_var(ipd, status));

	if (__ipipe_pipeline_head_p(ipd))
		local_irq_disable_hw();

	return x;
}

/*
 * ipipe_unstall_pipeline_from() -- Unstall the pipeline and
 * synchronize pending interrupts for a given domain. See
 * __ipipe_walk_pipeline() for more information.
 */
void fastcall ipipe_unstall_pipeline_from(struct ipipe_domain *ipd)
{
	struct list_head *pos;
	unsigned long flags;

	local_irq_save_hw(flags);

	__clear_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(ipd, status));

	if (ipd == ipipe_current_domain)
		pos = &ipd->p_link;
	else
		pos = __ipipe_pipeline.next;

	__ipipe_walk_pipeline(pos);

	if (likely(__ipipe_pipeline_head_p(ipd)))
		local_irq_enable_hw();
	else
		local_irq_restore_hw(flags);
}

unsigned long fastcall ipipe_test_and_unstall_pipeline_from(struct ipipe_domain *ipd)
{
	unsigned long x;

	x = test_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(ipd, status));
	ipipe_unstall_pipeline_from(ipd);

	return x;
}

void fastcall ipipe_restore_pipeline_from(struct ipipe_domain *ipd,
					  unsigned long x)
{
	if (x)
		ipipe_stall_pipeline_from(ipd);
	else
		ipipe_unstall_pipeline_from(ipd);
}

void ipipe_unstall_pipeline_head(void)
{
	struct ipipe_domain *head_domain;

	local_irq_disable_hw();

	head_domain = __ipipe_pipeline_head();
	__clear_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(head_domain, status));

	if (unlikely(ipipe_cpudom_var(head_domain, irqpend_himask) != 0)) {
		if (likely(head_domain == ipipe_current_domain))
			__ipipe_sync_pipeline(IPIPE_IRQMASK_ANY);
		else
			__ipipe_walk_pipeline(&head_domain->p_link);
        }

	local_irq_enable_hw();
}

void fastcall __ipipe_restore_pipeline_head(struct ipipe_domain *head_domain, unsigned long x)
{
	local_irq_disable_hw();

	if (x) {
#ifdef CONFIG_DEBUG_KERNEL
		static int warned;
		if (!warned && test_and_set_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(head_domain, status))) {
			/*
			 * Already stalled albeit ipipe_restore_pipeline_head()
			 * should have detected it? Send a warning once.
			 */
			warned = 1;
			printk(KERN_WARNING
				   "I-pipe: ipipe_restore_pipeline_head() optimization failed.\n");
			dump_stack();
		}
#else /* !CONFIG_DEBUG_KERNEL */
		set_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(head_domain, status));
#endif /* CONFIG_DEBUG_KERNEL */
	}
	else {
		__clear_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(head_domain, status));
		if (unlikely(ipipe_cpudom_var(head_domain, irqpend_himask) != 0)) {
			if (likely(head_domain == ipipe_current_domain))
				__ipipe_sync_pipeline(IPIPE_IRQMASK_ANY);
			else
				__ipipe_walk_pipeline(&head_domain->p_link);
		}
		local_irq_enable_hw();
	}
}

void fastcall __ipipe_spin_lock_irq(raw_spinlock_t *lock)
{
	local_irq_disable_hw();
	__raw_spin_lock(lock);
	__set_bit(IPIPE_STALL_FLAG, &ipipe_this_cpudom_var(status));
}

void fastcall __ipipe_spin_unlock_irq(raw_spinlock_t *lock)
{
	__raw_spin_unlock(lock);
	__clear_bit(IPIPE_STALL_FLAG, &ipipe_this_cpudom_var(status));
	local_irq_enable_hw();
}

unsigned long fastcall __ipipe_spin_lock_irqsave(raw_spinlock_t *lock)
{
	unsigned long flags;
	int s;

	local_irq_save_hw(flags);
	__raw_spin_lock(lock);
	s = __test_and_set_bit(IPIPE_STALL_FLAG, &ipipe_this_cpudom_var(status));

	return raw_mangle_irq_bits(s, flags);
}

void fastcall __ipipe_spin_unlock_irqrestore(raw_spinlock_t *lock, unsigned long x)
{
	__raw_spin_unlock(lock);
	if (!raw_demangle_irq_bits(&x))
		__clear_bit(IPIPE_STALL_FLAG, &ipipe_this_cpudom_var(status));
	local_irq_restore_hw(x);
}

void fastcall __ipipe_spin_unlock_irqbegin(ipipe_spinlock_t *lock)
{
	__raw_spin_unlock(&lock->__raw_lock);
}

void fastcall __ipipe_spin_unlock_irqcomplete(unsigned long x)
{
	if (!raw_demangle_irq_bits(&x))
		__clear_bit(IPIPE_STALL_FLAG, &ipipe_this_cpudom_var(status));
	local_irq_restore_hw(x);
}

/* Must be called hw IRQs off. */
void fastcall __ipipe_set_irq_pending(struct ipipe_domain *ipd, unsigned irq)
{
	int level = irq >> IPIPE_IRQ_ISHIFT, rank = irq & IPIPE_IRQ_IMASK;

	if (likely(!test_bit(IPIPE_LOCK_FLAG, &ipd->irqs[irq].control))) {
		__set_bit(rank, &ipipe_cpudom_var(ipd, irqpend_lomask)[level]);
		__set_bit(level,&ipipe_cpudom_var(ipd, irqpend_himask));
	} else
		__set_bit(rank, &ipipe_cpudom_var(ipd, irqheld_mask)[level]);

	ipipe_cpudom_var(ipd, irqall)[irq]++;
}

/* Must be called hw IRQs off. */
void fastcall __ipipe_lock_irq(struct ipipe_domain *ipd, int cpu, unsigned irq)
{
	if (likely(!test_and_set_bit(IPIPE_LOCK_FLAG, &ipd->irqs[irq].control))) {
		int level = irq >> IPIPE_IRQ_ISHIFT, rank = irq & IPIPE_IRQ_IMASK;
		if (__test_and_clear_bit(rank, &ipipe_percpudom(ipd, irqpend_lomask, cpu)[level]))
			__set_bit(rank, &ipipe_cpudom_var(ipd, irqheld_mask)[level]);
		if (ipipe_percpudom(ipd, irqpend_lomask, cpu)[level] == 0)
			__clear_bit(level, &ipipe_percpudom(ipd, irqpend_himask, cpu));
	}
}

/* Must be called hw IRQs off. */
void fastcall __ipipe_unlock_irq(struct ipipe_domain *ipd, unsigned irq)
{
	int cpu;

	if (likely(test_and_clear_bit(IPIPE_LOCK_FLAG, &ipd->irqs[irq].control))) {
		int level = irq >> IPIPE_IRQ_ISHIFT, rank = irq & IPIPE_IRQ_IMASK;
		for_each_online_cpu(cpu) {
			if (test_and_clear_bit(rank, &ipipe_percpudom(ipd, irqheld_mask, cpu)[level])) {
				/* We need atomic ops here: */
				set_bit(rank, &ipipe_percpudom(ipd, irqpend_lomask, cpu)[level]);
				set_bit(level, &ipipe_percpudom(ipd, irqpend_himask, cpu));
			}
		}
	}
}

/* __ipipe_walk_pipeline(): Plays interrupts pending in the log. Must
   be called with local hw interrupts disabled. */

void fastcall __ipipe_walk_pipeline(struct list_head *pos)
{
	struct ipipe_domain *this_domain = ipipe_current_domain, *next_domain;

	while (pos != &__ipipe_pipeline) {

		next_domain = list_entry(pos, struct ipipe_domain, p_link);

		if (test_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(next_domain, status)))
			break;	/* Stalled stage -- do not go further. */

		if (ipipe_cpudom_var(next_domain, irqpend_himask) != 0) {

			if (next_domain == this_domain)
				__ipipe_sync_pipeline(IPIPE_IRQMASK_ANY);
			else {

				ipipe_cpudom_var(this_domain, evsync) = 0;
				ipipe_current_domain = next_domain;
				ipipe_suspend_domain();	/* Sync stage and propagate interrupts. */

				if (ipipe_current_domain == next_domain)
					ipipe_current_domain = this_domain;
				/*
				 * Otherwise, something changed the current domain under our
				 * feet recycling the register set; do not override the new
				 * domain.
				 */

				if (ipipe_cpudom_var(this_domain, irqpend_himask) != 0 &&
				    !test_bit(IPIPE_STALL_FLAG,
					      &ipipe_cpudom_var(this_domain, status)))
					__ipipe_sync_pipeline(IPIPE_IRQMASK_ANY);
			}

			break;
		} else if (next_domain == this_domain)
			break;

		pos = next_domain->p_link.next;
	}
}

/*
 * ipipe_suspend_domain() -- Suspend the current domain, switching to
 * the next one which has pending work down the pipeline.
 */
void ipipe_suspend_domain(void)
{
	struct ipipe_domain *this_domain, *next_domain;
	struct list_head *ln;
	unsigned long flags;

	local_irq_save_hw(flags);

	this_domain = next_domain = ipipe_current_domain;

	__clear_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(this_domain, status));

	if (ipipe_cpudom_var(this_domain, irqpend_himask) != 0)
		goto sync_stage;

	for (;;) {
		ln = next_domain->p_link.next;

		if (ln == &__ipipe_pipeline)
			break;

		next_domain = list_entry(ln, struct ipipe_domain, p_link);

		if (test_bit(IPIPE_STALL_FLAG,
			     &ipipe_cpudom_var(next_domain, status)) != 0)
			break;

		if (ipipe_cpudom_var(next_domain, irqpend_himask) == 0)
			continue;

		ipipe_current_domain = next_domain;

sync_stage:
		__ipipe_sync_pipeline(IPIPE_IRQMASK_ANY);

		if (ipipe_current_domain != next_domain)
			/*
			 * Something has changed the current domain under our
			 * feet, recycling the register set; take note.
			 */
			this_domain = ipipe_current_domain;
	}

	ipipe_current_domain = this_domain;

	local_irq_restore_hw(flags);
}

/* ipipe_alloc_virq() -- Allocate a pipelined virtual/soft interrupt.
 * Virtual interrupts are handled in exactly the same way than their
 * hw-generated counterparts wrt pipelining.
 */
unsigned ipipe_alloc_virq(void)
{
	unsigned long flags, irq = 0;
	int ipos;

	spin_lock_irqsave(&__ipipe_pipelock, flags);

	if (__ipipe_virtual_irq_map != ~0) {
		ipos = ffz(__ipipe_virtual_irq_map);
		set_bit(ipos, &__ipipe_virtual_irq_map);
		irq = ipos + IPIPE_VIRQ_BASE;
	}

	spin_unlock_irqrestore(&__ipipe_pipelock, flags);

	return irq;
}

/* ipipe_virtualize_irq() -- Attach a handler (and optionally a hw
   acknowledge routine) to an interrupt for a given domain. */

int ipipe_virtualize_irq(struct ipipe_domain *ipd,
			 unsigned irq,
			 ipipe_irq_handler_t handler,
			 void *cookie,
			 ipipe_irq_ackfn_t acknowledge,
			 unsigned modemask)
{
	unsigned long flags;
	int err;

	if (irq >= IPIPE_NR_IRQS)
		return -EINVAL;

	if (ipd->irqs[irq].control & IPIPE_SYSTEM_MASK)
		return -EPERM;

	if (!test_bit(IPIPE_AHEAD_FLAG, &ipd->flags))
		/* Silently unwire interrupts for non-heading domains. */
		modemask &= ~IPIPE_WIRED_MASK;

	spin_lock_irqsave(&__ipipe_pipelock, flags);

	if (handler != NULL) {
		if (handler == IPIPE_SAME_HANDLER) {
			handler = ipd->irqs[irq].handler;
			cookie = ipd->irqs[irq].cookie;

			if (handler == NULL) {
				err = -EINVAL;
				goto unlock_and_exit;
			}
		} else if ((modemask & IPIPE_EXCLUSIVE_MASK) != 0 &&
			   ipd->irqs[irq].handler != NULL) {
			err = -EBUSY;
			goto unlock_and_exit;
		}

		/* Wired interrupts can only be delivered to domains
		 * always heading the pipeline, and using dynamic
		 * propagation. */

		if ((modemask & IPIPE_WIRED_MASK) != 0) {
			if ((modemask & (IPIPE_PASS_MASK | IPIPE_STICKY_MASK)) != 0) {
				err = -EINVAL;
				goto unlock_and_exit;
			}
			modemask |= (IPIPE_HANDLE_MASK);
		}

		if ((modemask & IPIPE_STICKY_MASK) != 0)
			modemask |= IPIPE_HANDLE_MASK;
	} else
		modemask &=
		    ~(IPIPE_HANDLE_MASK | IPIPE_STICKY_MASK |
		      IPIPE_EXCLUSIVE_MASK | IPIPE_WIRED_MASK);

	if (acknowledge == NULL && !ipipe_virtual_irq_p(irq))
		/* Acknowledge handler unspecified for a hw interrupt:
		   use the Linux-defined handler instead. */
		acknowledge = ipipe_root_domain->irqs[irq].acknowledge;

	ipd->irqs[irq].handler = handler;
	ipd->irqs[irq].cookie = cookie;
	ipd->irqs[irq].acknowledge = acknowledge;
	ipd->irqs[irq].control = modemask;

	if (irq < NR_IRQS && handler != NULL && !ipipe_virtual_irq_p(irq)) {
		__ipipe_enable_irqdesc(ipd, irq);

		if ((modemask & IPIPE_ENABLE_MASK) != 0) {
			if (ipd != ipipe_current_domain) {
				/* IRQ enable/disable state is domain-sensitive, so we may
				   not change it for another domain. What is allowed
				   however is forcing some domain to handle an interrupt
				   source, by passing the proper 'ipd' descriptor which
				   thus may be different from ipipe_current_domain. */
				err = -EPERM;
				goto unlock_and_exit;
			}
			__ipipe_enable_irq(irq);
		}
	}

	err = 0;

      unlock_and_exit:

	spin_unlock_irqrestore(&__ipipe_pipelock, flags);

	return err;
}

/* ipipe_control_irq() -- Change modes of a pipelined interrupt for
 * the current domain. */

int ipipe_control_irq(unsigned irq, unsigned clrmask, unsigned setmask)
{
	struct ipipe_domain *ipd;
	unsigned long flags;

	if (irq >= IPIPE_NR_IRQS)
		return -EINVAL;

	ipd = ipipe_current_domain;

	if (ipd->irqs[irq].control & IPIPE_SYSTEM_MASK)
		return -EPERM;

	if (ipd->irqs[irq].handler == NULL)
		setmask &= ~(IPIPE_HANDLE_MASK | IPIPE_STICKY_MASK);

	if ((setmask & IPIPE_STICKY_MASK) != 0)
		setmask |= IPIPE_HANDLE_MASK;

	if ((clrmask & (IPIPE_HANDLE_MASK | IPIPE_STICKY_MASK)) != 0)	/* If one goes, both go. */
		clrmask |= (IPIPE_HANDLE_MASK | IPIPE_STICKY_MASK);

	spin_lock_irqsave(&__ipipe_pipelock, flags);

	ipd->irqs[irq].control &= ~clrmask;
	ipd->irqs[irq].control |= setmask;

	if ((setmask & IPIPE_ENABLE_MASK) != 0)
		__ipipe_enable_irq(irq);
	else if ((clrmask & IPIPE_ENABLE_MASK) != 0)
		__ipipe_disable_irq(irq);

	spin_unlock_irqrestore(&__ipipe_pipelock, flags);

	return 0;
}

/* __ipipe_dispatch_event() -- Low-level event dispatcher. */

int fastcall __ipipe_dispatch_event (unsigned event, void *data)
{
	struct ipipe_domain *start_domain, *this_domain, *next_domain;
	ipipe_event_handler_t evhand;
	struct list_head *pos, *npos;
	unsigned long flags;
	int propagate = 1;

	local_irq_save_hw(flags);

	start_domain = this_domain = ipipe_current_domain;

	list_for_each_safe(pos, npos, &__ipipe_pipeline) {
		/*
		 * Note: Domain migration may occur while running
		 * event or interrupt handlers, in which case the
		 * current register set is going to be recycled for a
		 * different domain than the initiating one. We do
		 * care for that, always tracking the current domain
		 * descriptor upon return from those handlers.
		 */
		next_domain = list_entry(pos, struct ipipe_domain, p_link);

		/*
		 * Keep a cached copy of the handler's address since
		 * ipipe_catch_event() may clear it under our feet.
		 */
		evhand = next_domain->evhand[event];

		if (evhand != NULL) {
			ipipe_current_domain = next_domain;
			ipipe_cpudom_var(next_domain, evsync) |= (1LL << event);
			local_irq_restore_hw(flags);
			propagate = !evhand(event, start_domain, data);
			local_irq_save_hw(flags);
			ipipe_cpudom_var(next_domain, evsync) &= ~(1LL << event);
			if (ipipe_current_domain != next_domain)
				this_domain = ipipe_current_domain;
		}

		if (next_domain != ipipe_root_domain &&	/* NEVER sync the root stage here. */
		    ipipe_cpudom_var(next_domain, irqpend_himask) != 0 &&
		    !test_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(next_domain, status))) {
			ipipe_current_domain = next_domain;
			__ipipe_sync_pipeline(IPIPE_IRQMASK_ANY);
			if (ipipe_current_domain != next_domain)
				this_domain = ipipe_current_domain;
		}

		ipipe_current_domain = this_domain;

		if (next_domain == this_domain || !propagate)
			break;
	}

	local_irq_restore_hw(flags);

	return !propagate;
}

/*
 * __ipipe_dispatch_wired -- Wired interrupt dispatcher. Wired
 * interrupts are immediately and unconditionally delivered to the
 * domain heading the pipeline upon receipt, and such domain must have
 * been registered as an invariant head for the system (priority ==
 * IPIPE_HEAD_PRIORITY). The motivation for using wired interrupts is
 * to get an extra-fast dispatching path for those IRQs, by relying on
 * a straightforward logic based on assumptions that must always be
 * true for invariant head domains.  The following assumptions are
 * made when dealing with such interrupts:
 *
 * 1- Wired interrupts are purely dynamic, i.e. the decision to
 * propagate them down the pipeline must be done from the head domain
 * ISR.
 * 2- Wired interrupts cannot be shared or sticky.
 * 3- The root domain cannot be an invariant pipeline head, in
 * consequence of what the root domain cannot handle wired
 * interrupts.
 * 4- Wired interrupts must have a valid acknowledge handler for the
 * head domain (if needed), and in any case, must not rely on handlers
 * provided by lower priority domains during the acknowledge cycle
 * (see __ipipe_handle_irq).
 *
 * Called with hw interrupts off.
 */

int fastcall __ipipe_dispatch_wired(struct ipipe_domain *head_domain, unsigned irq)
{
	struct ipipe_domain *old;

	if (test_bit(IPIPE_LOCK_FLAG, &head_domain->irqs[irq].control)) {
		/* If we can't process this IRQ right now, we must
		 * mark it as held, so that it will get played during
		 * normal log sync when the corresponding interrupt
		 * source is eventually unlocked. */
		ipipe_cpudom_var(head_domain, irqall)[irq]++;
		__set_bit(irq & IPIPE_IRQ_IMASK, &ipipe_cpudom_var(head_domain, irqheld_mask)[irq >> IPIPE_IRQ_ISHIFT]);
		return 0;
	}

	if (test_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(head_domain, status))) {
		__ipipe_set_irq_pending(head_domain, irq);
		return 0;
	}

	old = ipipe_current_domain;
	ipipe_current_domain = head_domain; /* Switch to the head domain. */

	ipipe_cpudom_var(head_domain, irqall)[irq]++;
	__set_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(head_domain, status));
	head_domain->irqs[irq].handler(irq, head_domain->irqs[irq].cookie); /* Call the ISR. */
	__ipipe_run_irqtail();
	__clear_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(head_domain, status));

	/* We expect the caller to start a complete pipeline walk upon
	 * return, so that propagated interrupts will get played. */

	if (ipipe_current_domain == head_domain)
		ipipe_current_domain = old; /* Back to the preempted domain. */

	return 1;
}

/*
 * __ipipe_sync_stage() -- Flush the pending IRQs for the current
 * domain (and processor). This routine flushes the interrupt log
 * (see "Optimistic interrupt protection" from D. Stodolsky et al. for
 * more on the deferred interrupt scheme). Every interrupt that
 * occurred while the pipeline was stalled gets played. WARNING:
 * callers on SMP boxen should always check for CPU migration on
 * return of this routine. One can control the kind of interrupts
 * which are going to be sync'ed using the syncmask
 * parameter. IPIPE_IRQMASK_ANY plays them all, IPIPE_IRQMASK_VIRT
 * plays virtual interrupts only.
 *
 * This routine must be called with hw interrupts off.
 */
void fastcall __ipipe_sync_stage(unsigned long syncmask)
{
	unsigned long mask, submask;
	struct ipipe_domain *ipd;
	int level, rank, cpu;
	unsigned irq;

	if (__test_and_set_bit(IPIPE_SYNC_FLAG, &ipipe_this_cpudom_var(status)))
		return;

	ipd = ipipe_current_domain;
	cpu = ipipe_processor_id();

	/*
	 * The policy here is to keep the dispatching code interrupt-free
	 * by stalling the current stage. If the upper domain handler
	 * (which we call) wants to re-enable interrupts while in a safe
	 * portion of the code (e.g. SA_INTERRUPT flag unset for Linux's
	 * sigaction()), it will have to unstall (then stall again before
	 * returning to us!) the stage when it sees fit.
	 */
	while ((mask = (ipipe_this_cpudom_var(irqpend_himask) & syncmask)) != 0) {
		level = __ipipe_ffnz(mask);

		while ((submask = ipipe_this_cpudom_var(irqpend_lomask)[level]) != 0) {
			rank = __ipipe_ffnz(submask);
			irq = (level << IPIPE_IRQ_ISHIFT) + rank;

			if (test_bit(IPIPE_LOCK_FLAG, &ipd->irqs[irq].control)) {
				__clear_bit(rank, &ipipe_this_cpudom_var(irqpend_lomask)[level]);
				continue;
			}

			__clear_bit(rank, &ipipe_this_cpudom_var(irqpend_lomask)[level]);

			if (ipipe_this_cpudom_var(irqpend_lomask)[level] == 0)
				__clear_bit(level, &ipipe_this_cpudom_var(irqpend_himask));

			__set_bit(IPIPE_STALL_FLAG, &ipipe_this_cpudom_var(status));

			if (ipd == ipipe_root_domain)
				trace_hardirqs_off();

			__ipipe_run_isr(ipd, irq);
#ifdef CONFIG_SMP
			{
				int newcpu = ipipe_processor_id();

				if (newcpu != cpu) {	/* Handle CPU migration. */
					/*
					 * We expect any domain to clear the SYNC bit each
					 * time it switches in a new task, so that preemptions
					 * and/or CPU migrations (in the SMP case) over the
					 * ISR do not lock out the log syncer for some
					 * indefinite amount of time. In the Linux case,
					 * schedule() handles this (see kernel/sched.c). For
					 * this reason, we don't bother clearing it here for
					 * the source CPU in the migration handling case,
					 * since it must have scheduled another task in by
					 * now.
					 */
					__set_bit(IPIPE_SYNC_FLAG, &ipipe_this_cpudom_var(status));
					cpu = newcpu;
				}
			}
#endif	/* CONFIG_SMP */
			if (ipd == ipipe_root_domain &&
			    test_bit(IPIPE_STALL_FLAG, &ipipe_this_cpudom_var(status)))
				trace_hardirqs_on();

			__clear_bit(IPIPE_STALL_FLAG, &ipipe_this_cpudom_var(status));
		}
	}

	__clear_bit(IPIPE_SYNC_FLAG, &ipipe_this_cpudom_var(status));
}

/* ipipe_register_domain() -- Link a new domain to the pipeline. */

int ipipe_register_domain(struct ipipe_domain *ipd,
			  struct ipipe_domain_attr *attr)
{
	struct ipipe_domain *_ipd;
	struct list_head *pos;
	unsigned long flags;

	if (!ipipe_root_domain_p) {
		printk(KERN_WARNING
		       "I-pipe: Only the root domain may register a new domain.\n");
		return -EPERM;
	}

	if (attr->priority == IPIPE_HEAD_PRIORITY &&
	    test_bit(IPIPE_AHEAD_FLAG,&__ipipe_pipeline_head()->flags))
		return -EAGAIN;	/* Cannot override current head. */

	flags = ipipe_critical_enter(NULL);

	pos = NULL;
	ipd->slot = ffz(__ipipe_domain_slot_map);

	if (ipd->slot < CONFIG_IPIPE_DOMAINS) {
		set_bit(ipd->slot, &__ipipe_domain_slot_map);
		list_for_each(pos, &__ipipe_pipeline) {
			_ipd = list_entry(pos, struct ipipe_domain, p_link);
			if (_ipd->domid == attr->domid)
				break;
		}
	}

	ipipe_critical_exit(flags);

	if (pos != &__ipipe_pipeline) {
		if (ipd->slot < CONFIG_IPIPE_DOMAINS)
			clear_bit(ipd->slot, &__ipipe_domain_slot_map);
		return -EBUSY;
	}

#ifndef CONFIG_SMP
	/*
	 * Set up the perdomain pointers for direct access to the
	 * percpu domain data. This saves a costly multiply each time
	 * we need to refer to the contents of the percpu domain data
	 * array.
	 */
	__raw_get_cpu_var(ipipe_percpu_daddr)[ipd->slot] = &__raw_get_cpu_var(ipipe_percpu_darray)[ipd->slot];
#endif

	ipd->name = attr->name;
	ipd->domid = attr->domid;
	ipd->pdd = attr->pdd;
	ipd->flags = 0;

	if (attr->priority == IPIPE_HEAD_PRIORITY) {
		ipd->priority = INT_MAX;
		__set_bit(IPIPE_AHEAD_FLAG,&ipd->flags);
	}
	else
		ipd->priority = attr->priority;

	__ipipe_init_stage(ipd);

	INIT_LIST_HEAD(&ipd->p_link);

#ifdef CONFIG_PROC_FS
	__ipipe_add_domain_proc(ipd);
#endif /* CONFIG_PROC_FS */

	flags = ipipe_critical_enter(NULL);

	list_for_each(pos, &__ipipe_pipeline) {
		_ipd = list_entry(pos, struct ipipe_domain, p_link);
		if (ipd->priority > _ipd->priority)
			break;
	}

	list_add_tail(&ipd->p_link, pos);

	ipipe_critical_exit(flags);

	printk(KERN_INFO "I-pipe: Domain %s registered.\n", ipd->name);

	/*
	 * Finally, allow the new domain to perform its initialization
	 * chores.
	 */

	if (attr->entry != NULL) {
		ipipe_current_domain = ipd;
		attr->entry();
		ipipe_current_domain = ipipe_root_domain;

		local_irq_save_hw(flags);

		if (ipipe_root_cpudom_var(irqpend_himask) != 0 &&
		    !test_bit(IPIPE_STALL_FLAG, &ipipe_root_cpudom_var(status)))
			__ipipe_sync_pipeline(IPIPE_IRQMASK_ANY);

		local_irq_restore_hw(flags);
	}

	return 0;
}

/* ipipe_unregister_domain() -- Remove a domain from the pipeline. */

int ipipe_unregister_domain(struct ipipe_domain *ipd)
{
	unsigned long flags;

	if (!ipipe_root_domain_p) {
		printk(KERN_WARNING
		       "I-pipe: Only the root domain may unregister a domain.\n");
		return -EPERM;
	}

	if (ipd == ipipe_root_domain) {
		printk(KERN_WARNING
		       "I-pipe: Cannot unregister the root domain.\n");
		return -EPERM;
	}
#ifdef CONFIG_SMP
	{
		unsigned irq;
		int cpu;

		/*
		 * In the SMP case, wait for the logged events to drain on
		 * other processors before eventually removing the domain
		 * from the pipeline.
		 */

		ipipe_unstall_pipeline_from(ipd);

		flags = ipipe_critical_enter(NULL);

		for (irq = 0; irq < IPIPE_NR_IRQS; irq++) {
			clear_bit(IPIPE_HANDLE_FLAG, &ipd->irqs[irq].control);
			clear_bit(IPIPE_STICKY_FLAG, &ipd->irqs[irq].control);
			set_bit(IPIPE_PASS_FLAG, &ipd->irqs[irq].control);
		}

		ipipe_critical_exit(flags);

		for_each_online_cpu(cpu) {
			while (ipipe_percpudom(ipd, irqpend_himask, cpu) > 0)
				cpu_relax();
		}
	}
#endif	/* CONFIG_SMP */

	mutex_lock(&ipd->mutex);

#ifdef CONFIG_PROC_FS
	__ipipe_remove_domain_proc(ipd);
#endif /* CONFIG_PROC_FS */

	/*
	 * Simply remove the domain from the pipeline and we are almost done.
	 */

	flags = ipipe_critical_enter(NULL);
	list_del_init(&ipd->p_link);
	ipipe_critical_exit(flags);

	__ipipe_cleanup_domain(ipd);

	mutex_unlock(&ipd->mutex);

	printk(KERN_INFO "I-pipe: Domain %s unregistered.\n", ipd->name);

	return 0;
}

/*
 * ipipe_propagate_irq() -- Force a given IRQ propagation on behalf of
 * a running interrupt handler to the next domain down the pipeline.
 * ipipe_schedule_irq() -- Does almost the same as above, but attempts
 * to pend the interrupt for the current domain first.
 */
int fastcall __ipipe_schedule_irq(unsigned irq, struct list_head *head)
{
	struct ipipe_domain *ipd;
	struct list_head *ln;
	unsigned long flags;

	if (irq >= IPIPE_NR_IRQS ||
	    (ipipe_virtual_irq_p(irq)
	     && !test_bit(irq - IPIPE_VIRQ_BASE, &__ipipe_virtual_irq_map)))
		return -EINVAL;

	local_irq_save_hw(flags);

	ln = head;

	while (ln != &__ipipe_pipeline) {

		ipd = list_entry(ln, struct ipipe_domain, p_link);

		if (test_bit(IPIPE_HANDLE_FLAG, &ipd->irqs[irq].control)) {
			__ipipe_set_irq_pending(ipd, irq);
			local_irq_restore_hw(flags);
			return 1;
		}

		ln = ipd->p_link.next;
	}

	local_irq_restore_hw(flags);

	return 0;
}

/* ipipe_free_virq() -- Release a virtual/soft interrupt. */

int ipipe_free_virq(unsigned virq)
{
	if (!ipipe_virtual_irq_p(virq))
		return -EINVAL;

	clear_bit(virq - IPIPE_VIRQ_BASE, &__ipipe_virtual_irq_map);

	return 0;
}

void ipipe_init_attr(struct ipipe_domain_attr *attr)
{
	attr->name = "anon";
	attr->domid = 1;
	attr->entry = NULL;
	attr->priority = IPIPE_ROOT_PRIO;
	attr->pdd = NULL;
}

/*
 * ipipe_catch_event() -- Interpose or remove an event handler for a
 * given domain.
 */
ipipe_event_handler_t ipipe_catch_event(struct ipipe_domain *ipd,
					unsigned event,
					ipipe_event_handler_t handler)
{
	ipipe_event_handler_t old_handler;
	unsigned long flags;
	int self = 0, cpu;

	if (event & IPIPE_EVENT_SELF) {
		event &= ~IPIPE_EVENT_SELF;
		self = 1;
	}

	if (event >= IPIPE_NR_EVENTS)
		return NULL;

	flags = ipipe_critical_enter(NULL);

	if (!(old_handler = xchg(&ipd->evhand[event],handler)))	{
		if (handler) {
			if (self)
				ipd->evself |= (1LL << event);
			else
				__ipipe_event_monitors[event]++;
		}
	}
	else if (!handler) {
		if (ipd->evself & (1LL << event))
			ipd->evself &= ~(1LL << event);
		else
			__ipipe_event_monitors[event]--;
	} else if ((ipd->evself & (1LL << event)) && !self) {
			__ipipe_event_monitors[event]++;
			ipd->evself &= ~(1LL << event);
	} else if (!(ipd->evself & (1LL << event)) && self) {
			__ipipe_event_monitors[event]--;
			ipd->evself |= (1LL << event);
	}

	ipipe_critical_exit(flags);

	if (!handler && ipipe_root_domain_p) {
		/*
		 * If we cleared a handler on behalf of the root
		 * domain, we have to wait for any current invocation
		 * to drain, since our caller might subsequently unmap
		 * the target domain. To this aim, this code
		 * synchronizes with __ipipe_dispatch_event(),
		 * guaranteeing that either the dispatcher sees a null
		 * handler in which case it discards the invocation
		 * (which also prevents from entering a livelock), or
		 * finds a valid handler and calls it. Symmetrically,
		 * ipipe_catch_event() ensures that the called code
		 * won't be unmapped under our feet until the event
		 * synchronization flag is cleared for the given event
		 * on all CPUs.
		 */

		for_each_online_cpu(cpu) {
			while (ipipe_percpudom(ipd, evsync, cpu) & (1LL << event))
				schedule_timeout_interruptible(HZ / 50);
		}
	}

	return old_handler;
}

cpumask_t ipipe_set_irq_affinity (unsigned irq, cpumask_t cpumask)
{
#ifdef CONFIG_SMP
	if (irq >= IPIPE_NR_XIRQS)
		/* Allow changing affinity of external IRQs only. */
		return CPU_MASK_NONE;

	if (num_online_cpus() > 1)
		return __ipipe_set_irq_affinity(irq,cpumask);
#endif /* CONFIG_SMP */

	return CPU_MASK_NONE;
}

int fastcall ipipe_send_ipi (unsigned ipi, cpumask_t cpumask)

{
#ifdef CONFIG_SMP
	return __ipipe_send_ipi(ipi,cpumask);
#else /* !CONFIG_SMP */
	return -EINVAL;
#endif /* CONFIG_SMP */
}

int ipipe_alloc_ptdkey (void)
{
	unsigned long flags;
	int key = -1;

	spin_lock_irqsave(&__ipipe_pipelock,flags);

	if (__ipipe_ptd_key_count < IPIPE_ROOT_NPTDKEYS) {
		key = ffz(__ipipe_ptd_key_map);
		set_bit(key,&__ipipe_ptd_key_map);
		__ipipe_ptd_key_count++;
	}

	spin_unlock_irqrestore(&__ipipe_pipelock,flags);

	return key;
}

int ipipe_free_ptdkey (int key)
{
	unsigned long flags;

	if (key < 0 || key >= IPIPE_ROOT_NPTDKEYS)
		return -EINVAL;

	spin_lock_irqsave(&__ipipe_pipelock,flags);

	if (test_and_clear_bit(key,&__ipipe_ptd_key_map))
		__ipipe_ptd_key_count--;

	spin_unlock_irqrestore(&__ipipe_pipelock,flags);

	return 0;
}

int fastcall ipipe_set_ptd (int key, void *value)

{
	if (key < 0 || key >= IPIPE_ROOT_NPTDKEYS)
		return -EINVAL;

	current->ptd[key] = value;

	return 0;
}

void fastcall *ipipe_get_ptd (int key)

{
	if (key < 0 || key >= IPIPE_ROOT_NPTDKEYS)
		return NULL;

	return current->ptd[key];
}

#ifdef CONFIG_PROC_FS

struct proc_dir_entry *ipipe_proc_root;

static int __ipipe_version_info_proc(char *page,
				     char **start,
				     off_t off, int count, int *eof, void *data)
{
	int len = sprintf(page, "%s\n", IPIPE_VERSION_STRING);

	len -= off;

	if (len <= off + count)
		*eof = 1;

	*start = page + off;

	if(len > count)
		len = count;

	if(len < 0)
		len = 0;

	return len;
}

static int __ipipe_common_info_show(struct seq_file *p, void *data)
{
	struct ipipe_domain *ipd = (struct ipipe_domain *)p->private;
	char handling, stickiness, lockbit, exclusive, virtuality;

	unsigned long ctlbits;
	unsigned irq;

	seq_printf(p, "       +----- Handling ([A]ccepted, [G]rabbed, [W]ired, [D]iscarded)\n");
	seq_printf(p, "       |+---- Sticky\n");
	seq_printf(p, "       ||+--- Locked\n");
	seq_printf(p, "       |||+-- Exclusive\n");
	seq_printf(p, "       ||||+- Virtual\n");
	seq_printf(p, "[IRQ]  |||||\n");

	mutex_lock(&ipd->mutex);

	for (irq = 0; irq < IPIPE_NR_IRQS; irq++) {
		/* Remember to protect against
		 * ipipe_virtual_irq/ipipe_control_irq if more fields
		 * get involved. */
		ctlbits = ipd->irqs[irq].control;

		if (irq >= IPIPE_NR_XIRQS && !ipipe_virtual_irq_p(irq))
			/*
			 * There might be a hole between the last external
			 * IRQ and the first virtual one; skip it.
			 */
			continue;

		if (ipipe_virtual_irq_p(irq)
		    && !test_bit(irq - IPIPE_VIRQ_BASE, &__ipipe_virtual_irq_map))
			/* Non-allocated virtual IRQ; skip it. */
			continue;

		/*
		 * Statuses are as follows:
		 * o "accepted" means handled _and_ passed down the pipeline.
		 * o "grabbed" means handled, but the interrupt might be
		 * terminated _or_ passed down the pipeline depending on
		 * what the domain handler asks for to the I-pipe.
		 * o "wired" is basically the same as "grabbed", except that
		 * the interrupt is unconditionally delivered to an invariant
		 * pipeline head domain.
		 * o "passed" means unhandled by the domain but passed
		 * down the pipeline.
		 * o "discarded" means unhandled and _not_ passed down the
		 * pipeline. The interrupt merely disappears from the
		 * current domain down to the end of the pipeline.
		 */
		if (ctlbits & IPIPE_HANDLE_MASK) {
			if (ctlbits & IPIPE_PASS_MASK)
				handling = 'A';
			else if (ctlbits & IPIPE_WIRED_MASK)
				handling = 'W';
			else
				handling = 'G';
		} else if (ctlbits & IPIPE_PASS_MASK)
			/* Do not output if no major action is taken. */
			continue;
		else
			handling = 'D';

		if (ctlbits & IPIPE_STICKY_MASK)
			stickiness = 'S';
		else
			stickiness = '.';

		if (ctlbits & IPIPE_LOCK_MASK)
			lockbit = 'L';
		else
			lockbit = '.';

		if (ctlbits & IPIPE_EXCLUSIVE_MASK)
			exclusive = 'X';
		else
			exclusive = '.';

		if (ipipe_virtual_irq_p(irq))
			virtuality = 'V';
		else
			virtuality = '.';

		seq_printf(p, " %3u:  %c%c%c%c%c\n",
			     irq, handling, stickiness, lockbit, exclusive, virtuality);
	}

	seq_printf(p, "[Domain info]\n");

	seq_printf(p, "id=0x%.8x\n", ipd->domid);

	if (test_bit(IPIPE_AHEAD_FLAG,&ipd->flags))
		seq_printf(p, "priority=topmost\n");
	else
		seq_printf(p, "priority=%d\n", ipd->priority);

	mutex_unlock(&ipd->mutex);

	return 0;
}

static int __ipipe_common_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, __ipipe_common_info_show, PROC_I(inode)->pde->data);
}

static struct file_operations __ipipe_info_proc_ops = {
	.owner		= THIS_MODULE,
	.open		= __ipipe_common_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void __ipipe_add_domain_proc(struct ipipe_domain *ipd)
{
	struct proc_dir_entry *e = create_proc_entry(ipd->name, 0444, ipipe_proc_root);
	if (e) {
		e->proc_fops = &__ipipe_info_proc_ops;
		e->data = (void*) ipd;
	}
}

void __ipipe_remove_domain_proc(struct ipipe_domain *ipd)
{
	remove_proc_entry(ipd->name,ipipe_proc_root);
}

void __init ipipe_init_proc(void)
{
	ipipe_proc_root = create_proc_entry("ipipe",S_IFDIR, 0);
	create_proc_read_entry("version",0444,ipipe_proc_root,&__ipipe_version_info_proc,NULL);
	__ipipe_add_domain_proc(ipipe_root_domain);

	__ipipe_init_tracer();
}

#endif	/* CONFIG_PROC_FS */

#ifdef CONFIG_IPIPE_DEBUG_CONTEXT

DEFINE_PER_CPU(int, ipipe_percpu_context_check) = { 1 };

void ipipe_check_context(struct ipipe_domain *border_ipd)
{
	/* Note: We don't make the per_cpu access atomic. We assume that code
	   which temporarily disables the check does this in atomic context
	   only. */
	if (likely(ipipe_current_domain->priority <= border_ipd->priority) ||
	    !per_cpu(ipipe_percpu_context_check, ipipe_processor_id()))
		return;

	ipipe_context_check_off();

	ipipe_trace_panic_freeze();
	ipipe_set_printk_sync(ipipe_current_domain);
	printk(KERN_ERR "I-pipe: Detected illicit call from domain '%s'\n"
	       KERN_ERR "        into a service reserved for domain '%s' and "
			"below.\n",
	       ipipe_current_domain->name, border_ipd->name);
	show_stack(NULL, NULL);
	ipipe_trace_panic_dump();
}

EXPORT_SYMBOL(ipipe_check_context);
#endif /* CONFIG_IPIPE_DEBUG_CONTEXT */

EXPORT_SYMBOL(ipipe_virtualize_irq);
EXPORT_SYMBOL(ipipe_control_irq);
EXPORT_SYMBOL(ipipe_suspend_domain);
EXPORT_SYMBOL(ipipe_alloc_virq);
EXPORT_PER_CPU_SYMBOL(ipipe_percpu_domain);
EXPORT_PER_CPU_SYMBOL(ipipe_percpu_darray);
EXPORT_SYMBOL(ipipe_root);
EXPORT_SYMBOL(ipipe_stall_pipeline_from);
EXPORT_SYMBOL(ipipe_test_and_stall_pipeline_from);
EXPORT_SYMBOL(ipipe_unstall_pipeline_from);
EXPORT_SYMBOL(ipipe_restore_pipeline_from);
EXPORT_SYMBOL(ipipe_test_and_unstall_pipeline_from);
EXPORT_SYMBOL(ipipe_unstall_pipeline_head);
EXPORT_SYMBOL(__ipipe_restore_pipeline_head);
EXPORT_SYMBOL(__ipipe_unstall_root);
EXPORT_SYMBOL(__ipipe_restore_root);
EXPORT_SYMBOL(__ipipe_spin_lock_irq);
EXPORT_SYMBOL(__ipipe_spin_unlock_irq);
EXPORT_SYMBOL(__ipipe_spin_lock_irqsave);
EXPORT_SYMBOL(__ipipe_spin_unlock_irqrestore);
EXPORT_SYMBOL(__ipipe_pipeline);
EXPORT_SYMBOL(__ipipe_lock_irq);
EXPORT_SYMBOL(__ipipe_unlock_irq);
EXPORT_SYMBOL(ipipe_register_domain);
EXPORT_SYMBOL(ipipe_unregister_domain);
EXPORT_SYMBOL(ipipe_free_virq);
EXPORT_SYMBOL(ipipe_init_attr);
EXPORT_SYMBOL(ipipe_catch_event);
EXPORT_SYMBOL(ipipe_alloc_ptdkey);
EXPORT_SYMBOL(ipipe_free_ptdkey);
EXPORT_SYMBOL(ipipe_set_ptd);
EXPORT_SYMBOL(ipipe_get_ptd);
EXPORT_SYMBOL(ipipe_set_irq_affinity);
EXPORT_SYMBOL(ipipe_send_ipi);
EXPORT_SYMBOL(__ipipe_schedule_irq);
#ifdef CONFIG_GENERIC_CLOCKEVENTS
EXPORT_SYMBOL(ipipe_request_tickdev);
EXPORT_SYMBOL(ipipe_release_tickdev);
#endif
