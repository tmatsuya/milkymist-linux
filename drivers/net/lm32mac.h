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

#ifndef DRIVERS_NET_LM32MAC_H
#define DRIVERS_NET_LM32MAC_H

#include <linux/spinlock.h>

/* define for debugging */
#undef DEBUG_TX
#undef DEBUG_RX
#undef DEBUG_INIT

/* define for packet content debugging (very noisy!) */
#undef DEBUG_DATA

#define DRIVER_NAME "lm32mac"

/* For interrupt driven transmit/send:
 * the buffer size is given in ulongs, so 2048 means 8 kilobytes of buffer */
#define LM32MAC_BUFFERSIZE            (2048)
#define LM32MAC_BUFFERSIZE_MASK       (LM32MAC_BUFFERSIZE-1)

typedef struct LM32MAC_Ringbuffer {
  volatile ulong first;
  volatile ulong length;
  volatile ulong data[LM32MAC_BUFFERSIZE];
} LM32MAC_Ringbuffer_t;

/* private eth interface data */
struct LM32MAC_Priv {
	int idx;
	LM32MAC_Ringbuffer_t in_buffer;
	LM32MAC_Ringbuffer_t out_buffer;
	/* lock to protect out_buffer (lm32mac_send vs lm32mac_trytosend) */
	spinlock_t out_buffer_lock;
	/* Statistics table. */
	struct net_device_stats stats;
};

#endif /* #ifndef DRIVERS_NET_LM32MAC_H */
