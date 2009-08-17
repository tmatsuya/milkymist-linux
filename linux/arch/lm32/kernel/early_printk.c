/*
 * (C) Copyright 2009
 *     Sebastien Bourdeauducq
 * (C) Copyright 2007
 *     Theobroma Systems <www.theobroma-systems.com>
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
 * arch/mips/kernel/early_printk.c
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2002, 2003, 06, 07 Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2007 MIPS Technologies, Inc.
 *   written by Ralf Baechle (ralf@linux-mips.org)
 */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/string.h>
#include <asm-lm32/setup.h>

#define MMPTR(x) (*((volatile unsigned int *)(x)))
#define CSR_UART_UCR 		MMPTR(0x80000000)
#define CSR_UART_RXTX 		MMPTR(0x80000004)

#define UART_DR			(0x01)
#define UART_ERR		(0x02)
#define UART_BUSY		(0x04)

#define UART_TXBUSY		(0x08)
#define UART_TXDONE		(0x10)
#define UART_TXACK		(0x20)

static void early_console_putc (char c)
{
	while(CSR_UART_UCR & UART_TXBUSY);
	CSR_UART_RXTX = c;
}

// write string to console
static void
early_console_write(struct console *con, const char *s, unsigned n)
{
	while (n-- && *s) {
		early_console_putc(*s);
		s++;
	}
}

// setup console
static int
early_console_setup(struct console *con, char *s)
{
	if( s )
		early_console_write(con, s, strlen(s));

	return 0;
}

static struct console early_console = {
	.name	= "early",
	.write	= early_console_write,
	.setup = early_console_setup,
	.flags	= CON_PRINTBUFFER | CON_BOOT,
	.index	= -1
};

static int early_console_initialized;

void setup_early_printk(void)
{
	if (early_console_initialized)
		return;
	early_console_initialized = 1;

	register_console(&early_console);
}
