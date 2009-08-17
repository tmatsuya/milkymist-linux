/**
 *   @ingroup hal
 *   @file
 *
 *   Adeos-based Real-Time Abstraction Layer for x86.
 *
 *   Inspired from original RTAI/x86 HAL interface: \n
 *   Copyright &copy; 2000 Paolo Mantegazza, \n
 *   Copyright &copy; 2000 Steve Papacharalambous, \n
 *   Copyright &copy; 2000 Stuart Hughes, \n
 *
 *   RTAI/x86 rewrite over Adeos: \n
 *   Copyright &copy; 2002-2007 Philippe Gerum.
 *   NMI watchdog, SMI workaround: \n
 *   Copyright &copy; 2004 Gilles Chanteperdrix.
 *
 *   LM32 port:
 *   Copyright (C) 2007 Theobroma Systems <mico32@theobroma-systems.com>
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
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *   02111-1307, USA.
 */

/**
 * @addtogroup hal
 *
 * x86-specific HAL services.
 *
 *@{*/

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/bitops.h>
#include <asm/system.h>
#include <asm/hardirq.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <asm/timex.h>
#include <asm/xenomai/hal.h>
#include <stdarg.h>

static struct {
	unsigned long flags;
	int count;
} rthal_linux_irq[IPIPE_NR_XIRQS];

int rthal_irq_host_request(unsigned irq,
			   rthal_irq_host_handler_t handler,
			   char *name, void *dev_id)
{
    unsigned long flags;

    if (irq >= IPIPE_NR_XIRQS || !handler)
        return -EINVAL;

    spin_lock_irqsave(rthal_irq_desc_lock(irq), flags);

    if (rthal_linux_irq[irq].count++ == 0 && rthal_irq_descp(irq)->action) {
        rthal_linux_irq[irq].flags = rthal_irq_descp(irq)->action->flags;
        rthal_irq_descp(irq)->action->flags |= IRQF_SHARED;
    }

    spin_unlock_irqrestore(rthal_irq_desc_lock(irq), flags);

    return request_irq(irq, handler, IRQF_SHARED, name, dev_id);
}

int rthal_irq_host_release(unsigned irq, void *dev_id)
{
    unsigned long flags;

    if (irq >= IPIPE_NR_XIRQS || rthal_linux_irq[irq].count == 0)
        return -EINVAL;

    free_irq(irq, dev_id);

    spin_lock_irqsave(rthal_irq_desc_lock(irq), flags);

    if (--rthal_linux_irq[irq].count == 0 && rthal_irq_descp(irq)->action)
        rthal_irq_descp(irq)->action->flags = rthal_linux_irq[irq].flags;

    spin_unlock_irqrestore(rthal_irq_desc_lock(irq), flags);

    return 0;
}

int rthal_irq_enable(unsigned irq)
{
	if (irq >= NR_IRQS)
		return -EINVAL;

	rthal_irq_desc_status(irq) &= ~IRQ_DISABLED;

	return rthal_irq_chip_enable(irq);
}

int rthal_irq_disable(unsigned irq)
{

	if (irq >= NR_IRQS)
		return -EINVAL;

	rthal_irq_desc_status(irq) |= IRQ_DISABLED;

	return rthal_irq_chip_disable(irq);
}

int rthal_irq_end(unsigned irq)
{
	if (irq >= NR_IRQS)
		return -EINVAL;

	return rthal_irq_chip_end(irq);
}

void rthal_timer_program(int periodic, cycles_t cyc)
{
	lm32_systimer_program(periodic, cyc);
}

unsigned long rthal_timer_calibrate(void)
{
	return 1000000000 / lm32tag_cpu[0]->frequency;
}

int rthal_timer_request(void (*handler)(void), int cpu)
{
    unsigned long flags;
    int err;

    flags = rthal_critical_enter(NULL);

		__ipipe_mach_timerstolen = 1;

    rthal_timer_program_shot(lm32tag_cpu[0]->frequency/HZ);

    err = rthal_irq_request(RTHAL_TIMER_IRQ,
                            (rthal_irq_handler_t) handler, NULL, NULL);

    rthal_critical_exit(flags);

    return err;
}

void rthal_timer_release(int cpu)
{
    unsigned long flags;

    flags = rthal_critical_enter(NULL);

    rthal_irq_release(RTHAL_TIMER_IRQ);

		__ipipe_mach_timerstolen = 0;

    rthal_critical_exit(flags);
}

static inline int do_exception_event(unsigned event, unsigned domid, void *data)
{
	if (domid == RTHAL_DOMAIN_ID) {
		rthal_realtime_faults[rthal_processor_id()][event]++;

		if (rthal_trap_handler != NULL &&
		    rthal_trap_handler(event, domid, data) != 0)
			return RTHAL_EVENT_STOP;
	}

	return RTHAL_EVENT_PROPAGATE;
}

RTHAL_DECLARE_EVENT(exception_event);

static inline void do_rthal_domain_entry(void)
{
	unsigned trapnr;

	/* Trap all faults. */
	for (trapnr = 0; trapnr < RTHAL_NR_FAULTS; trapnr++)
		rthal_catch_exception(trapnr, &exception_event);

	printk(KERN_INFO "Xenomai: hal/lm32 started.\n");
}

RTHAL_DECLARE_DOMAIN(rthal_domain_entry);

int rthal_arch_init(void)
{
	if (rthal_cpufreq_arg == 0)
		rthal_cpufreq_arg = lm32tag_cpu[0]->frequency;

	if (rthal_timerfreq_arg == 0)
		rthal_timerfreq_arg = lm32tag_cpu[0]->frequency;

	return 0;
}

void rthal_arch_cleanup(void)
{
	printk(KERN_INFO "Xenomai: hal/x86 stopped.\n");
}

EXPORT_SYMBOL(rthal_arch_init);
EXPORT_SYMBOL(rthal_arch_cleanup);
