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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/setup.h>

static struct mtd_partition ecp250_partitions[] = {
	{
		.name = "U-Boot",
		.size = 0x00040000, /* 256 kB */
		.offset = 0x0000000
	},
	{
		.name = "Linux Kernel",
		.size = 0x00200000, /* 2 MB */
		.offset = MTDPART_OFS_APPEND,
	},
	{
		.name = "Free",
		.size = (0x02000000 - 0x40000 - 0x40000 - 0x200000), /* rest to play with */
		.offset = MTDPART_OFS_APPEND,
	},
	{
		.name = "U-Boot Env",
		.size = 0x40000, /* last 256 kB is U-Boot environment */
		.offset = MTDPART_OFS_APPEND,
	}
};

static struct map_info ecp250_map[1] = {
	{
		/* .name will be set from lm32tag_flash */
		/* .phys will be set from lm32tag_flash */
		/* .size will be set from lm32tag_flash */
		.bankwidth = 4, /* always 32bit access with Mico32 */
	},
};

static struct mtd_info *ecp250_flash_mtd[1] = {NULL};

static void __exit  cleanup_ecp250_flash(void)
{
	int i;

	for (i=0; i<2; i++) {
		if (ecp250_flash_mtd[i]) {
			del_mtd_partitions(ecp250_flash_mtd[i]);
			map_destroy(ecp250_flash_mtd[i]);
			iounmap(ecp250_map[0].virt);
		}
	}
}

static int __init init_ecp250_flash(void)
{
	printk("ecp250_flash: Flash driver initializing\n");

	if( lm32tag_num_flash > 1 )
		printk("Warning: only supporting single configured flash at the moment!\n");
	
	if( lm32tag_num_flash == 0 ) {
		printk("No flash devices found!\n");
		return -ENXIO;
	}

	ecp250_map[0].name = lm32tag_flash[0]->name;
	ecp250_map[0].phys = lm32tag_flash[0]->addr;
	ecp250_map[0].size = lm32tag_flash[0]->size;

	ecp250_map[0].virt = ioremap(ecp250_map[0].phys, ecp250_map[0].size);
	if (!ecp250_map[0].virt) {
		printk("Failed to ioremap Flash space\n");
		return -EIO;
	}

	ecp250_flash_mtd[0] = do_map_probe("cfi_probe", &ecp250_map[0]);
	if (ecp250_flash_mtd[0]) {
		ecp250_flash_mtd[0]->owner = THIS_MODULE;
		add_mtd_partitions(ecp250_flash_mtd[0], ecp250_partitions, ARRAY_SIZE(ecp250_partitions));
		return 0;
	} else
		printk("Probing flash device %s failed!\n", ecp250_map[0].name);

	return -ENXIO;
}

module_init(init_ecp250_flash);
module_exit(cleanup_ecp250_flash);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ECP250 Board MTD driver");
MODULE_AUTHOR("Theobroma Systems mico32@theobroma-systems.com");
