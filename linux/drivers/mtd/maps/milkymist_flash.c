/*
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
 /* Based on ecp250_flash.c */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/setup.h>

static struct mtd_partition milkymist_partitions[] = {
	{
		.name = "Bios",
		.size = 0x00040000, 	/* 256 kB */
		.offset = 0x0000000
	},
	{
		.name = "Linux Kernel",
		.size = 0x00200000, 	/* 2 MB */
		.offset = MTDPART_OFS_APPEND,
	},
	{
		.name = "Rootfs",
		.size = (0x00800000 - 0x40000 - 0x200000), /* rest to play with */
		.offset = MTDPART_OFS_APPEND,
	}
};

static struct map_info milkymist_map[1] = {
	{
		/* .name will be set from lm32tag_flash */
		/* .phys will be set from lm32tag_flash */
		/* .size will be set from lm32tag_flash */
		.bankwidth = 2, /* always 32bit access with Mico32 */
	}
};

static struct mtd_info *milkymist_flash_mtd[1] = {NULL};

static void __exit  cleanup_milkymist_flash(void)
{
	int i;

	for (i=0; i<2; i++) {
		if (milkymist_flash_mtd[i]) {
			del_mtd_partitions(milkymist_flash_mtd[i]);
			map_destroy(milkymist_flash_mtd[i]);
			iounmap(milkymist_map[0].virt);
		}
	}
}

static int __init init_milkymist_flash(void)
{
	printk("milkymist_flash: Flash driver initializing\n");

	milkymist_map[0].name = "milkymist_flash";
	milkymist_map[0].phys = 0x00000000;
	milkymist_map[0].size = 0x800000;
	milkymist_map[0].cached =
	    ioremap(milkymist_map[0].phys, milkymist_map[0].size);

	milkymist_map[0].virt = ioremap(milkymist_map[0].phys, milkymist_map[0].size);

	milkymist_flash_mtd[0] = do_map_probe("map_rom", &milkymist_map[0]);
	if (milkymist_flash_mtd[0]) {
		milkymist_flash_mtd[0]->owner = THIS_MODULE;
		add_mtd_partitions(milkymist_flash_mtd[0], milkymist_partitions, ARRAY_SIZE(milkymist_partitions));
		return 0;
	} else
		printk("Probing flash device %s failed!\n", milkymist_map[0].name);

	return -ENXIO;
}

module_init(init_milkymist_flash);
module_exit(cleanup_milkymist_flash);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Milkymist MTD Mapping driver");
MODULE_AUTHOR("Clark Xin, codinflu@gmail.com");
