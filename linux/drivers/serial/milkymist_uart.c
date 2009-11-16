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

#include "milkymist_uart.h"

#define MMPTR(x) (*((volatile unsigned int *)(x)))
#define CSR_UART_RXTX		MMPTR(0x80000000)
#define CSR_UART_DIVISOR	MMPTR(0x80000004)

/* these two will be initialized by lm32uart_init */
static struct uart_port lm32uart_ports[1];
static struct LM32_uart_priv lm32uart_privs[1];

static struct uart_port* __devinit lm32uart_init_port(struct platform_device *pdev);

static unsigned int lm32uart_tx_empty(struct uart_port *port);
static void lm32uart_set_mctrl(struct uart_port *port, unsigned int mctrl);
static unsigned int lm32uart_get_mctrl(struct uart_port *port);
static void lm32uart_start_tx(struct uart_port *port);
static void lm32uart_stop_tx(struct uart_port *port);
static void lm32uart_stop_rx(struct uart_port *port);
static void lm32uart_enable_ms(struct uart_port *port);
static void lm32uart_break_ctl(struct uart_port *port, int break_state);
static int lm32uart_startup(struct uart_port *port);
static void lm32uart_shutdown(struct uart_port *port);
static void lm32uart_set_termios(struct uart_port *port, struct ktermios *termios, struct ktermios *old);
static const char *lm32uart_type(struct uart_port *port);
static void lm32uart_release_port(struct uart_port *port);
static int lm32uart_request_port(struct uart_port *port);
static void lm32uart_config_port(struct uart_port *port, int flags);
static int lm32uart_verify_port(struct uart_port *port, struct serial_struct *ser);

static inline void lm32uart_set_baud_rate(struct uart_port *port, unsigned long baud);
static irqreturn_t lm32uart_irq_rx(int irq, void* portarg);
static irqreturn_t lm32uart_irq_tx(int irq, void* portarg);

static struct uart_ops lm32uart_pops = {
	.tx_empty	= lm32uart_tx_empty,
	.set_mctrl	= lm32uart_set_mctrl,
	.get_mctrl	= lm32uart_get_mctrl,
	.stop_tx	= lm32uart_stop_tx,
	.start_tx	= lm32uart_start_tx,
	.stop_rx	= lm32uart_stop_rx,
	.enable_ms	= lm32uart_enable_ms,
	.break_ctl	= lm32uart_break_ctl,
	.startup	= lm32uart_startup,
	.shutdown	= lm32uart_shutdown,
	.set_termios	= lm32uart_set_termios,
	.type = lm32uart_type,
	.release_port	= lm32uart_release_port,
	.request_port	= lm32uart_request_port,
	.config_port	= lm32uart_config_port,
	.verify_port	= lm32uart_verify_port
};

extern unsigned lm32tag_num_uart;

static inline void lm32uart_set_baud_rate(struct uart_port *port, unsigned long baud)
{
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;

	uart->div = 100000000 / baud / 16;
}

static void lm32uart_tx_next_char(struct uart_port* port, LM32_uart_t* uart)
{
	/* interrupt already cleared */
	struct circ_buf *xmit = &(port->info->xmit);

	if (port->x_char) {
		/* send xon/xoff character */
		uart->rxtx = port->x_char;
		port->x_char = 0;
		port->icount.tx++;
		return;
	}

	/* stop transmitting if buffer empty */
	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		lm32uart_stop_tx(port);
		return;
	}
	
	/* send next character */
	uart->rxtx = xmit->buf[xmit->tail];
	xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
	port->icount.tx++;

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit))
		lm32uart_stop_tx(port);

}

static void lm32uart_rx_next_char(struct uart_port* port, LM32_uart_t* uart)
{
	struct tty_struct *tty = port->info->tty;
	unsigned long ch;
	unsigned long ucr = 0;
	unsigned int flg;

	ch = uart->rxtx & 0xFF;
	port->icount.rx++;

	flg = TTY_NORMAL;

	if (uart_handle_sysrq_char(port, ch))
		goto ignore_char;

	uart_insert_char(port, ucr, LM32_UART_LSR_OE, ch, flg);

ignore_char:

	tty_flip_buffer_push(tty);
}

static irqreturn_t lm32uart_irq_rx(int irq, void* portarg)
{
	struct uart_port* port = (struct uart_port*)portarg;
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;
	irqreturn_t ret = IRQ_NONE;

	/* data ready in buffer -> receive character */
	lm32uart_rx_next_char(port, uart);
	return IRQ_HANDLED;
}

static irqreturn_t lm32uart_irq_tx(int irq, void* portarg)
{
	struct uart_port* port = (struct uart_port*)portarg;
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;
	irqreturn_t ret = IRQ_NONE;

	/* transmit register empty -> can send next byte */
	lm32uart_tx_next_char(port, uart);
	return IRQ_HANDLED;
}

static unsigned int lm32uart_tx_empty(struct uart_port *port)
{
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;

	if( uart->ucr & LM32_UART_TX_BUSY )
		return 0;
	else
		return TIOCSER_TEMT;
}

static void lm32uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	// TODO do we need modem control?
}

static unsigned int lm32uart_get_mctrl(struct uart_port *port)
{
	// TODO do we need modem control?
	return 0;
}

static void lm32uart_start_tx(struct uart_port *port)
{
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;
	LM32_uart_priv_t* priv = (LM32_uart_priv_t*)port->private_data;

	lm32_irq_unmask(port->irq+1);
}

static void lm32uart_stop_tx(struct uart_port *port)
{
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;
	LM32_uart_priv_t* priv = (LM32_uart_priv_t*)port->private_data;

	lm32_irq_mask(port->irq+1);
}

static void lm32uart_stop_rx(struct uart_port *port)
{
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;
	LM32_uart_priv_t* priv = (LM32_uart_priv_t*)port->private_data;

	lm32_irq_mask(port->irq);
}

static void lm32uart_enable_ms(struct uart_port *port)
{
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;
	LM32_uart_priv_t* priv = (LM32_uart_priv_t*)port->private_data;

	lm32_irq_unmask(port->irq);
}

static void lm32uart_break_ctl(struct uart_port *port, int break_state)
{
}

static int lm32uart_startup(struct uart_port *port)
{
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;
	LM32_uart_priv_t* priv = (LM32_uart_priv_t*)port->private_data;

	if( request_irq(port->irq, lm32uart_irq_rx,
				IRQF_DISABLED, "milkymist_uart RX", port) ) {
		printk(KERN_NOTICE "Unable to attach Milkymist UART RX interrupt\n");
		return -EBUSY;
	}
#if 0
	if( request_irq(port->irq+1, lm32uart_irq_tx,
				IRQF_DISABLED, "milkymist_uart TX", port) ) {
		printk(KERN_NOTICE "Unable to attach Milkymist UART TX interrupt\n");
		return -EBUSY;
	}
#endif

	lm32_irq_unmask(port->irq);
#if 0
	lm32_irq_unmask(port->irq+1);
#endif

	return 0;
}

static void lm32uart_shutdown(struct uart_port *port)
{
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;
	LM32_uart_priv_t* priv = (LM32_uart_priv_t*)port->private_data;

	/* deactivate irq and irq handling */
	free_irq(port->irq, port);
	free_irq(port->irq+1, port);
}

static void lm32uart_set_termios(
		struct uart_port *port, struct ktermios *termios, struct ktermios *old)
{
	unsigned long baud;
	unsigned long flags;
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;
	LM32_uart_priv_t* priv = (LM32_uart_priv_t*)port->private_data;

	/* >> 4 means / 16 */
	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk >> 4);

	/* deactivate irqs */
	spin_lock_irqsave(&port->lock, flags);

	lm32uart_set_baud_rate(port, baud);

	uart_update_timeout(port, termios->c_cflag, baud);

	/* read status mask */
	port->read_status_mask = LM32_UART_LSR_OE;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= LM32_UART_LSR_FE | LM32_UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		port->read_status_mask |= LM32_UART_LSR_BI;

	/* ignore status mask */
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= LM32_UART_LSR_PE | LM32_UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		port->ignore_status_mask |= LM32_UART_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			port->ignore_status_mask |= LM32_UART_LSR_OE;
	}
	if ((termios->c_cflag & CREAD) == 0)
		port->ignore_status_mask |= LM32_UART_RX_AVAILABLE;

	/* restore irqs */
	spin_unlock_irqrestore(&port->lock, flags);
}

static const char *lm32uart_type(struct uart_port *port)
{
	/* check, to be on the safe side */
	if( port->type == PORT_LM32UART )
		return "MILKYMIST_UART";
	else
		return "error";
}

static void lm32uart_release_port(struct uart_port *port)
{
}

static int lm32uart_request_port(struct uart_port *port)
{
	return 0;
}

/* we will only configure the port type here */
static void lm32uart_config_port(struct uart_port *port, int flags)
{
	if( flags & UART_CONFIG_TYPE ) {
		port->type = PORT_LM32UART;
	}
}

/* we do not allow the user to configure via this method */
static int lm32uart_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	return -EINVAL;
}

#ifdef CONFIG_SERIAL_MILKYMIST_CONSOLE
static void lm32_console_putchar(struct uart_port *port, int ch)
{
	CSR_UART_RXTX = ch;
	while(!(lm32_irq_pending() & (1 << IRQ_UARTTX)));
	lm32_irq_ack(IRQ_UARTTX);
}

/*
 * Interrupts are disabled on entering
 */
static void lm32_console_write(struct console *co, const char *s, u_int count)
{
	struct uart_port *port = &lm32uart_ports[co->index];
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;
	LM32_uart_priv_t* priv = (LM32_uart_priv_t*)port->private_data;

	uart_console_write(port, s, count, lm32_console_putchar);

	/* Wait for transmitter to become empty and restore interrupts */
#if 0
	while((uart->ucr & LM32_UART_LSR_THRE) == 0)
		barrier();
#endif
}

/*
 * If the port was already initialised (eg, by a boot loader), try to determine
 * the current setup.
 * In case of the LM32 we can only use the lm32tag_uart configuration as the
 * configuration registers of the uarts are write-only.
 */
static void __init lm32_console_get_options(struct uart_port *port, int *baud, int *parity, int *bits)
{
	int line = port->line;

	if( line < 0 || line >= lm32tag_num_uart ) {
		printk(KERN_ERR "invalid uart port line %d, using 0\n",
				line);
		line = 0;
	}

	*baud = 192000;
	*bits = 8;
	*parity = 'o'; /* TODO put parity into LM32TAG_UART */
}

static int __init lm32_console_setup(struct console *co, char *options)
{
	struct uart_port *port = &lm32uart_ports[co->index];
	LM32_uart_t* uart = (LM32_uart_t*)port->membase;
	LM32_uart_priv_t* priv = (LM32_uart_priv_t*)port->private_data;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (port->membase == 0)		/* Port not initialized yet - delay setup */
		return -ENODEV;

	/* configure */
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		lm32_console_get_options(port, &baud, &parity, &bits);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct uart_driver lm32uart_driver;

static struct console lm32_console = {
	.name		= LM32UART_DEVICENAME,
	.write		= lm32_console_write,
	.device		= uart_console_device,
	.setup		= lm32_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &lm32uart_driver,
};

/*
 * Early console initialization
 */
static int __init lm32_early_console_init(void)
{
	if( lm32tag_num_uart > 0 ) {
		/* first uart device = default console */
		add_preferred_console(LM32UART_DEVICENAME, lm32uart_default_console_device->id, NULL);
		lm32uart_init_port(&lm32uart_default_console_device);
		register_console(&lm32_console);
		pr_info("milkymist_uart: registered real console\n");
		return 0;
	} else
		return -1;
}
console_initcall(lm32_early_console_init);
/**/

/*
 * Late console initialization
 */
static int __init lm32_late_console_init(void)
{
	if( lm32tag_num_uart > 0 &&
			!(lm32_console.flags & CON_ENABLED) ) {
		register_console(&lm32_console);
		pr_info("milkymist_uart: registered real console\n");
	}
	return 0;
}
core_initcall(lm32_late_console_init);

#define LM32_CONSOLE_DEVICE	&lm32_console
#else
#define LM32_CONSOLE_DEVICE	NULL
#endif

static struct uart_driver lm32uart_driver = {
	.owner       = THIS_MODULE,
	.driver_name = LM32UART_DRIVERNAME,
	.dev_name    = LM32UART_DEVICENAME,
	.major       = LM32UART_MAJOR,
	.minor       = LM32UART_MINOR,
	.nr          = 0, /* will be filled from lm32tag_uart by init */
	.cons        = LM32_CONSOLE_DEVICE
};

static struct uart_port* __devinit lm32uart_init_port(struct platform_device *pdev)
{
	struct uart_port* port;
	
	port = &lm32uart_ports[0];
	port->type = PORT_LM32UART;
	port->iobase = (void __iomem*)0;;
	port->membase = (void __iomem*)0x80000000;
	port->irq = 3;
	port->uartclk = cpu_frequency * 16;
	port->flags = UPF_SKIP_TEST | UPF_BOOT_AUTOCONF; // TODO perhaps this is not completely correct
	port->iotype = UPIO_PORT; // TODO perhaps this is not completely correct
	port->ops = &lm32uart_pops;
	port->line = 0;
	port->private_data = (void*)&lm32uart_privs[0];
	return port;
}

static int __devinit lm32uart_serial_probe(struct platform_device *pdev)
{
	struct uart_port *port;
	int ret;

	if( pdev->id < 0 || pdev->id >= lm32tag_num_uart )
		return -1;

	port = lm32uart_init_port(pdev);

	ret = uart_add_one_port(&lm32uart_driver, port);
	if (!ret) {
		pr_info("milkymist_uart: added port %d with irq %d at 0x%lx\n",
				port->line, port->irq, (unsigned long)port->membase);
		device_init_wakeup(&pdev->dev, 1);
		platform_set_drvdata(pdev, port);
	} else
		printk(KERN_ERR "milkymist_uart: could not add port %d: %d\n", port->line, ret);

	return ret;
}

static int __devexit lm32uart_serial_remove(struct platform_device *pdev)
{
	struct uart_port *port = platform_get_drvdata(pdev);
	int ret = 0;

	device_init_wakeup(&pdev->dev, 0);
	platform_set_drvdata(pdev, NULL);

	if (port) {
		ret = uart_remove_one_port(&lm32uart_driver, port);
		kfree(port);
	}

	return ret;
}

static struct platform_driver lm32uart_serial_driver = {
	.probe		= lm32uart_serial_probe,
	.remove		= __devexit_p(lm32uart_serial_remove),
	.driver		= {
		.name	= "milkymist_uart",
		.owner	= THIS_MODULE,
	},
};

static int __init lm32uart_init(void)
{
	int ret;

	pr_info("milkymist_uart: Milkymist UART driver\n");

	/* configure from hardware setup structures */
	lm32uart_driver.nr = lm32tag_num_uart;
	ret = uart_register_driver(&lm32uart_driver);
	if( ret < 0 )
		return ret;

	ret = platform_driver_register(&lm32uart_serial_driver);
	if( ret < 0 )
		uart_unregister_driver(&lm32uart_driver);

	return ret;
}

static void __exit lm32uart_exit(void)
{
	platform_driver_unregister(&lm32uart_serial_driver);
	uart_unregister_driver(&lm32uart_driver);
}

module_init(lm32uart_init);
module_exit(lm32uart_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Milkymist UART driver");
MODULE_AUTHOR("Theobroma Systems mico32@theobroma-systems.com");
