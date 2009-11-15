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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include <linux/errno.h>
#include <linux/seq_file.h>

unsigned long irq_err_count;
/* this is the current mask in IM */
static unsigned long lm32_current_irq_mask = 0;

/*
 * NOP IRQ functions
 */
static void noop(unsigned int irq)
{
}

static unsigned int noop_ret(unsigned int irq)
{
	return 0;
}

void lm32_irq_mask(unsigned int irq)
{
	unsigned long flags;
	unsigned long mask = ~(1 << irq);

	local_irq_save_hw(flags);

#ifdef CONFIG_IPIPE
	ipipe_irq_lock(irq);
#endif

	mask &= lm32_current_irq_mask;
	lm32_current_irq_mask = mask;

	/*
	 * set mask
	 */
	asm volatile ("wcsr IM, %0" : : "r"(mask));

	local_irq_restore_hw(flags);
}

void lm32_irq_unmask(unsigned int irq)
{
	unsigned long flags;
	unsigned long mask = (1 << irq);

	if( !(lm32_current_irq_mask & mask) ) {
		local_irq_save_hw(flags);

		mask |= lm32_current_irq_mask;
		lm32_current_irq_mask = mask;

		/*
		 * set mask
		 */
		asm volatile ("wcsr IM, %0" : : "r"(mask));

#ifdef CONFIG_IPIPE
		ipipe_irq_unlock(irq);
#endif

		local_irq_restore_hw(flags);
	}
}

void lm32_irq_ack(unsigned int irq)
{
	unsigned long mask = 1 << irq;

	/*
	 * confirm irq
	 */
	asm volatile ("wcsr IP, %0" : : "r"(mask));
}

void lm32_irq_mask_ack(unsigned int irq)
{
	unsigned long flags;
	unsigned long mask = ~(1 << irq);
	unsigned long ack = 1 << irq;

	local_irq_save_hw(flags);

	mask &= lm32_current_irq_mask;
	lm32_current_irq_mask = mask;

	/*
	 * set mask
	 */
	asm volatile ("wcsr IM, %0" : : "r"(mask));

	/*
	 * confirm irq
	 */
	asm volatile ("wcsr IP, %0" : : "r"(ack));

	local_irq_restore_hw(flags);
}

unsigned long lm32_irq_pending()
{
	unsigned long ret;

	/*
	 * read interrupt pending register
	 */
	asm volatile ("rcsr %0, IP" : "=r"(ret) : );

	return ret;
}

/*
 * LM32 IRQs implementation
 */
struct irq_chip lm32_internal_irq_chip = {
	.name		= "LM32",
	.startup	= noop_ret,
	.shutdown	= noop,
	.enable		= noop,
	.disable	= lm32_irq_mask,
	.ack		= lm32_irq_ack,
	.mask		= lm32_irq_mask,
	.unmask		= lm32_irq_unmask,
	.mask_ack		= lm32_irq_mask_ack,
	.end		= noop,
};

void __init init_IRQ(void)
{
	int irq;

	local_irq_disable();

	for (irq = 0; irq < NR_IRQS; irq++) {
		set_irq_chip(irq, &lm32_internal_irq_chip);
#ifdef CONFIG_IPIPE
		/* mask irq upon entry */
		set_irq_handler(irq, handle_level_irq);
#else
		set_irq_handler(irq, handle_simple_irq);
#endif
	}

#ifdef CONFIG_IPIPE
	 for (irq = 0; irq < NR_IRQS; irq++) {
		 struct irq_desc *desc = irq_desc + irq;
		 desc->ic_prio = 0;
		 desc->thr_prio = 0;
	 }
#endif /* CONFIG_IPIPE */
}

int show_interrupts(struct seq_file *p, void *v)
{
	int i = *(loff_t *) v;
	struct irqaction * action;
	unsigned long flags;

	if (i < NR_IRQS)
	{
		spin_lock_irqsave(&irq_desc[i].lock, flags);
		action = irq_desc[i].action;
		if( action )
		{
			seq_printf(p, "%3d: ", i);
			seq_printf(p, "%10s ", irq_desc[i].chip->name ? : "-");
			seq_printf(p, " %s", action->name);
			for (action = action->next; action; action = action->next)
				seq_printf(p, ", %s", action->name);
			seq_putc(p, '\n');
		}
		spin_unlock_irqrestore(&irq_desc[i].lock, flags);
	} else if (i == NR_IRQS) {
		seq_printf(p, "Errors: %lu\n", irq_err_count);
	}

	return 0;
}

/* Handle bad interrupts */
static struct irq_desc bad_irq_desc = {
	.handle_irq = handle_bad_irq,
	.lock = SPIN_LOCK_UNLOCKED
};

asmlinkage void manage_signals_irq(struct pt_regs* regs);

/*
 * do_IRQ handles all hardware IRQ's.  Decoded IRQs should not
 * come via this function.  Instead, they should provide their
 * own 'handler'
 */
asmlinkage void asm_do_IRQ(unsigned long vec, struct pt_regs *regs)
{
	struct pt_regs *old_regs;
	struct irq_desc *desc;
	unsigned int irq;
	
	old_regs = set_irq_regs(regs);

	irq_enter();

#ifndef CONFIG_IPIPE
	/* decode irq */
	for (irq=0 ; irq<32; ++irq ) {
		if ( vec & (1 << irq) ) {
			/* acknowledge */
			lm32_irq_ack(irq);
			generic_handle_irq(irq);
		}
	}
#endif

	irq_exit();

	set_irq_regs(old_regs);

	manage_signals_irq(regs);
}

#ifdef CONFIG_IPIPE
asmlinkage unsigned long __ipipe_grab_irq(int vec, struct pt_regs *regs)
{
	int ret = 0;

#if 0
	// show vec
	volatile unsigned long* ledbase = (volatile unsigned long*)lm32tag_leds[0]->addr; 
	*ledbase = ~( (vec & 0xFF) | ((vec & 0xFF0000) >> 16) );
	/* show lowest 8 irqs + irq 20 at position of irq 5 */
#endif

#if 0
	// count up to show interrupt activity
	volatile unsigned long* segbase = (volatile unsigned long*)lm32tag_7seg[0]->addr; 
	*segbase = 0x200 | ((*segbase + 1) & 0xFF);
#endif

	/* fastpath for system timer */
	/* only do system timer if it is not masked!
	 * if it is masked it cannot have caused this interrupt! */
	if( likely(
				(vec == IRQ_MASK_SYSTMR) &&
				(lm32_current_irq_mask & IRQ_MASK_SYSTMR)) ) {
		goto handle_tmr_irq;
	}

	{
		unsigned int irq = 0;
		unsigned long mask = 1;
		do {
			/* decode irq */
			while( (vec & mask & lm32_current_irq_mask) == 0 && (mask != 0) ) {
				irq++;
				mask = mask + mask;
			}

			if( mask ) {
				ipipe_trace_irq_entry(irq); 
				__ipipe_handle_irq(irq, regs);
				local_irq_disable_hw();
				ipipe_trace_irq_exit(irq);

				/* prepare for next iteration */
				irq++;
				mask = mask + mask;
			}
		} while( mask );
	}

	goto out;

handle_tmr_irq:
	ipipe_trace_irq_entry(IRQ_SYSTMR); 
	__ipipe_handle_irq(IRQ_SYSTMR, regs);
	ipipe_trace_irq_exit(IRQ_SYSTMR);

out:
	if (ipipe_root_domain_p)
		ret = !test_bit(IPIPE_STALL_FLAG, &ipipe_root_cpudom_var(status));

	/* TODO LM32 do something with return value */
	return ret;
}
#endif
