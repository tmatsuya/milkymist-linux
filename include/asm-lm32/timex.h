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

#ifndef _ASM_TIMEX_H
#define _ASM_TIMEX_H

#ifdef __KERNEL__

/* cannot use value supplied by bootloader because this value is used in an #if */
/* 100 MHz works as default value, even for 75 MHz bitstreams */
#define CLOCK_TICK_RATE		(100*1000*1000)

#endif /* __KERNEL__ */

typedef unsigned long long cycles_t;

cycles_t get_cycles(void);

void lm32_systimer_ack(void);
void lm32_systimer_program(int periodic, cycles_t cyc);

#endif /*  _ASM_TIMEX_H */
