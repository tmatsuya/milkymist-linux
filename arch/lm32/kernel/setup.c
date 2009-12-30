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
 * Partially based on
 *
 * linux/arch/m68knommu/kernel/setup.c
 */

/*
 * This file handles the architecture-dependent parts of system setup
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/genhd.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/major.h>
#include <linux/initrd.h>
#include <linux/bootmem.h>
#include <linux/seq_file.h>
#include <linux/root_dev.h>
#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/thread_info.h>

/* this is set first thing as the kernel is started
 * from the arguments to the kernel. */
unsigned long asmlinkage _kernel_arg_cmdline; /* address of the commandline parameters */
unsigned long asmlinkage _kernel_arg_initrd_start;
unsigned long asmlinkage _kernel_arg_initrd_end;

char __initdata command_line[COMMAND_LINE_SIZE];


/* from mm/init.c */
extern void bootmem_init(void);
extern void paging_init(void);

unsigned int cpu_frequency;
unsigned int sdram_start;
unsigned int sdram_size;

unsigned int lm32tag_num_uart = 1;

void __init setup_arch(char **cmdline_p)
{
	/*
	 * init "current thread structure" pointer
	 */
	lm32_current_thread = (struct thread_info*)&init_thread_union;

	cpu_frequency = (unsigned long)CONFIG_CPU_CLOCK;
	sdram_start = (unsigned long)CONFIG_MEMORY_START;
	sdram_size = (unsigned long)CONFIG_MEMORY_SIZE;

	/* Keep a copy of command line */
	*cmdline_p = (char*)_kernel_arg_cmdline;


#if defined(CONFIG_BOOTPARAM)
	/* CONFIG_CMDLINE should override all */
	strncpy(*cmdline_p, CONFIG_BOOTPARAM_STRING, COMMAND_LINE_SIZE);
#endif

	memcpy(boot_command_line, *cmdline_p, COMMAND_LINE_SIZE);
	boot_command_line[COMMAND_LINE_SIZE-1] = 0;

#ifdef CONFIG_DUMMY_CONSOLE
        conswitchp = &dummy_con;
#endif

#ifdef CONFIG_EARLY_PRINTK
	{
		extern void setup_early_printk(void);

		setup_early_printk();
	}
#endif

	/*
	 * Init boot memory
	 */
	bootmem_init();

	/*
	 * Get kmalloc into gear.
	 */
	paging_init();
}

/*
 *	Get CPU information for use by the procfs.
 */

static int show_cpuinfo(struct seq_file *m, void *v)
{
    char *cpu, *mmu, *fpu;
    u_long clockfreq;

    cpu = "lm32";
    mmu = "none";
    fpu = "none";

    clockfreq = loops_per_jiffy*HZ;

    seq_printf(m, "CPU:\t\t%s\n"
		   "MMU:\t\t%s\n"
		   "FPU:\t\t%s\n"
		   "Clocking:\t%lu.%1luMHz\n"
		   "BogoMips:\t%lu.%02lu\n"
		   "Calibration:\t%lu loops\n",
		   cpu, mmu, fpu,
		   clockfreq/1000000,(clockfreq/100000)%10,
		   (loops_per_jiffy*HZ)/500000,((loops_per_jiffy*HZ)/5000)%100,
		   (loops_per_jiffy*HZ));

	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < NR_CPUS ? ((void *) 0x12345678) : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

struct seq_operations cpuinfo_op = {
	.start	= c_start,
	.next	= c_next,
	.stop	= c_stop,
	.show	= show_cpuinfo,
};

static struct resource milkymistuart_resources[] = {
	[0] = {
		.start = 0x80000000,
		.end = 0x8000000f,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_UARTRX,
		.end = IRQ_UARTTX,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device milkymistuart_device = {
	.name = "milkymist_uart",
	.id = 0,
	.num_resources = ARRAY_SIZE(milkymistuart_resources),
	.resource = milkymistuart_resources,
};

#ifdef CONFIG_BOARD_XILINX_ML401
static struct resource lm32sysace_resources[] = {
	[0] = {
		.start = 0xa0000000,
		.end = 0xa00000ff,
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device lm32sysace_device = {
	.name = "xsysace",
	.id = 0,
	.num_resources = ARRAY_SIZE(lm32sysace_resources),
	.resource = lm32sysace_resources,
};
#endif

#if defined(CONFIG_BOARD_XILINX_ML401) && defined(CONFIG_SERIO_MILKBD)
static struct resource lm32milkbd_resources[] = {
	[0] = {
		.start = 0x80007000,
		.end = 0x80007000,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_KEYBOARD,
		.end = IRQ_KEYBOARD,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device lm32milkbd_device = {
	.name = "milkbd",
	.id = 0,
	.num_resources = ARRAY_SIZE(lm32milkbd_resources),
	.resource = lm32milkbd_resources,
};
#endif

#if defined(CONFIG_BOARD_XILINX_ML401) && defined(CONFIG_SERIO_MILKMOUSE)
static struct resource lm32milkmouse_resources[] = {
	[0] = {
		.start = 0x80008000,
		.end = 0x80008000,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_MOUSE,
		.end = IRQ_MOUSE,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device lm32milkmouse_device = {
	.name = "milkmouse",
	.id = 0,
	.num_resources = ARRAY_SIZE(lm32milkmouse_resources),
	.resource = lm32milkmouse_resources,
};
#endif

/* setup all devices we find in the hardware setup information */
/* setup all devices we find in the hardware setup information */
static int __init setup_devices(void) {
	int ret = 0;
	int err;

	err = platform_device_register(&milkymistuart_device);
	if( err ) {
		printk(KERN_ERR "could not register 'milkymist_uart'error:%d\n", err);
		ret = err;
	}

#ifdef CONFIG_BOARD_XILINX_ML401
	err = platform_device_register(&lm32sysace_device);
	if( err ) {
		printk(KERN_ERR "could not register 'milkymist_sysace'error:%d\n", err);
		ret = err;
	}
#endif

#if defined(CONFIG_BOARD_XILINX_ML401) && defined(CONFIG_SERIO_MILKBD)
	err = platform_device_register(&lm32milkbd_device);
	if( err ) {
		printk(KERN_ERR "could not register 'milkymist_ps2kbd'error:%d\n", err);
		ret = err;
	}
#endif

#if defined(CONFIG_BOARD_XILINX_ML401) && defined(CONFIG_SERIO_MILKMOUSE)
	err = platform_device_register(&lm32milkmouse_device);
	if( err ) {
		printk(KERN_ERR "could not register 'milkymist_ps2mouse'error:%d\n", err);
		ret = err;
	}
#endif

	return ret;
}
/* default console - interface to milkymistuart.c serial + console driver */
struct platform_device* milkymistuart_default_console_device = &milkymistuart_device;

arch_initcall(setup_devices);

