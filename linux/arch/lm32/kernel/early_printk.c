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

typedef struct LM32_uart {
  volatile unsigned int  rxtx;
  volatile unsigned int  ier;
  volatile unsigned int  iir;
  volatile unsigned int  lcr;
  volatile unsigned int  mcr;
  volatile unsigned int  lsr;
  volatile unsigned int  msr;
  volatile unsigned int  div;
} LM32_uart_t;

/* activate this to set the early console baudrate,
 * otherwise the old baudrate (from the bootloader) is used */
#undef LM32_EARLY_CONSOLE_SET_BAUDRATE

#define LM32_CONSOLE_BAUDRATE   9600
#define LM32_UART_LSR_THRE      (1 << 5)
#define LM32_UART_LSR_DR        (1 << 0)

static LM32_uart_t  *uart = 0;

#ifdef LM32_EARLY_CONSOLE_SET_BAUDRATE
static void early_console_setbrg( void )
{ 
  if (uart) {
    unsigned int  baud_by_100   = LM32_CONSOLE_BAUDRATE / 100;
    unsigned int  baud_multiple = 1024 * 1024 * baud_by_100;
    unsigned int  sysclk_by_100 = lm32tag_cpu[0]->frequency / 100;

    uart->div = (baud_multiple + (sysclk_by_100 / 2)) / sysclk_by_100;
  }

  return; 
}
#endif

static void early_console_putc (char c)
{
  if (uart) {
    if (c == '\n')
      early_console_putc('\r');

    /* wait for the transmit holding register empty (THRE) signal */
    while ((uart->lsr & LM32_UART_LSR_THRE) == 0) 
      /* spin */ ;

    uart->rxtx = c;
		/* uncomment this if you want to expose the uart -> address 0 bug
		if( *((volatile unsigned long*)(0x0)) != 0x98000000 )
					 asm volatile("break");
		*/
  }
}

// write string to console
static void
early_console_write(struct console *con, const char *s, unsigned n)
{
	while (n-- && *s) {
		if (*s == '\n')
			early_console_putc('\r');
		early_console_putc(*s);
		s++;
	}
}

// setup console
static int
early_console_setup(struct console *con, char *s)
{
  if (uart) {
    /* disable UART interrupts */
    uart->ier = 0;
    /* set to 8bit, 1 stop character */
    uart->lcr = ( 0x3 | 0x4 );

    /* set up baud rate */
#ifdef LM32_EARLY_CONSOLE_SET_BAUDRATE
    early_console_setbrg();
#endif

		if( s )
			early_console_write(con, s, strlen(s));

		return 0;
  } else {
		return -1;
	}
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

	if( lm32tag_num_uart > 0 )
		uart = (LM32_uart_t*)lm32tag_uart[0]->addr;

	register_console(&early_console);
}
