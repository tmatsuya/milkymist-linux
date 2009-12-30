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

#ifndef _LM32_ASM_IRQ_H
#define _LM32_ASM_IRQ_H

/* # of lm32 interrupts */
#define NR_IRQS (32)

/* # of lm32 irq levels */
#define NR_IRQLVL	1

#define	NO_IRQ		(-1)

/* LM32: we always use the first timer in the hardware-description as the system timer */
extern unsigned int  lm32_core_timer_irq;

#define	IRQ_GPIO		(0)
#define	IRQ_TIMER0		(1)
#define	IRQ_TIMER1		(2)
#define	IRQ_UARTRX		(3)
#define	IRQ_UARTTX		(4)
#define	IRQ_AC97CRREQUEST	(5)
#define	IRQ_AC97CRREPLY		(6)
#define	IRQ_AC97DMAR		(7)
#define	IRQ_AC97DMAW		(8)
#define	IRQ_PFPU		(9)
#define	IRQ_TMU			(10)
#define	IRQ_KEYBOARD		(11)
#define	IRQ_MOUSE		(12)

#define IRQ_SYSTMR	(IRQ_TIMER0)

#include <linux/irq.h>

#define irq_canonicalize(i) (i)

extern unsigned long irq_err_count;
static inline void ack_bad_irq(int irq)
{
	irq_err_count++;
}


/* in arch/lm32/kernel/irq.c */
void lm32_irq_mask(unsigned int irq);
void lm32_irq_unmask(unsigned int irq);
void lm32_irq_ack(unsigned int irq);
unsigned long lm32_irq_pending(void);

#endif /* _LM32_ASM_IRQ_H_ */
