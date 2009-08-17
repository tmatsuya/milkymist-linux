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

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <asm/setup.h>

#define DRVNAME "lm32_gpio"

/* LM32 GPIO device register structure */
typedef struct lm32_gpio_regs {
	volatile unsigned long pio_data;
	volatile unsigned long pio_tri;
	volatile unsigned long irq_mask;
	volatile unsigned long edge_capture;
} lm32_gpio_regs_t;

typedef struct lm32_gpio_priv {
	/* pointer to registers */
	struct lm32_gpio_regs* regs;
	/* which device in lm32tag_gpio is this */
	u8 idx;
	/* which bit on this device is this */
	u8 bit;
	/* if this bit is configured as input, output or tristate (LM32_GPIO_DIR_...) */
	u8 dir;
	/* remember last output to this bit */
	u8 lastout;
	/* pointer to struct device* */
	struct device* dev;
} lm32_gpio_priv_t;

enum {
	LM32_GPIO_DIR_INPUT = 0,
	LM32_GPIO_DIR_OUTPUT = 1,
	LM32_GPIO_DIR_TRISTATE = 2,
};

static unsigned totalbits;
/* pointer to one priv structure for each bit = each minor number */
static struct lm32_gpio_priv* bit_privs = NULL;
static struct platform_device* pdev = NULL;
static struct cdev cdev;
/* default to dynamic major number */
static int major = 0;

ssize_t lm32_gpio_write(struct file *file, const char __user *data,
		       size_t len, loff_t *ppos);
ssize_t lm32_gpio_read(struct file *file, char __user * buf,
		size_t len, loff_t * ppos);
static int lm32_gpio_open(struct inode *inode, struct file *file);
static int lm32_gpio_release(struct inode *inode, struct file *file);

static const struct file_operations lm32_gpio_fileops = {
	.owner   = THIS_MODULE,
	.write   = lm32_gpio_write,
	.read    = lm32_gpio_read,
	.open    = lm32_gpio_open,
	.release = lm32_gpio_release
};

ssize_t lm32_gpio_write(struct file *file, const char __user *data,
		       size_t len, loff_t *ppos)
{
	lm32_gpio_priv_t* priv = (lm32_gpio_priv_t*)file->private_data;
	unsigned long bitmask = (1 << priv->bit);
	size_t i;
	int err = 0;

	for (i = 0; i < len; ++i) {
		char c;
		if (get_user(c, data + i))
			err = -EFAULT;
		switch (c) {
		case '0':
			if( priv->dir == LM32_GPIO_DIR_INPUT ) {
				err = -EIO;
				break;
			}
			/* unset bit */
			priv->regs->pio_data &= ~bitmask;
			break;
		case '1':
			if( priv->dir == LM32_GPIO_DIR_INPUT ) {
				err = -EIO;
				break;
			}
			/* set bit */
			priv->regs->pio_data |= bitmask;
			break;
		case 'T':
			if( priv->dir != LM32_GPIO_DIR_TRISTATE ) {
				err = -EIO;
				break;
			}
			/* set tristate */
			priv->regs->pio_tri |= bitmask;
			break;
		case 't':
			if( priv->dir != LM32_GPIO_DIR_TRISTATE ) {
				err = -EIO;
				break;
			}
			/* unset tristate */
			priv->regs->pio_tri &= ~bitmask;
			break;
		case '\r':
		case '\n':
			/* end of settings string, do nothing */
			break;
		default:
			err = -EINVAL;
			break;
		}
	}
	if( err )
		return err;
	else
		return len;
}

ssize_t lm32_gpio_read(struct file *file, char __user * buf,
		size_t len, loff_t * ppos)
{
	lm32_gpio_priv_t* priv = (lm32_gpio_priv_t*)file->private_data;
	unsigned long bitmask;
	unsigned long state;
	char chr;

	if( priv->dir == LM32_GPIO_DIR_OUTPUT ) {
		/* output last written state */
		state = priv->lastout;
	} else {
		/* output retrieved state */
		bitmask = (1 << priv->bit);
		state = priv->regs->pio_data & bitmask;
	}
	chr = state?'1':'0';

	if( put_user(chr, buf) )
		return -EFAULT;

	return 1;
}

static int lm32_gpio_open(struct inode *inode, struct file *file)
{
	unsigned m = iminor(inode);

	if (m >= totalbits)
		return -EINVAL;

	/* link in private bit info for this minor number = for this bit */
	file->private_data = &(bit_privs[m]);

	return nonseekable_open(inode, file);
}

static int lm32_gpio_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int __init lm32_gpio_init(void)
{
	unsigned int i, j;
	unsigned int atBit;
	int rc;
	dev_t devid;

	printk("lm32_gpio: initializing\n");
	if( lm32tag_num_gpio == 0 ) {
		printk(DRVNAME ": no GPIO pins present\n");
		return -ENODEV;
	}

	/* calculate total bits */
	totalbits = 0;
	for(i = 0; i < lm32tag_num_gpio; ++i)
		totalbits += lm32tag_gpio[i]->width_input + lm32tag_gpio[i]->width_output + lm32tag_gpio[i]->width_data;
	/* initialize bit_privs:
	 * for each bit in all controllers one struct is created,
	 * each struct contains information to which controller it belongs to */
	bit_privs = (struct lm32_gpio_priv*)kzalloc(
			sizeof(struct lm32_gpio_priv)*totalbits, GFP_KERNEL);
	if( !bit_privs )
		return -ENOMEM;

	atBit = 0;
	for(i = 0; i < lm32tag_num_gpio; ++i) {
		switch(lm32tag_gpio[i]->port_types) {
			case LM32TAG_GPIO_PORT_OUTPUT:
				/* output bits */
				for(j = 0; j < lm32tag_gpio[i]->width_data; ++j) {
					/* initialize bit atBit */
					bit_privs[atBit].regs = (struct lm32_gpio_regs*)lm32tag_gpio[i]->addr;
					bit_privs[atBit].idx = i;
					bit_privs[atBit].bit = j;
					bit_privs[atBit].dir = LM32_GPIO_DIR_OUTPUT;
					bit_privs[atBit].dev = &pdev->dev;
					atBit++;
				}
				break;
			case LM32TAG_GPIO_PORT_INPUT:
				/* input bits */
				for(j = 0; j < lm32tag_gpio[i]->width_data; ++j) {
					/* initialize bit atBit */
					bit_privs[atBit].regs = (struct lm32_gpio_regs*)lm32tag_gpio[i]->addr;
					bit_privs[atBit].idx = i;
					bit_privs[atBit].bit = j;
					bit_privs[atBit].dir = LM32_GPIO_DIR_INPUT;
					bit_privs[atBit].dev = &pdev->dev;
					atBit++;
				}
				break;
			case LM32TAG_GPIO_PORT_TRISTATE:
				/* tristate bits */
				for(j = 0; j < lm32tag_gpio[i]->width_data; ++j) {
					/* initialize bit atBit */
					bit_privs[atBit].regs = (struct lm32_gpio_regs*)lm32tag_gpio[i]->addr;
					bit_privs[atBit].idx = i;
					bit_privs[atBit].bit = j;
					bit_privs[atBit].dir = LM32_GPIO_DIR_TRISTATE;
					bit_privs[atBit].dev = &pdev->dev;
					atBit++;
				}
				break;
			case LM32TAG_GPIO_PORT_INPUTOUTPUT:
				/* simultaneous input and output bits (programmed to different connectors) */
				/* input bits */
				for(j = 0; j < lm32tag_gpio[i]->width_input; ++j) {
					/* initialize bit atBit */
					bit_privs[atBit].regs = (struct lm32_gpio_regs*)lm32tag_gpio[i]->addr;
					bit_privs[atBit].idx = i;
					bit_privs[atBit].bit = j;
					bit_privs[atBit].dir = LM32_GPIO_DIR_INPUT;
					bit_privs[atBit].dev = &pdev->dev;
					atBit++;
				}
				/* output bits */
				for(j = 0; j < lm32tag_gpio[i]->width_output; ++j) {
					/* initialize bit atBit */
					bit_privs[atBit].regs = (struct lm32_gpio_regs*)lm32tag_gpio[i]->addr;
					bit_privs[atBit].idx = i;
					bit_privs[atBit].bit = j;
					bit_privs[atBit].dir = LM32_GPIO_DIR_OUTPUT;
					bit_privs[atBit].dev = &pdev->dev;
					atBit++;
				}
				break;
		}
	}

	/* support dev_dbg() with pdev->dev */
	pdev = platform_device_alloc(DRVNAME, 0);
	if (!pdev)
		return -ENOMEM;

	rc = platform_device_add(pdev);
	if (rc) {
		platform_device_put(pdev);
		return rc;
	}

	if (major) {
		devid = MKDEV(major, 0);
		rc = register_chrdev_region(devid, totalbits, DRVNAME);
	} else {
		rc = alloc_chrdev_region(&devid, 0, totalbits, DRVNAME);
		major = MAJOR(devid);
	}
	if (rc < 0) {
		printk(DRVNAME ": chrdev_region err: %d\n", rc);
		platform_device_del(pdev);
		platform_device_put(pdev);
		return rc;
	}

	cdev_init(&cdev, &lm32_gpio_fileops);
	cdev_add(&cdev, devid, totalbits);

	return 0;
}

static void __exit lm32_gpio_cleanup(void)
{
	cdev_del(&cdev);
	unregister_chrdev_region(MKDEV(major, 0), totalbits);
	platform_device_unregister(pdev);
	kfree(bit_privs);
}

module_init(lm32_gpio_init);
module_exit(lm32_gpio_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Lattice Mico 32 GPIO driver");
MODULE_AUTHOR("Theobroma Systems mico32@theobroma-systems.com");
