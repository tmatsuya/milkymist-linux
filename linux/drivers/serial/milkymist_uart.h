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

#ifndef __DRIVER_SERIAL_LM32UART_H
#define __DRIVER_SERIAL_LM32UART_H

#define LM32UART_DRIVERNAME "lm32uart"
#define LM32UART_DEVICENAME "ttyS"
#define LM32UART_MAJOR TTY_MAJOR // TODO is this correct?
#define LM32UART_MINOR 64 // TODO is this correct?

typedef struct LM32_uart_priv {
	/* ier, lcr are write only so we have to store it here for operations like |= */
	unsigned int ier;
	unsigned int lcr;
} LM32_uart_priv_t;

/* LM32 UART register layout */
typedef struct LM32_uart {
  volatile unsigned int  ucr;
  volatile unsigned int  rxtx;
  volatile unsigned int  div;
  volatile unsigned int  ier;
} LM32_uart_t;

/* Milkymist UART */
#define	LM32_UART_RX_AVAILABLE	0x01
#define	LM32_UART_RX_ERROR	0x02
#define	LM32_UART_RX_ACK	0x04
#define LM32_UART_TX_BUSY	0x08
#define	LM32_UART_TX_DONE	0x10
#define	LM32_UART_TX_ACK	0x20

/* IER bit definitions */
enum {
	LM32_UART_IER_MSI = (1 << 3),
	LM32_UART_IER_RLSI = (1 << 2),
	LM32_UART_IER_THRI = (1 << 1),
	LM32_UART_IER_RBRI = (1 << 0)
};

/* IIR bit definitions */
enum {
	LM32_UART_IIR_NONE = 1,
	LM32_UART_IIR_LSR_ERROR = ((1 << 2) | (1 << 1)),
	LM32_UART_IIR_LSR_DR = (1 << 2),
	LM32_UART_IIR_LSR_THRE = (1 << 1),
	LM32_UART_IIR_MSR = 0
};

/* LCR bit definitions */
enum {
	/* 0 = disable break assertion, 1 = enable */
	LM32_UART_LCR_SB = (1 << 5),
	/* 0 = disable stick parity, 1 = enable */
	LM32_UART_LCR_SP = (1 << 4),
	/* 0 = even parity, 1 = odd parity */
	LM32_UART_LCR_EPS = (1 << 3),
	/* 0 = disable parity bit, 1 = enable */
	LM32_UART_LCR_PEN = (1 << 3),
	/* 0 = 1 stop bit, 1 = 1.5 stop bits if WSL = 5 bit
	 * 2 stop bits if WSL > 5 bit */
	LM32_UART_LCR_STB = (1 << 2),
	/* all WSL bits */
	LM32_UART_LCR_WSL = ((1 << 1) | (1 << 0)),
	/* 8 bit data */
	LM32_UART_LCR_WSL8 = ((1 << 1) | (1 << 0)),
	/* 7 bit data */
	LM32_UART_LCR_WSL7 = (1 << 1),
	/* 6 bit data */
	LM32_UART_LCR_WSL6 = (1 << 0),
	/* 5 bit data */
	LM32_UART_LCR_WSL5 = 0
};

/* LSR bit definitions */
enum {
	LM32_UART_LSR_TEMT = (1 << 6),
	LM32_UART_LSR_THRE = (1 << 5),
	LM32_UART_LSR_BI = (1 << 4),
	LM32_UART_LSR_FE = (1 << 3),
	LM32_UART_LSR_PE = (1 << 2),
	LM32_UART_LSR_OE = (1 << 1),
	LM32_UART_LSR_DR = (1 << 0)
};

#endif /* #ifndef __DRIVER_SERIAL_LM32UART_H */
