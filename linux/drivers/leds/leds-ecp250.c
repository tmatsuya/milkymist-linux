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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <asm/setup.h>


/* on the ecp250 there are 8 leds */
#define NR_LEDS	8

/*
 * Our context
 */
struct ecp250leds_context {
	struct led_classdev	cdev;
	unsigned long bitmask;
	char ledname[14]; // "leds-ecp250_X"
};

static void ecp250leds_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	struct ecp250leds_context *led_dev =
		container_of(led_cdev, struct ecp250leds_context, cdev);
	volatile unsigned long* port = (volatile unsigned long*)lm32tag_leds[0]->addr;

	if (value)
		*port |= led_dev->bitmask;
	else
		*port &= ~(led_dev->bitmask);
}

static struct ecp250leds_context ecp250leds_leds[NR_LEDS];

static int ecp250leds_probe(struct platform_device *pdev)
{
	volatile unsigned long* port = (volatile unsigned long*)lm32tag_leds[0]->addr;
	int i;
	int ret;

	/* initialize to zero */
	*port = 0x0;

	for (i = 0, ret = 0; ret >= 0 && i < ARRAY_SIZE(ecp250leds_leds); i++) {
		ret = led_classdev_register(&pdev->dev,
				&ecp250leds_leds[i].cdev);
	}

	if (ret < 0 && i > 1) {
		for (i = i - 2; i >= 0; i--)
			led_classdev_unregister(&ecp250leds_leds[i].cdev);
	}

	return ret;
}

static int ecp250leds_remove(struct platform_device *pdev)
{
	int i;

	for (i = ARRAY_SIZE(ecp250leds_leds) - 1; i >= 0; i--)
		led_classdev_unregister(&ecp250leds_leds[i].cdev);

	return 0;
}

static struct platform_driver ecp250leds_driver = {
	.probe		= ecp250leds_probe,
	.remove		= ecp250leds_remove,
	.driver		= {
		.name = "leds-ecp250",
	},
};

static int __init ecp250leds_init(void)
{
	int i;

	printk("leds-ecp250: Led driver initialising\n");
	for (i = ARRAY_SIZE(ecp250leds_leds) - 1; i >= 0; i--) {
		ecp250leds_leds[i].bitmask = (1 << i);
		// build unique name
		memcpy(ecp250leds_leds[i].ledname, "leds-ecp250_X", 14);
		ecp250leds_leds[i].ledname[12] = '0' + i;
		ecp250leds_leds[i].cdev.name = &ecp250leds_leds[i].ledname[0];
		ecp250leds_leds[i].cdev.brightness_set = ecp250leds_set;
	}

	return platform_driver_register(&ecp250leds_driver);
}

static void __exit ecp250leds_exit(void)
{
	return platform_driver_unregister(&ecp250leds_driver);
}

module_init(ecp250leds_init);
module_exit(ecp250leds_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LED driver support for Lattice ECP250 eval board (Mico 32)");
MODULE_AUTHOR("Theobroma Systems mico32@theobroma-systems.com");
