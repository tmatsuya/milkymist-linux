/*
 * (C) Copyright 2009 Sebastien Bourdeauducq
 * (C) Copyright 2007 Theobroma Systems <www.theobroma-systems.com>
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
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/setup.h>

#define MMPTR(x)		(*((volatile unsigned int *)(x)))

#define CSR_UART_RXTX		MMPTR(0x80000000)
#define CSR_UART_DIVISOR	MMPTR(0x80000004)

#define IRQ_UARTRX		(3)
#define IRQ_UARTTX		(4)

#define MILKYMISTUART_DRIVERNAME "milkymist_uart"
#define MILKYMISTUART_DEVICENAME "ttyS"
#define MILKYMISTUART_MAJOR TTY_MAJOR
#define MILKYMISTUART_MINOR 64


/* these two will be initialized by milkymistuart_init */
static struct uart_port milkymistuart_ports[1];

static struct uart_port* __devinit milkymistuart_init_port(struct platform_device *pdev);

static unsigned int milkymistuart_tx_empty(struct uart_port *port);
static void milkymistuart_set_mctrl(struct uart_port *port, unsigned int mctrl);
static unsigned int milkymistuart_get_mctrl(struct uart_port *port);
static void milkymistuart_start_tx(struct uart_port *port);
static void milkymistuart_stop_tx(struct uart_port *port);
static void milkymistuart_stop_rx(struct uart_port *port);
static void milkymistuart_enable_ms(struct uart_port *port);
static void milkymistuart_break_ctl(struct uart_port *port, int break_state);
static int milkymistuart_startup(struct uart_port *port);
static void milkymistuart_shutdown(struct uart_port *port);
static void milkymistuart_set_termios(struct uart_port *port, struct ktermios *termios, struct ktermios *old);
static const char *milkymistuart_type(struct uart_port *port);
static void milkymistuart_release_port(struct uart_port *port);
static int milkymistuart_request_port(struct uart_port *port);
static void milkymistuart_config_port(struct uart_port *port, int flags);
static int milkymistuart_verify_port(struct uart_port *port, struct serial_struct *ser);

static inline void milkymistuart_set_baud_rate(struct uart_port *port, unsigned long baud);
static irqreturn_t milkymistuart_irq_rx(int irq, void* portarg);
static irqreturn_t milkymistuart_irq_tx(int irq, void* portarg);

static struct uart_ops milkymistuart_pops = {
	.tx_empty	= milkymistuart_tx_empty,
	.set_mctrl	= milkymistuart_set_mctrl,
	.get_mctrl	= milkymistuart_get_mctrl,
	.stop_tx	= milkymistuart_stop_tx,
	.start_tx	= milkymistuart_start_tx,
	.stop_rx	= milkymistuart_stop_rx,
	.enable_ms	= milkymistuart_enable_ms,
	.break_ctl	= milkymistuart_break_ctl,
	.startup	= milkymistuart_startup,
	.shutdown	= milkymistuart_shutdown,
	.set_termios	= milkymistuart_set_termios,
	.type		= milkymistuart_type,
	.release_port	= milkymistuart_release_port,
	.request_port	= milkymistuart_request_port,
	.config_port	= milkymistuart_config_port,
	.verify_port	= milkymistuart_verify_port
};

static inline void milkymistuart_set_baud_rate(struct uart_port *port, unsigned long baud)
{
	CSR_UART_DIVISOR = 100000000 / baud / 16;
}

static void milkymistuart_tx_next_char(struct uart_port* port)
{
	struct circ_buf *xmit = &(port->info->xmit);

	if (port->x_char) {
		/* send xon/xoff character */
		CSR_UART_RXTX = port->x_char;
		port->x_char = 0;
		port->icount.tx++;
		return;
	}

	/* stop transmitting if buffer empty */
	if (uart_circ_empty(xmit) || uart_tx_stopped(port))
		return;
	
	/* send next character */
	CSR_UART_RXTX = xmit->buf[xmit->tail];
	xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
	port->icount.tx++;

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);
}

static void milkymistuart_rx_next_char(struct uart_port* port)
{
	struct tty_struct *tty = port->info->tty;
	unsigned char ch;

	ch = CSR_UART_RXTX & 0xFF;
	port->icount.rx++;
CSR_UART_RXTX = 0x00;

	if (uart_handle_sysrq_char(port, ch))
		goto ignore_char;

	tty_insert_flip_char(tty, ch, TTY_NORMAL);

ignore_char:
	tty_flip_buffer_push(tty);
}

static irqreturn_t milkymistuart_irq_rx(int irq, void* portarg)
{
	struct uart_port* port = (struct uart_port*)portarg;

	milkymistuart_rx_next_char(port);
	
	return IRQ_HANDLED;
}

static irqreturn_t milkymistuart_irq_tx(int irq, void* portarg)
{
	struct uart_port* port = (struct uart_port*)portarg;

	milkymistuart_tx_next_char(port);

	return IRQ_HANDLED;
}

static unsigned int milkymistuart_tx_empty(struct uart_port *port)
{
	return TIOCSER_TEMT;
}

static void milkymistuart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	/* no modem control */
}

static unsigned int milkymistuart_get_mctrl(struct uart_port *port)
{
	/* no modem control */
	return 0;
}

static void milkymistuart_start_tx(struct uart_port *port)
{
//	lm32_irq_unmask(IRQ_UARTTX);
	return 0;
}

static void milkymistuart_stop_tx(struct uart_port *port)
{
//	lm32_irq_mask(IRQ_UARTTX);
	return 0;
}


static void milkymistuart_stop_rx(struct uart_port *port)
{
//	lm32_irq_mask(IRQ_UARTRX);
	return 0;
}

static void milkymistuart_enable_ms(struct uart_port *port)
{
//	lm32_irq_unmask(IRQ_UARTRX);
}

static void milkymistuart_break_ctl(struct uart_port *port, int break_state)
{
	/* TODO */
}

static int milkymistuart_startup(struct uart_port *port)
{
	if( request_irq(IRQ_UARTRX, milkymistuart_irq_rx,
				IRQF_DISABLED, "milkymist_uart RX", port) ) {
		printk(KERN_NOTICE "Unable to attach Milkymist UART RX interrupt\n");
		return -EBUSY;
	}
	if( request_irq(IRQ_UARTTX, milkymistuart_irq_tx,
				IRQF_DISABLED, "milkymist_uart TX", port) ) {
		printk(KERN_NOTICE "Unable to attach Milkymist UART TX interrupt\n");
		return -EBUSY;
	}

	lm32_irq_unmask(IRQ_UARTRX);
	lm32_irq_unmask(IRQ_UARTTX);

	return 0;
}

static void milkymistuart_shutdown(struct uart_port *port)
{
	free_irq(IRQ_UARTRX, port);
	free_irq(IRQ_UARTTX, port);
}

static void milkymistuart_set_termios(
		struct uart_port *port, struct ktermios *termios, struct ktermios *old)
{
	unsigned long baud;
	unsigned long flags;

	/* >> 4 means / 16 */
	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk >> 4);

	/* deactivate irqs */
	spin_lock_irqsave(&port->lock, flags);

	milkymistuart_set_baud_rate(port, baud);

	uart_update_timeout(port, termios->c_cflag, baud);

	/* restore irqs */
	spin_unlock_irqrestore(&port->lock, flags);
}

static const char *milkymistuart_type(struct uart_port *port)
{
	/* check, to be on the safe side */
	if( port->type == PORT_MILKYMISTUART )
		return "milkymist_uart";
	else
		return "error";
}

static void milkymistuart_release_port(struct uart_port *port)
{
}

static int milkymistuart_request_port(struct uart_port *port)
{
	return 0;
}

/* we will only configure the port type here */
static void milkymistuart_config_port(struct uart_port *port, int flags)
{
	if( flags & UART_CONFIG_TYPE ) {
		port->type = PORT_MILKYMISTUART;
	}
}

/* we do not allow the user to configure via this method */
static int milkymistuart_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	return -EINVAL;
}

#ifdef CONFIG_SERIAL_MILKYMIST_CONSOLE
static void milkymist_console_putchar(struct uart_port *port, int ch)
{
	CSR_UART_RXTX = ch;
	while(!(lm32_irq_pending() & (1 << IRQ_UARTTX)));
	lm32_irq_ack(IRQ_UARTTX);
}

/*
 * Interrupts are disabled on entering
 */
static void milkymist_console_write(struct console *co, const char *s, u_int count)
{
	struct uart_port *port = &milkymistuart_ports[co->index];

	uart_console_write(port, s, count, milkymist_console_putchar);
}

static int __init milkymist_console_setup(struct console *co, char *options)
{
	struct uart_port *port = &milkymistuart_ports[co->index];

	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct uart_driver milkymistuart_driver;

static struct console milkymist_console = {
	.name		= MILKYMISTUART_DEVICENAME,
	.write		= milkymist_console_write,
	.device		= uart_console_device,
	.setup		= milkymist_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &milkymistuart_driver,
};

/*
 * Early console initialization
 */
static int __init milkymist_early_console_init(void)
{
	add_preferred_console(MILKYMISTUART_DEVICENAME, milkymistuart_default_console_device->id, NULL);
	milkymistuart_init_port(&milkymistuart_default_console_device);
	register_console(&milkymist_console);
	pr_info("milkymist_uart: registered real console\n");
	return 0;
}
console_initcall(milkymist_early_console_init);

/*
 * Late console initialization
 */
static int __init milkymist_late_console_init(void)
{
	if( !(milkymist_console.flags & CON_ENABLED) ) {
		register_console(&milkymist_console);
		pr_info("milkymist_uart: registered real console\n");
	}
	return 0;
}
core_initcall(milkymist_late_console_init);

#define MILKYMIST_CONSOLE_DEVICE	&milkymist_console
#else
#define MILKYMIST_CONSOLE_DEVICE	NULL
#endif

static struct uart_driver milkymistuart_driver = {
	.owner       = THIS_MODULE,
	.driver_name = MILKYMISTUART_DRIVERNAME,
	.dev_name    = MILKYMISTUART_DEVICENAME,
	.major       = MILKYMISTUART_MAJOR,
	.minor       = MILKYMISTUART_MINOR,
	.nr          = 0, /* will be filled by init */
	.cons        = MILKYMIST_CONSOLE_DEVICE
};

static struct uart_port* __devinit milkymistuart_init_port(struct platform_device *pdev)
{
	struct uart_port* port;
	
	port = &milkymistuart_ports[0];
	port->type = PORT_MILKYMISTUART;
	port->iobase = (void __iomem*)0x80000000;
	port->membase = (void __iomem*)0x80000000;
	port->irq = IRQ_UARTRX;
	port->uartclk = cpu_frequency * 16;
	port->flags = UPF_SKIP_TEST | UPF_BOOT_AUTOCONF; // TODO perhaps this is not completely correct
	port->iotype = UPIO_PORT; // TODO perhaps this is not completely correct
	port->regshift = 0;
	port->ops = &milkymistuart_pops;
	port->line = 0;
	port->fifosize = 1;
	port->private_data = NULL;
	return port;
}

static int __devinit milkymistuart_serial_probe(struct platform_device *pdev)
{
	struct uart_port *port;
	int ret;

	if( pdev->id != 0 )
		return -1;

	port = milkymistuart_init_port(pdev);

	ret = uart_add_one_port(&milkymistuart_driver, port);
	if (!ret) {
		pr_info("milkymist_uart: added port %d with irq %d-%d at 0x%lx\n",
				port->line, IRQ_UARTRX, IRQ_UARTTX, (unsigned long)port->membase);
		device_init_wakeup(&pdev->dev, 1);
		platform_set_drvdata(pdev, port);
	} else
		printk(KERN_ERR "milkymist_uart: could not add port %d: %d\n", port->line, ret);

	return ret;
}

static int __devexit milkymistuart_serial_remove(struct platform_device *pdev)
{
	struct uart_port *port = platform_get_drvdata(pdev);
	int ret = 0;

	device_init_wakeup(&pdev->dev, 0);
	platform_set_drvdata(pdev, NULL);

	if (port) {
		ret = uart_remove_one_port(&milkymistuart_driver, port);
		kfree(port);
	}

	return ret;
}

static struct platform_driver milkymistuart_serial_driver = {
	.probe		= milkymistuart_serial_probe,
	.remove		= __devexit_p(milkymistuart_serial_remove),
	.driver		= {
		.name	= "milkymist_uart",
		.owner	= THIS_MODULE,
	},
};

static int __init milkymistuart_init(void)
{
	int ret;
	
	pr_info("milkymist_uart: Milkymist UART driver\n");

	/* configure from hardware setup structures */
	milkymistuart_driver.nr = 1;
	ret = uart_register_driver(&milkymistuart_driver);
	if( ret < 0 )
		return ret;

	ret = platform_driver_register(&milkymistuart_serial_driver);
	if( ret < 0 )
		uart_unregister_driver(&milkymistuart_driver);

	return ret;
}

static void __exit milkymistuart_exit(void)
{
	platform_driver_unregister(&milkymistuart_serial_driver);
	uart_unregister_driver(&milkymistuart_driver);
}

module_init(milkymistuart_init);
module_exit(milkymistuart_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Milkymist UART driver");
MODULE_AUTHOR("Milkymist Project");
