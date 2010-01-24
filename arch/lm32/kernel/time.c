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

/*
 * Based on
 *
 * linux/arch/m68knommu/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/profile.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/interrupt.h>

//#include <asm/machdep.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/irq_regs.h>
#include <asm/setup.h>
#include <asm/uaccess.h>

#define MMPTR(x) (*((volatile unsigned int *)(x)))

#define CSR_TIMER0_CONTROL	MMPTR(0x80001010)
#define CSR_TIMER0_COMPARE	MMPTR(0x80001014)
#define CSR_TIMER0_COUNTER	MMPTR(0x80001018)

#define TIMER_ENABLE		(0x01)
#define TIMER_AUTORESTART	(0x02)

cycles_t lm32_cycles = 0;

static irqreturn_t timer_interrupt(int irq, void *timer_idx);

/* irq action description */
static struct irqaction lm32_core_timer_irqaction = {
	.name = "LM32 Timer Tick",
	.flags = IRQF_DISABLED,
	.handler = timer_interrupt,
};

/*
 * timer_interrupt() needs to call the "do_timer()"
 * routine every clocktick
 */
static irqreturn_t timer_interrupt(int irq, void *arg)
{
	lm32_cycles += CSR_TIMER0_COMPARE;
	write_seqlock(&xtime_lock);

	do_timer(1);
#ifndef CONFIG_SMP
	update_process_times(user_mode(0));
#endif

	if (current->pid)
		profile_tick(CPU_PROFILING);

	write_sequnlock(&xtime_lock);
	return(IRQ_HANDLED);
}

void time_init(void)
{
	xtime.tv_sec = 0;
	xtime.tv_nsec = 0;

	wall_to_monotonic.tv_sec = -xtime.tv_sec;

	lm32_systimer_program(1, cpu_frequency / HZ);

	if( setup_irq(IRQ_SYSTMR, &lm32_core_timer_irqaction) )
		panic("could not attach timer interrupt!");

	lm32_irq_unmask(IRQ_SYSTMR);
}

static unsigned long get_time_offset(void)
{
	return CSR_TIMER0_COUNTER/(cpu_frequency / HZ);
}

cycles_t get_cycles(void)
{
	return lm32_cycles +
		CSR_TIMER0_COUNTER;
}

void lm32_systimer_program(int periodic, cycles_t cyc)
{
	/* stop timer */
	CSR_TIMER0_CONTROL = 0;
	/* reset/configure timer */
	CSR_TIMER0_COUNTER = 0;
	CSR_TIMER0_COMPARE = cyc;
	/* start timer */
	CSR_TIMER0_CONTROL = periodic ? TIMER_ENABLE|TIMER_AUTORESTART : TIMER_ENABLE;
}

/*
 * This version of gettimeofday has near microsecond resolution.
 */
void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;
	unsigned long seq;
	unsigned long usec, sec;

	do {
		seq = read_seqbegin_irqsave(&xtime_lock, flags);
		usec = get_time_offset();
		sec = xtime.tv_sec;
		usec += (xtime.tv_nsec / 1000);
	} while (read_seqretry_irqrestore(&xtime_lock, seq, flags));

	while (usec >= 1000000) {
		usec -= 1000000;
		sec++;
	}

	tv->tv_sec = sec;
	tv->tv_usec = usec;
}
EXPORT_SYMBOL(do_gettimeofday);

int do_settimeofday(struct timespec *tv)
{
	time_t wtm_sec, sec = tv->tv_sec;
	long wtm_nsec, nsec = tv->tv_nsec;

	if ((unsigned long)tv->tv_nsec >= NSEC_PER_SEC)
		return -EINVAL;

	write_seqlock_irq(&xtime_lock);
	/*
	 * This is revolting. We need to set the xtime.tv_usec
	 * correctly. However, the value in this location is
	 * is value at the last tick.
	 * Discover what correction gettimeofday
	 * would have done, and then undo it!
	 */
	nsec -= (get_time_offset() * 1000);

	wtm_sec  = wall_to_monotonic.tv_sec + (xtime.tv_sec - sec);
	wtm_nsec = wall_to_monotonic.tv_nsec + (xtime.tv_nsec - nsec);

	set_normalized_timespec(&xtime, sec, nsec);
	set_normalized_timespec(&wall_to_monotonic, wtm_sec, wtm_nsec);

	ntp_clear();
	write_sequnlock_irq(&xtime_lock);
	clock_was_set();

	return 0;
}
EXPORT_SYMBOL(do_settimeofday);
