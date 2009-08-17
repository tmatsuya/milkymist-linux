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
 *  linux/arch/lm32/mm/kmap.c
 *
 *  Based on:
 *
 *  linux/arch/m68knommu/mm/kmap.c
 *  Copyright (C) 2000 Lineo, <davidm@snapgear.com>
 *  Copyright (C) 2000-2002 David McCullough <davidm@snapgear.com>
 */

#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/io.h>
#include <asm/system.h>

#undef DEBUG

/*
 * Map some physical address range into the kernel address space.
 */
void *__ioremap(unsigned long physaddr, unsigned long size, int cacheflag)
{
	return (void *)physaddr;
}

/*
 * Unmap a ioremap()ed region again.
 */
void iounmap(void *addr)
{
}

/*
 * __iounmap unmaps nearly everything, so be careful
 * it doesn't free currently pointer/page tables anymore but it
 * wans't used anyway and might be added later.
 */
void __iounmap(void *addr, unsigned long size)
{
}

/*
 * Set new cache mode for some kernel address space.
 * The caller must push data for that range itself, if such data may already
 * be in the cache.
 */
void kernel_set_cachemode(void *addr, unsigned long size, int cmode)
{
}
