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

#include <asm-lm32/hwsetup_kernel.h>

/* this is set first thing as the kernel is started
 * from the arguments to the kernel. */
unsigned long asmlinkage _kernel_arg_hwsetup; /* address of the hwsetup parameters */
unsigned long asmlinkage _kernel_arg_cmdline; /* address of the commandline parameters */
unsigned long asmlinkage _kernel_arg_initrd_start;
unsigned long asmlinkage _kernel_arg_initrd_end;

char __initdata command_line[COMMAND_LINE_SIZE];

/*
 * Each line creates:
 * unsigned long lm32tag_num_##name = 0;
 * LM32TAG_##type##_t* lm32tag_##name[LM32MAX_HWSETUP_ITEMS];
 */
#define DECLARE_CONFIG_ITEM(type,name) \
	unsigned long lm32tag_num_##name = 0; \
	LM32Tag_##type##_t* lm32tag_##name[LM32MAX_HWSETUP_ITEMS];

DECLARE_CONFIG_ITEM(CPU, cpu)
DECLARE_CONFIG_ITEM(ASRAM, asram)
DECLARE_CONFIG_ITEM(Flash, flash)
DECLARE_CONFIG_ITEM(SDRAM, sdram)
DECLARE_CONFIG_ITEM(OCM, ocm)
DECLARE_CONFIG_ITEM(DDR_SDRAM, ddr_sdram)
DECLARE_CONFIG_ITEM(DDR2_SDRAM, ddr2_sdram)
DECLARE_CONFIG_ITEM(Timer, timer)
DECLARE_CONFIG_ITEM(UART, uart)
DECLARE_CONFIG_ITEM(GPIO, gpio)
DECLARE_CONFIG_ITEM(TriSpeedMAC, trispeedmac)
DECLARE_CONFIG_ITEM(I2CM, i2cm)
DECLARE_CONFIG_ITEM(LEDS, leds)
DECLARE_CONFIG_ITEM(7SEG, 7seg)

#define PARSE_HWSETUP(tagconst, pointers, number) \
	number = 0; \
	hdr = (LM32Tag_Header_t*)_kernel_arg_hwsetup; \
	while( hdr->tag != LM32TAG_EOL ) { \
		if( hdr->tag == tagconst ) { \
			pointers[number] = (typeof(pointers[number]))((unsigned long)hdr + sizeof(LM32Tag_Header_t)); \
			number++; \
		} \
		hdr = (LM32Tag_Header_t*)((unsigned long)hdr + hdr->size); \
	} \

/* from mm/init.c */
extern void bootmem_init(void);
extern void paging_init(void);

static int __init setup_devices(void);

void __init setup_arch(char **cmdline_p)
{
	LM32Tag_Header_t* hdr; /* for parsing hardware setup */

	/* parse hardware setup */
	PARSE_HWSETUP(LM32TAG_CPU, lm32tag_cpu, lm32tag_num_cpu);
	PARSE_HWSETUP(LM32TAG_ASRAM, lm32tag_asram, lm32tag_num_asram);
	PARSE_HWSETUP(LM32TAG_FLASH, lm32tag_flash, lm32tag_num_flash);
	PARSE_HWSETUP(LM32TAG_SDRAM, lm32tag_sdram, lm32tag_num_sdram);
	PARSE_HWSETUP(LM32TAG_OCM, lm32tag_ocm, lm32tag_num_ocm);
	PARSE_HWSETUP(LM32TAG_DDR_SDRAM, lm32tag_ddr_sdram, lm32tag_num_ddr_sdram);
	PARSE_HWSETUP(LM32TAG_DDR2_SDRAM, lm32tag_ddr2_sdram, lm32tag_num_ddr2_sdram);
	PARSE_HWSETUP(LM32TAG_TIMER, lm32tag_timer, lm32tag_num_timer);
	PARSE_HWSETUP(LM32TAG_UART, lm32tag_uart, lm32tag_num_uart);
	PARSE_HWSETUP(LM32TAG_GPIO, lm32tag_gpio, lm32tag_num_gpio);
	PARSE_HWSETUP(LM32TAG_TRISPEEDMAC, lm32tag_trispeedmac, lm32tag_num_trispeedmac);
	PARSE_HWSETUP(LM32TAG_I2CM, lm32tag_i2cm, lm32tag_num_i2cm);
	PARSE_HWSETUP(LM32TAG_LEDS, lm32tag_leds, lm32tag_num_leds);
	PARSE_HWSETUP(LM32TAG_7SEG, lm32tag_7seg, lm32tag_num_7seg);

	/*
	 * init "current thread structure" pointer
	 */
	lm32_current_thread = (struct thread_info*)&init_thread_union;

#ifdef CONFIG_EARLY_PRINTK
	{
		extern void setup_early_printk(void);

		setup_early_printk();
	}
#endif

	/* Keep a copy of command line */
	*cmdline_p = (char*)_kernel_arg_cmdline;
	memcpy(boot_command_line, *cmdline_p, COMMAND_LINE_SIZE);
	boot_command_line[COMMAND_LINE_SIZE-1] = 0;

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

/* lm32mac resources and devices */
static struct resource lm32mac_resources[LM32MAX_HWSETUP_ITEMS][2];
static struct platform_device lm32mac_devices[LM32MAX_HWSETUP_ITEMS] = {
	[0 ... LM32MAX_HWSETUP_ITEMS-1] = {
		.name = "lm32mac"
	}
};

/* lm32uart resources and devices */
static struct resource lm32uart_resources[LM32MAX_HWSETUP_ITEMS][2];
static struct platform_device lm32uart_devices[LM32MAX_HWSETUP_ITEMS] = {
	[0 ... LM32MAX_HWSETUP_ITEMS-1] = {
		.name = "lm32uart"
	}
};

/* leds-ecp250 resources and devices */
static struct resource ecp250leds_resources[LM32MAX_HWSETUP_ITEMS][1];
static struct platform_device ecp250leds_devices[LM32MAX_HWSETUP_ITEMS] = {
	[0 ... LM32MAX_HWSETUP_ITEMS-1] = {
		.name = "leds-ecp250"
	}
};

/* default console - interface to lm32uart.c serial + console driver */
struct platform_device* lm32uart_default_console_device = &lm32uart_devices[0];

/* setup all devices we find in the hardware setup information */
static int __init setup_devices(void) {
	int ret = 0;
	int i, err;

	/* lm32mac */
	for(i = 0; i < lm32tag_num_trispeedmac; ++i) {
		/* setup resources */
		lm32mac_resources[i][0].start = lm32tag_trispeedmac[i]->addr;
		lm32mac_resources[i][0].end =
			lm32tag_trispeedmac[i]->addr + 1024;
		/* TODO: put size element for lm32mac into hardware setup structures */
		lm32mac_resources[i][0].flags = IORESOURCE_MEM;
		lm32mac_resources[i][1].start = lm32tag_trispeedmac[i]->irq;
		lm32mac_resources[i][1].end = lm32tag_trispeedmac[i]->irq;
		lm32mac_resources[i][1].flags = IORESOURCE_IRQ;
		/* setup device */
		lm32mac_devices[i].id = i;
		lm32mac_devices[i].num_resources = 2;
		lm32mac_devices[i].resource = lm32mac_resources[i];

		/* register device */
		err = platform_device_register(&lm32mac_devices[i]);
		if( err ) {
			printk(KERN_ERR "could not register 'lm32mac' id:%d error:%d\n", i, err);
			ret = err;
		}
	}

	/* lm32uart */
	for(i = 0; i < lm32tag_num_uart; ++i) {
		/* setup resources */
		lm32uart_resources[i][0].start = lm32tag_uart[i]->addr;
		lm32uart_resources[i][0].end = lm32tag_uart[i]->addr + 8;
		/* TODO: put size element for lm32uart into hardware setup structures */
		lm32uart_resources[i][0].flags = IORESOURCE_MEM;
		lm32uart_resources[i][1].start = lm32tag_uart[i]->irq;
		lm32uart_resources[i][1].end = lm32tag_uart[i]->irq;
		lm32uart_resources[i][1].flags = IORESOURCE_IRQ;
		/* setup device */
		lm32uart_devices[i].id = i;
		lm32uart_devices[i].num_resources = 2;
		lm32uart_devices[i].resource = lm32uart_resources[i];

		/* register device */
		err = platform_device_register(&lm32uart_devices[i]);
		if( err ) {
			printk(KERN_ERR "could not register 'lm32uart' id:%d error:%d\n", i, err);
			ret = err;
		}
	}

	/* ecp250leds */
	for(i = 0; i < lm32tag_num_leds; ++i) {
		/* setup resources */
		ecp250leds_resources[i][0].start = lm32tag_leds[i]->addr;
		ecp250leds_resources[i][0].end = lm32tag_leds[i]->addr + 4;
		/* TODO: put size element for lm32uart into hardware setup structures */
		ecp250leds_resources[i][0].flags = IORESOURCE_MEM;
		/* setup device */
		ecp250leds_devices[i].id = i;
		ecp250leds_devices[i].num_resources = 1;
		ecp250leds_devices[i].resource = ecp250leds_resources[i];

		/* register device */
		err = platform_device_register(&ecp250leds_devices[i]);
		if( err ) {
			printk(KERN_ERR "could not register 'ecp250leds' id:%d error:%d\n", i, err);
			ret = err;
		}
	}

	return ret;
}

arch_initcall(setup_devices);
