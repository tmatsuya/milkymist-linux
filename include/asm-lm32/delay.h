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
 * Based on:
 * include/asm-mips/delay.h
 */

#ifndef _LM32_DELAY_H
#define _LM32_DELAY_H

static inline void __delay(unsigned long loops)
{
	/* get a realistic delay by using add more often than branch 
	 * (branch needs 4 cycles if taken, add only one) */
	asm volatile(
		"andi r2, %0, 1\n"
		"be r2, r0, 8\n" /* jump over next instruction */
		"addi %0, %0, -1\n"
		"andi r2, %0, 2\n"
		"be r2, r0, 8\n" /* jump over next instruction */
		"addi %0, %0, -2\n"
		"andi r2, %0, 4\n"
		"be r2, r0, 8\n" /* jump over next instruction */
		"addi %0, %0, -4\n"
		"addi %0, %0, -1\n" /* loop */ 
		"addi %0, %0, -1\n"
		"addi %0, %0, -1\n"
		"addi %0, %0, -1\n"
		"addi %0, %0, -1\n"
		"addi %0, %0, -1\n"
		"addi %0, %0, -1\n"
		"addi %0, %0, -1\n"
		"bne %0, r0, -32\n"
		: "=r"(loops)
		: "0"(loops)
		: "r2"
	);
}

#include <linux/param.h>	/* needed for HZ */

/*
 * Use only for very small delays ( < 1 msec).  Should probably use a
 * lookup table, really, as the multiplications take much too long with
 * short delays.  This is a "reasonable" implementation, though (and the
 * first constant multiplications gets optimized away if the delay is
 * a constant)
 */
static inline void udelay(unsigned long usecs)
{
	extern unsigned long loops_per_jiffy;
	__delay(usecs * loops_per_jiffy / (1000000 / HZ));
}

#endif
