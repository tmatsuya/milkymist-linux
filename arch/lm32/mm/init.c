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

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/initrd.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <linux/linkage.h>
#include <linux/pfn.h>

#include <asm-generic/sections.h>
#include <asm-lm32/setup.h>

//#undef DEBUG
#define DEBUG

/* in arch/lm32/kernel/setup.c */
extern unsigned long asmlinkage _kernel_arg_initrd_start;
extern unsigned long asmlinkage _kernel_arg_initrd_end;

unsigned long physical_memory_start;
unsigned long physical_memory_end;
unsigned long memory_start;
unsigned long memory_end;

void __init bootmem_init(void)
{
	unsigned long bootmap_size;

	/*
	 * Init memory
	 */
	physical_memory_start = sdram_start;
	physical_memory_end = sdram_start+sdram_size;
	if( ((unsigned long)_end < physical_memory_start) || ((unsigned long)_end > physical_memory_end) )
		printk("BUG: your kernel is not located in the ddr sdram");
	/* start after kernel code */
	memory_start = PAGE_ALIGN((unsigned long)_end);
	memory_end = physical_memory_end;
#ifdef DEBUG
	printk("memory from %lx - %lx\n", memory_start, memory_end);
#endif

	init_mm.start_code = (unsigned long)_stext;
	init_mm.end_code = (unsigned long)_etext;
	init_mm.end_data = (unsigned long)_edata;
	init_mm.brk = (unsigned long)0;

	/*
	 * Give all the memory to the bootmap allocator, tell it to put the
	 * boot mem_map at the start of memory.
	 */
	bootmap_size = init_bootmem_node(
			NODE_DATA(0),
			memory_start >> PAGE_SHIFT, /* map goes here */
			PAGE_OFFSET >> PAGE_SHIFT,
			memory_end >> PAGE_SHIFT);

	/*
	 * Free the usable memory, we have to make sure we do not free
	 * the bootmem bitmap so we then reserve it after freeing it :-)
	 */
	free_bootmem(memory_start, memory_end - memory_start);
	reserve_bootmem(memory_start, bootmap_size);

	/*
	 * reserve initrd boot memory
	 */
#ifdef CONFIG_BLK_DEV_INITRD
	if(_kernel_arg_initrd_start) {
		unsigned long reserve_start = _kernel_arg_initrd_start & PAGE_MASK;
		unsigned long reserve_end = (_kernel_arg_initrd_end + PAGE_SIZE-1) & PAGE_MASK;
		initrd_start = _kernel_arg_initrd_start;
		initrd_end = _kernel_arg_initrd_end;
		printk("reserving initrd memory: %lx size %lx\n", reserve_start, reserve_end-reserve_start);
		reserve_bootmem(reserve_start, reserve_end-reserve_start);
	}
#endif
}

/*
 * paging_init() continues the virtual memory environment setup which
 * was begun by the code in arch/head.S.
 * The parameters are pointers to where to stick the starting and ending
 * addresses of available kernel virtual memory.
 */
void __init paging_init(void)
{
	/*
	 * Make sure start_mem is page aligned, otherwise bootmem and
	 * page_alloc get different views of the world.
	 */
#ifdef DEBUG
	unsigned long start_mem = PAGE_ALIGN(memory_start);
#endif
	unsigned long end_mem   = memory_end & PAGE_MASK;

#ifdef DEBUG
	printk ("start_mem is %#lx\nvirtual_end is %#lx\n",
		start_mem, end_mem);
#endif

	set_fs(KERNEL_DS);

#ifdef DEBUG
	printk ("before free_area_init\n");

	printk ("free_area_init -> start_mem is %#lx\nvirtual_end is %#lx\n",
		start_mem, end_mem);
#endif

	{
		unsigned i;
		unsigned long zones_size[MAX_NR_ZONES];

		for (i = 0; i < MAX_NR_ZONES; i++)
			zones_size[i] = 0;
		zones_size[ZONE_NORMAL] = (end_mem - PAGE_OFFSET) >> PAGE_SHIFT;
		free_area_init(zones_size);
	}
	printk ("after free_area_init\n");
}

void __init mem_init(void)
{
	int codek = 0, datak = 0;
	unsigned long tmp;
	unsigned long start_mem = memory_start;
	unsigned long end_mem   = memory_end;
	/* TODO: use more of hardware setup to initialize memory */
	unsigned long ramlen = sdram_size;

#ifdef DEBUG
	printk(KERN_DEBUG "Mem_init: start=%lx, end=%lx\n", start_mem, end_mem);
#endif

	end_mem &= PAGE_MASK;
	high_memory = (void *) end_mem;

	start_mem = PAGE_ALIGN(start_mem);
	max_mapnr = num_physpages = (((unsigned long) high_memory) - PAGE_OFFSET) >> PAGE_SHIFT;

	/* this will put all memory onto the freelists */
	totalram_pages = free_all_bootmem();

	codek = (_etext - _stext) >> 10;
	datak = (__bss_stop - __bss_start) >> 10;

	tmp = nr_free_pages() << PAGE_SHIFT;
	printk(KERN_INFO "Memory available: %luk/%luk RAM, (%dk kernel code, %dk data)\n",
	       tmp >> 10,
	       ramlen >> 10,
	       codek,
	       datak
	       );
}

static void free_init_pages(const char *what, unsigned long start, unsigned long end) {
	unsigned long pfn;
	printk("Freeing %s mem: %ldk freed\n", what, (end-start) >> 10);

	for (pfn = PFN_UP(start); pfn < PFN_DOWN(end); pfn++) {
		struct page* page = pfn_to_page(pfn);

		ClearPageReserved(page);
		init_page_count(page);
		__free_page(page);
		totalram_pages++;
	}
}

void
free_initmem(void)
{
	/* this will create segfaults, so deactivated */
	/* free_init_pages("unused kernel", (unsigned long)&__init_begin, (unsigned* long)&__init_end); */
}

#ifdef CONFIG_BLK_DEV_INITRD
void __init free_initrd_mem(unsigned long start, unsigned long end) {
	free_init_pages("initrd", start, end);
}
#endif

void show_mem(void)
{
	unsigned long i;
	int free = 0, total = 0, reserved = 0, shared = 0;
	int cached = 0;

	printk(KERN_INFO "\nMem-info:\n");
	show_free_areas();
	i = max_mapnr;
	while (i-- > 0) {
		total++;
		if (PageReserved(mem_map+i))
			reserved++;
		else if (PageSwapCache(mem_map+i))
			cached++;
		else if (!page_count(mem_map+i))
			free++;
		else
			shared += page_count(mem_map+i) - 1;
	}
	printk(KERN_INFO "%d pages of RAM\n",total);
	printk(KERN_INFO "%d free pages\n",free);
	printk(KERN_INFO "%d reserved pages\n",reserved);
	printk(KERN_INFO "%d pages shared\n",shared);
	printk(KERN_INFO "%d pages swap cached\n",cached);
}
