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

#define DRVNAME "ecp250_7seg"

/* LM32 GPIO device register structure */
struct lm32_gpio_regs {
	volatile unsigned long pio_data;
	volatile unsigned long pio_tri;
	volatile unsigned long irq_mask;
	volatile unsigned long edge_capture;
};

#define SEG_A	(0x01)
#define SEG_B	(0x02)
#define SEG_C	(0x04)
#define SEG_D	(0x08)
#define SEG_E	(0x10)
#define SEG_F	(0x20)
#define SEG_G	(0x40)
#define SEG_DOT	(0x80)

#define SEGMENT_A	(0x100)
#define SEGMENT_B	(0x200)

struct lm32_7seg_priv {
	/* pointer to registers */
	struct lm32_gpio_regs* regs;
	/* which device in lm32tag_7seg is this */
	u8 idx;
	/* remember currently displayed data */
	u8 left;
	u8 left_dot;
	u8 right;
	u8 right_dot;
	/* padding */
	u8 reserved0;
	u8 reserved1;
	u8 reserved2;
};

/* pointer to one priv structure for each display = each minor number */
static struct lm32_7seg_priv* privs = NULL;
static u8* charmap = NULL;
static struct platform_device* pdev = NULL;
static struct cdev cdev;
/* default to dynamic major number */
static int major = 0;

ssize_t lm32_7seg_write(struct file *file, const char __user *data,
		       size_t len, loff_t *ppos);
static int lm32_7seg_open(struct inode *inode, struct file *file);
static int lm32_7seg_release(struct inode *inode, struct file *file);

static const struct file_operations lm32_7seg_fileops = {
	.owner   = THIS_MODULE,
	.write   = lm32_7seg_write,
	.open    = lm32_7seg_open,
	.release = lm32_7seg_release
};

ssize_t lm32_7seg_write(struct file *file, const char __user *data,
		       size_t len, loff_t *ppos)
{
	struct lm32_7seg_priv* priv = (struct lm32_7seg_priv*)file->private_data;
	size_t i;
	unsigned long out;
	int err = 0;

	for (i = 0; i < len; ++i) {
		u8 c;
		u8 mapped_char;
		/* get char from user */
		if (get_user(c, data + i))
			err = -EFAULT;
		mapped_char = charmap[c];
		/* "shift left" all four displayed data elements */
		priv->left = priv->left_dot;
		priv->left_dot = priv->right;
		priv->right = priv->right_dot;
		priv->right_dot = mapped_char;
	}

	/* if left char contains data, set left char, else set right char */
	if( (priv->left | priv->left_dot) != 0 )
		/* program segment B */
		out = SEGMENT_B | priv->left | priv->left_dot;
	else
		/* program segment A */
		out = SEGMENT_A | priv->right | priv->right_dot;
	
	/* set output */
	priv->regs->pio_data = ~out;

	if( err )
		return err;
	else
		return len;
}

static int lm32_7seg_open(struct inode *inode, struct file *file)
{
	unsigned m = iminor(inode);

	if (m >= lm32tag_num_7seg)
		return -EINVAL;

	/* link in private bit info for this minor number = for this bit */
	file->private_data = &(privs[m]);

	return nonseekable_open(inode, file);
}

static int lm32_7seg_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int __init lm32_7seg_init(void)
{
	unsigned int i;
	int rc;
	dev_t devid;

	printk("ecp250_7seg: 7 Segment driver initializing\n");

	if( lm32tag_num_7seg == 0 ) {
		printk(KERN_ERR DRVNAME ": no displays present\n");
		return -ENODEV;
	}

	/* allocate */
	privs = (struct lm32_7seg_priv*)kzalloc(
			sizeof(struct lm32_7seg_priv)*lm32tag_num_7seg, GFP_KERNEL);
	charmap = (u8*)kzalloc(
			sizeof(u8)*256, GFP_KERNEL);
	if( !privs || !charmap ) {
		if( privs )
			kfree(privs);
		if( charmap )
			kfree(charmap);
		return -ENOMEM;
	}

	/* initialize charmap */
	charmap['.'] = SEG_DOT;
	charmap['0'] = (SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F);
	charmap['1'] = (SEG_B | SEG_C);
	charmap['2'] = (SEG_A | SEG_B | SEG_D | SEG_E | SEG_G);
	charmap['3'] = (SEG_A | SEG_B | SEG_C | SEG_D | SEG_G);
	charmap['4'] = (SEG_B | SEG_C | SEG_F | SEG_G);
	charmap['5'] = (SEG_A | SEG_C | SEG_D | SEG_F | SEG_G);
	charmap['6'] = (SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G);
	charmap['7'] = (SEG_A | SEG_B | SEG_C);
	charmap['8'] = (SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G);
	charmap['9'] = (SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G);

	/* initialize privs */
	for(i = 0; i < lm32tag_num_7seg; ++i) {
		privs[i].regs = (struct lm32_gpio_regs*)lm32tag_7seg[i]->addr;
		privs[i].idx = i;
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
		rc = register_chrdev_region(devid, lm32tag_num_7seg, DRVNAME);
	} else {
		rc = alloc_chrdev_region(&devid, 0, lm32tag_num_7seg, DRVNAME);
		major = MAJOR(devid);
	}
	if (rc < 0) {
		dev_err(&pdev->dev, DRVNAME ": chrdev_region err: %d\n", rc);
		platform_device_del(pdev);
		platform_device_put(pdev);
		return rc;
	}

	cdev_init(&cdev, &lm32_7seg_fileops);
	cdev_add(&cdev, devid, lm32tag_num_7seg);

	return 0;
}

static void __exit lm32_7seg_cleanup(void)
{
	cdev_del(&cdev);
	unregister_chrdev_region(MKDEV(major, 0), lm32tag_num_7seg);
	platform_device_unregister(pdev);
	kfree(privs);
	kfree(charmap);
}

module_init(lm32_7seg_init);
module_exit(lm32_7seg_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Lattice Mico 32 ECP250 7 Segment Display driver");
MODULE_AUTHOR("Theobroma Systems mico32@theobroma-systems.com");
