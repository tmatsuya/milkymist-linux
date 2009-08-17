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
 *  linux/arch/lm32/mm/memory.c
 *
 *  Based on:
 *
 *  linux/arch/m68k/mm/memory.c
 *  Copyright (C) 1998  Kenneth Albanowski <kjahds@kjahds.com>,
 *  Copyright (C) 1999-2002, Greg Ungerer (gerg@snapgear.com)
 *  Copyright (C) 1995  Hamish Macdonald
 */

#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/slab.h>

#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/traps.h>
#include <asm/io.h>

/*
 * cache_clear() semantics: Clear any cache entries for the area in question,
 * without writing back dirty entries first. This is useful if the data will
 * be overwritten anyway, e.g. by DMA to memory. The range is defined by a
 * _physical_ address.
 */

void cache_clear (unsigned long paddr, int len)
{
	(void)paddr;
	(void)len;
	asm volatile (
			"nop\n"
			"wcsr DCC, r0\n"
			"nop\n"
	);
}


/*
 * we can only invalidate the whole cache on the mico32
 */
static __inline__ void cache_invalidate_lines(unsigned long paddr, int len)
{
	cache_clear(paddr, len);
}


/*
 * cache_push() semantics: Write back any dirty cache data in the given area,
 * and invalidate the range in the instruction cache. It needs not (but may)
 * invalidate those entries also in the data cache. The range is defined by a
 * _physical_ address.
 */

void cache_push (unsigned long paddr, int len)
{
	cache_invalidate_lines(paddr, len);
}


/*
 * cache_push_v() semantics: Write back any dirty cache data in the given
 * area, and invalidate those entries at least in the instruction cache. This
 * is intended to be used after data has been written that can be executed as
 * code later. The range is defined by a _user_mode_ _virtual_ address  (or,
 * more exactly, the space is defined by the %sfc/%dfc register.)
 */

void cache_push_v (unsigned long vaddr, int len)
{
	cache_invalidate_lines(vaddr, len);
}

/* Map some physical address range into the kernel address space. The
 * code is copied and adapted from map_chunk().
 */

unsigned long kernel_map(unsigned long paddr, unsigned long size,
			 int nocacheflag, unsigned long *memavailp )
{
	return paddr;
}

