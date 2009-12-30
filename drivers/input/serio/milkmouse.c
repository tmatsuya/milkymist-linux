/*
 * milkmouse.c
 */

/*
 * Milkymist PS/2 mouse connector driver for Linux
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Simunkova 1594, Prague 8, 182 00 Czech Republic
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <asm/irq.h>

MODULE_AUTHOR("");
MODULE_DESCRIPTION("Milkymist PS/2 mouse connector driver");
MODULE_LICENSE("GPL");

/*
 * Register numbers.
 */
#define	PS2_DATA_REG	0x80008000
#define PS2_STATUS_REG	0x80008004
#define PS2_TX_BUSY	0x01

static int milkmouse_write(struct serio *port, unsigned char val)
{
	while(readl(PS2_STATUS_REG)&PS2_TX_BUSY);

	writel(val, PS2_DATA_REG);

	return 0;
}

static irqreturn_t milkmouse_rx(int irq, void *dev_id)
{
	struct serio *port = dev_id;
	unsigned int byte;
	int handled = IRQ_NONE;

	byte = readl(PS2_DATA_REG);
	serio_interrupt(port, byte, 0);
	handled = IRQ_HANDLED;

	return handled;
}

static int milkmouse_open(struct serio *port)
{

	if (request_irq(IRQ_MOUSE, milkmouse_rx, 0, "milkmouse", port) != 0) {
		printk(KERN_ERR "milkmouse.c: Could not allocate mouse receive IRQ\n");
		return -EBUSY;
	}

	lm32_irq_unmask(IRQ_MOUSE);

	printk(KERN_INFO "milkymist_ps2: Mouse connector at 0x%08x irq %d\n",
		PS2_DATA_REG,
		IRQ_MOUSE);

	return 0;
}

static void milkmouse_close(struct serio *port)
{
	free_irq(IRQ_MOUSE, port);
}

/*
 * Allocate and initialize serio structure for subsequent registration
 * with serio core.
 */
static int __devinit milkmouse_probe(struct platform_device *dev)
{
	struct serio *serio;

	serio = kzalloc(sizeof(struct serio), GFP_KERNEL);
	if (!serio)
		return -ENOMEM;

	serio->id.type		= SERIO_8042;
	serio->write		= milkmouse_write;
	serio->open		= milkmouse_open;
	serio->close		= milkmouse_close;
	serio->dev.parent	= &dev->dev;
	strlcpy(serio->name, "Milkymist PS/2 Mouse connector", sizeof(serio->name));
	strlcpy(serio->phys, "milkymist/ps2mouse", sizeof(serio->phys));

	platform_set_drvdata(dev, serio);
	serio_register_port(serio);
	return 0;
}

static int __devexit milkmouse_remove(struct platform_device *dev)
{
	struct serio *serio = platform_get_drvdata(dev);
	serio_unregister_port(serio);
	return 0;
}

static struct platform_driver milkmouse_driver = {
	.probe		= milkmouse_probe,
	.remove		= __devexit_p(milkmouse_remove),
	.driver		= {
	.name		= "milkmouse",
	},
};

static int __init milkmouse_init(void)
{
	return platform_driver_register(&milkmouse_driver);
}

static void __exit milkmouse_exit(void)
{
	platform_driver_unregister(&milkmouse_driver);
}

module_init(milkmouse_init);
module_exit(milkmouse_exit);
