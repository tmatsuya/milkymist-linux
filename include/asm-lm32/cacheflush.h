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

#ifndef _LM32_ASM_CACHEFLUSH_H
#define _LM32_ASM_CACHEFLUSH_H

#include <linux/mm.h>

#define flush_cache_all()			__flush_cache_all()
#define flush_cache_mm(mm)			__flush_cache_all()
#define flush_cache_dup_mm(mm)			__flush_cache_all()
#define flush_cache_range(vma, start, end)	__flush_cache_all()
#define flush_cache_page(vma, vmaddr)		__flush_cache_all()
#define flush_dcache_range(start,len)		__flush_cache_all()
#define flush_dcache_page(page)			__flush_cache_all()
#define flush_dcache_mmap_lock(mapping)		__flush_cache_all()
#define flush_dcache_mmap_unlock(mapping)	__flush_cache_all()
#define flush_icache_range(start,len)		__flush_cache_all()
#define flush_icache_page(vma,pg)		__flush_cache_all()
#define flush_icache_user_range(vma,pg,adr,len)	__flush_cache_all()
#define flush_cache_vmap(start, end)		__flush_cache_all()
#define flush_cache_vunmap(start, end)		__flush_cache_all()

#define copy_to_user_page(vma, page, vaddr, dst, src, len) \
	memcpy(dst, src, len)
#define copy_from_user_page(vma, page, vaddr, dst, src, len) \
	memcpy(dst, src, len)

static inline void __flush_cache_all(void)
{
	asm volatile (
			"nop\n"
			"wcsr DCC, r0\n"
			"nop\n"
	);
}

#endif /* _LM32_ASM_CACHEFLUSH_H */
