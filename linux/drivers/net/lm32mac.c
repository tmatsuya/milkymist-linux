/*
 * (C) Copyright 2007 - 2008
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
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/in.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/crc32.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>

#include "lm32mac.h"

// static void lm32mac_tx_err(struct net_device *dev);
// static void lm32mac_tx_timeout(struct net_device *dev);
// static void lm32mac_receive(struct net_device *dev);
// static void lm32mac_rx_overrun(struct net_device *dev);
// 
// static void lm32mac_trigger_send(struct net_device *dev, unsigned int length,
// 								int start_page);


/* normal registers (see next the following enum for special registers) */
enum LM32MAC_registers {
	RX_LEN_FIFO = 0x000,
	RX_DATA_FIFO = 0x004,
	TX_LEN_FIFO = 0x008,
	TX_DATA_FIFO = 0x00C,
	VERID = 0x100,
	INTR_SRC = 0x104,
	INTR_ENB = 0x108,
	RX_STATUS = 0x10C,
	TX_STATUS = 0x110,
	RX_FRAMES_CNT = 0x114,
	TX_FRAMES_CNT = 0x118,
	RX_FIFO_TH = 0x11C,
	TX_FIFO_TH = 0x120,
	SYS_CTL = 0x124,
	PAUSE_TMR = 0x128,
	MAC_REGS_DATA = 0x200,
	MAC_REGS_ADDR_RW = 0x204
	/* statistics counter registers omitted for now */
};

/* special MAC registers (indirect addressing via MAC_REGS_DATA, MAC_REGS_ADDR_RW) */
enum LM32MAC_mac_registers {
	MAC_MODE = 0x00,
	MAC_TX_RX_CTL = 0x02,
	MAC_MAX_PKT_SIZE = 0x04,
	MAC_IPG_VAL = 0x08,
	MAC_MAC_ADDR_0 = 0x0A,
	MAC_MAC_ADDR_1 = 0x0C,
	MAC_MAC_ADDR_2 = 0x0E,
	MAC_TX_RX_STS = 0x12,
	MAC_GMII_MNG_CTL = 0x14,
	MAC_GMII_MNG_DAT = 0x16,
	MAC_MLT_TAB_0 = 0x22,
	MAC_MLT_TAB_1 = 0x24,
	MAC_MLT_TAB_2 = 0x26,
	MAC_MLT_TAB_3 = 0x28,
	MAC_MLT_TAB_4 = 0x2A,
	MAC_MLT_TAB_5 = 0x2C,
	MAC_MLT_TAB_6 = 0x2E,
	MAC_MLT_TAB_7 = 0x30,
	MAC_VLAN_TAG = 0x32,
	MAC_PAUSE_OP = 0x34
};

/* receive/transmit status bit masks
 * this is applicable to the registers RX_STATUS and TX_STATUS */
enum LM32MAC_xstatus {
	MAC_XSTATUS_FINISHED = 0x01, /* sent/received completely into/from fifo */
	MAC_XSTATUS_FIFO_ALMOST_FULL = 0x02,
	MAC_XSTATUS_FIFO_ALMOST_EMPTY = 0x04,
	MAC_XSTATUS_ERR_PKT = 0x8,
	MAC_XSTATUS_ERR_FIFO_FULL = 0x10,
	MAC_XSTATUS_ERR = 0x18 /* any error */
};

/* interrupt register bits
 * this is applicable to the registers INTR_SRC and INTR_ENB */
enum LM32MAC_interrupt_bits {
	MAC_INTR_TX_SENT = 0x0001,
	MAC_INTR_TX_FIFO_ALMOST_FULL = 0x0002,
	MAC_INTR_TX_FIFO_ALMOST_EMPTY = 0x0004,
	MAC_INTR_TX_ERROR = 0x0008,
	MAC_INTR_TX_FIFO_FULL = 0x0010,
	MAC_INTR_RX_READY = 0x0100,
	MAC_INTR_RX_FIFO_ALMOST_FULL = 0x0200,
	MAC_INTR_RX_FIRO_ERROR = 0x0400,
	MAC_INTR_RX_ERROR = 0x0800,
	MAC_INTR_RX_FIFO_FULL = 0x1000,
	MAC_INTR_TX_SUMMARY = 0x10000,
	MAC_INTR_RX_SUMMARY = 0x20000,
	MAC_INTR_ENABLE = 0x40000
};

/* transmit/receive control register bits
 * this is applicable to the MAC_TX_RX_CTL register */
enum LM32MAC_tx_rx_control_bits {
	MAC_TXRXCTL_RX_SHORT = 0x100,
	MAC_TXRXCTL_RX_BCAST = 0x080,
	MAC_TXRXCTL_DISABLE_RTY = 0x040,
	MAC_TXRXCTL_HALFDUPLEX = 0x020,
	MAC_TXRXCTL_RX_MCAST = 0x010,
	MAC_TXRXCTL_RX_PAUSE = 0x008,
	MAC_TXRXCTL_TX_DISABLE_FCS = 0x004,
	MAC_TXRXCTL_RX_DISCARD_FCS_PAD = 0x002,
	MAC_TXRXCTL_PROMISCOUS = 0x001
};

/* pointer to register
 * use one of the LM32MAC_registers enum values as name */
#define LM32_PREG(idx, name) ((volatile ulong*)(lm32tag_trispeedmac[idx]->addr+(name)))

/* write long (32 bit) to register
 * use one of the LM32MAC_registers enum values as name */
#define LM32_OUTL(idx, name, value) *LM32_PREG((idx), (name)) = value;

/* read long (32 bit) from register
 * use one of the LM32MAC_registers enum values as name */
#define LM32_INL(idx, name) (*LM32_PREG((idx), (name)))

/* write to MAC register (special write mode)
 * use one of the LM32MAC_mac_registers enum values as reg_addr */
static void macout(u8 idx, ulong reg_addr, ushort value)
{
#ifdef DEBUG_INIT
	printk(KERN_DEBUG "lm32mac[%d]:writing 0x%x to MAC register 0x%lx\n", idx, value, reg_addr);
#endif
	LM32_OUTL(idx, MAC_REGS_DATA, value);
	udelay(1000);
	LM32_OUTL(idx, MAC_REGS_ADDR_RW, (reg_addr) | 0x80000000);
}

/* read from MAC register (special reading mode)
 * use one of the LM32MAC_mac_registers enum values as reg_addr */
static ushort macin(u8 idx, ulong reg_addr)
{
	ushort ret;

	LM32_OUTL(idx, MAC_REGS_ADDR_RW, reg_addr);
	udelay(1000);
	ret = LM32_INL(idx, MAC_REGS_DATA) & 0x0000FFFF;
#ifdef DEBUG_INIT
	printk(KERN_DEBUG "lm32mac[%d]:read 0x%x from MAC register 0x%lx\n", idx, ret, reg_addr);
#endif
	return ret;
}

static void mac_enable(struct net_device *dev);
static void mac_disable(struct net_device *dev);
static void mac_reset(struct net_device *dev);

static irqreturn_t lm32mac_interrupt(int irq, void* devarg);
static void lm32mac_trytosend(struct net_device* dev);
static void lm32mac_disable_interrupts(struct net_device *dev);
static void lm32mac_enable_interrupts(struct net_device *dev);

static int lm32mac_open(struct net_device *dev);
static int lm32mac_close(struct net_device *dev);

static irqreturn_t lm32mac_interrupt(int irq, void* devarg)
{
	struct net_device* dev = (struct net_device*)devarg;
	struct LM32MAC_Priv* priv = (struct LM32MAC_Priv*) netdev_priv(dev);
	unsigned long idx = priv->idx;
	irqreturn_t ret = IRQ_NONE;

	/* get interrupt source */
	unsigned long intr_src = LM32_INL(idx, INTR_SRC);
	if( intr_src & MAC_INTR_RX_READY )
	{
		ulong rxframes;
		ret = IRQ_HANDLED;
		/* received packet */
		rxframes = LM32_INL(idx, RX_FRAMES_CNT) & 0x0000FFFF;
		while( rxframes > 0 )
		{
			/* read next packet length */
			ulong rxlen;
			struct sk_buff *skb;

			rxlen = LM32_INL(idx, RX_LEN_FIFO) & 0x0000FFFF;
			skb = dev_alloc_skb(rxlen + 2);
			if( !skb )
			{
				/* drop packet if too large for buffer */

				/* now read all data (to discard it) */
				ulong recvd = 0;
				while(recvd < rxlen)
				{
					LM32_INL(idx, RX_DATA_FIFO);
					recvd += 4;
				}
				/* try next packet, perhaps it will fit */
			}
			else
			{
				int net_ret;
				ulong recvd;
				char* buf;

				skb_reserve(skb,2);	/* IP headers on 16 byte boundaries */
				buf = skb_put(skb, rxlen);

				/* read all data and write to buffer */
				recvd = 0;
				while(recvd < rxlen)
				{
					unsigned long data = LM32_INL(idx, RX_DATA_FIFO);
					/* write to buffer */
					*buf++ = data >> 24;
					*buf++ = (data >> 16) & 0xFF;
					*buf++ = (data >> 8) & 0xFF;
					*buf++ = data & 0xFF;
					recvd += 4;
				}

				skb->protocol = eth_type_trans(skb, dev);

#ifdef DEBUG_RX
				printk("lm32mac: in %d\n", rxlen);
#endif
				net_ret = netif_rx(skb);
				dev->last_rx = jiffies;
				if( net_ret == NET_RX_DROP )
					printk("lm32mac.c: dropped packet!\n");
			}
			rxframes = LM32_INL(idx, RX_FRAMES_CNT) & 0x0000FFFF;
		}
	}
	if( intr_src & MAC_INTR_TX_SENT ) {
		lm32mac_trytosend(dev);
		ret = IRQ_HANDLED;
	}

	return ret;
}

static void lm32mac_trytosend(struct net_device* dev)
{
	struct LM32MAC_Priv* priv = (struct LM32MAC_Priv*) netdev_priv(dev);
	unsigned long idx = priv->idx;
	unsigned long flags;

	/* sent packet */
	ulong txframes = LM32_INL(idx, TX_FRAMES_CNT) & 0x0000FFFF;
	if( (txframes == 0) && (priv->out_buffer.length != 0) )
	{
		ulong txlen;
		ulong sent;

		/* no frame left in fifo and packet waiting in buffer
		 * -> write next packet to fifo */

		/* protect out_buffer */
		spin_lock_irqsave(&priv->out_buffer_lock, flags);

		txlen = priv->out_buffer.data[priv->out_buffer.first];
		if( txlen > LM32MAC_BUFFERSIZE ) {
			printk(KERN_ERR "lm32mac: inconsistency in buffer: txlen = %lx bytes\n", txlen);
			priv->out_buffer.length = 0;
			spin_unlock_irqrestore(&priv->out_buffer_lock, flags);
			return;
		}
		priv->out_buffer.first  = (priv->out_buffer.first + 1) & LM32MAC_BUFFERSIZE_MASK;
		priv->out_buffer.length--;

		sent = 0;
		while(sent < txlen)
		{
			LM32_OUTL(idx, TX_DATA_FIFO, priv->out_buffer.data[priv->out_buffer.first]);
			priv->out_buffer.first  = (priv->out_buffer.first + 1) & LM32MAC_BUFFERSIZE_MASK;
			priv->out_buffer.length--;
			sent += 4;
		}
		/* write length */
		LM32_OUTL(idx, TX_LEN_FIFO, txlen);
		dev->trans_start = jiffies;

		/* unprotect out_buffer */
		spin_unlock_irqrestore(&priv->out_buffer_lock, flags);

		if( priv->out_buffer.length < (LM32MAC_BUFFERSIZE >> 2) )
			/* restart queue in case it was stopped because of a buffer overflow */
			netif_start_queue(dev);
		else
			/* wake queue in any other case */
			netif_wake_queue(dev);
	}
}

static int lm32mac_send(struct sk_buff* skb, struct net_device *dev)
{
	struct LM32MAC_Priv* priv = (struct LM32MAC_Priv*) netdev_priv(dev);
	volatile unsigned char* ch;
	ulong sent, maxsent, tosend;
	unsigned long flags;

	lm32mac_disable_interrupts(dev);

#ifdef	DEBUG_TX
	printk("lm32mac: out %d\n", skb->len);
#endif

	/* +2 because: 1 ulong safety if rxlen is not an integer multiple of 4 + 1 ulong length marker */
	if( ((skb->len >> 2) + priv->out_buffer.length + 2) > LM32MAC_BUFFERSIZE )
	{
		/* cannot store in buffer for sending because buffer too full */
#ifdef	DEBUG_TX
		printk(KERN_DEBUG "lm32mac[%d]:could not send %d bytes, buffer contains %lu bytes.\n",
				priv->idx, skb->len, priv->out_buffer.length);
#endif
		netif_stop_queue(dev);
		lm32mac_enable_interrupts(dev);
		return 1; /* error */
	}

	ch = skb->data;
#if defined(DEBUG_TX) && defined(DEBUG_DATA)
	{
		int i;
		for(i = 0; i < skb->len; ++i)
			if( i % 8 == 7 )
				printk("%02x\n", ch[i]);
			else
				printk("%02x ", ch[i]);
		printk("\n");
	}
#endif

	/* protect out_buffer */
	spin_lock_irqsave(&priv->out_buffer_lock, flags);

	/* write data length (2 bytes) to buffer */
	priv->out_buffer.data[(priv->out_buffer.first + priv->out_buffer.length) & LM32MAC_BUFFERSIZE_MASK] =
		skb->len;
	priv->out_buffer.length++;

	/* read all data and write to buffer */
	sent = 0;
	/* calculate how many ulongs can be safely sent */
	maxsent = skb->len >> 2;
	ch = skb->data;
	while(sent < maxsent)
	{
		/* write to buffer */
		unsigned char* out = (unsigned char*)&priv->out_buffer.data[
			(priv->out_buffer.first + priv->out_buffer.length) & LM32MAC_BUFFERSIZE_MASK];
		*out++ = *ch++;
		*out++ = *ch++;
		*out++ = *ch++;
		*out = *ch++;
#if	defined(DEBUG_TX) && defined(DEBUG_DATA)
		printk("writing to buffer: %08lx\n",
				priv->out_buffer.data[(priv->out_buffer.first + priv->out_buffer.length) & LM32MAC_BUFFERSIZE_MASK]);
#endif
		priv->out_buffer.length++;
		sent++;
	}

	/* send remaining data (buffer may not be multiple of 4 large) */
	if( (skb->len & 0x3) != 0 ) {
		unsigned char* out = (unsigned char*)&priv->out_buffer.data[
			(priv->out_buffer.first + priv->out_buffer.length) & LM32MAC_BUFFERSIZE_MASK];
		tosend = skb->len & 0x3;
		while( tosend != 0 ) {
#if	defined(DEBUG_TX) && defined(DEBUG_DATA)
			printk("writing partial last byte to buffer: %02x\n", *ch);
#endif
			*out++ = *ch++;
			tosend--;
		}

		priv->out_buffer.length++;
	}

	/* unprotect out_buffer */
	spin_unlock_irqrestore(&priv->out_buffer_lock, flags);

	/* try to send, this will only send if there is no send currently in progress
	 * if this does not send, the next "sucessful send" interrupt will again call this method */
	lm32mac_trytosend(dev);

	dev_kfree_skb(skb);

	lm32mac_enable_interrupts(dev);

#ifdef	DEBUG_TX
	printk("lm32mac: oout %d\n", skb->len);
#endif

	return 0 /* SUCCESS */;
}

static void lm32mac_disable_interrupts(struct net_device *dev)
{
	struct LM32MAC_Priv* priv = (struct LM32MAC_Priv*) netdev_priv(dev);
	ulong intr_enb = LM32_INL(priv->idx, INTR_ENB);
	LM32_OUTL(priv->idx, INTR_ENB, intr_enb & (~MAC_INTR_ENABLE));
}

static void lm32mac_enable_interrupts(struct net_device *dev)
{
	struct LM32MAC_Priv* priv = (struct LM32MAC_Priv*) netdev_priv(dev);
	ulong intr_enb = LM32_INL(priv->idx, INTR_ENB);
	LM32_OUTL(priv->idx, INTR_ENB, intr_enb | MAC_INTR_ENABLE);
}

static void mac_disable(struct net_device *dev)
{
	struct LM32MAC_Priv* priv = (struct LM32MAC_Priv*) netdev_priv(dev);
	/* disable rx and tx */
	macout(priv->idx, MAC_MODE, 0x0000);
}

static void mac_enable(struct net_device *dev)
{
	struct LM32MAC_Priv* priv = (struct LM32MAC_Priv*) netdev_priv(dev);
	/* enable rx and tx (no gbit, no flow control) */
	macout(priv->idx, MAC_MODE, 0x000C);
}

static void mac_reset(struct net_device *dev)
{
	struct LM32MAC_Priv* priv = (struct LM32MAC_Priv*) netdev_priv(dev);
	/* flush fifos */
	LM32_OUTL(priv->idx, SYS_CTL, 0x00000018);

	/* read clear-on-read registers */
	(void)LM32_INL(priv->idx, INTR_SRC);
	(void)LM32_INL(priv->idx, TX_STATUS);
	(void)LM32_INL(priv->idx, RX_STATUS);

	/* reset flush bits */
	LM32_OUTL(priv->idx, SYS_CTL, 0x00000000);
}

static int lm32mac_set_mac_address(struct net_device *netdev, void *p)
{
#if 0
	struct nic *nic = netdev_priv(netdev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	e100_exec_cb(nic, NULL, e100_setup_iaaddr);
#endif

	/* TODO:
	   1. Refactor lm32mac_open into _open & _init
	   2. Implement _set_mac_address & call _init/_reset
	 */

#if 1
	struct LM32MAC_Priv *priv = (struct LM32MAC_Priv*) netdev_priv(netdev);
	struct sockaddr *addr = p;
	unsigned short *ap;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);

	mac_disable(netdev);

	/* configure ethernet (MAC) address */
	ap = (unsigned short*)netdev->dev_addr;
	macout(priv->idx, MAC_MAC_ADDR_0, ap[0]);
	macout(priv->idx, MAC_MAC_ADDR_1, ap[1]);
	macout(priv->idx, MAC_MAC_ADDR_2, ap[2]);

	/* configure for receiving normal packets + broadcast */
	macout(priv->idx, MAC_TX_RX_CTL, MAC_TXRXCTL_RX_BCAST);

	mac_enable(netdev);
	mac_reset(netdev);
#endif

	return 0;
}

static int lm32mac_open(struct net_device *dev)
{
	int err;
	struct LM32MAC_Priv* priv = (struct LM32MAC_Priv*) netdev_priv(dev);
	/* no interrupts */
	ulong intr_enb = 0x00000000;

	printk(KERN_DEBUG "opening device %s\n", dev->name);

	mac_disable(dev);

	/* setup receive ring buffer */
	priv->in_buffer.first = 0;
	priv->in_buffer.length = 0;

	/* we have to enable the RX summary bit else no interrupt will be generated at all */
	intr_enb |= MAC_INTR_RX_READY | MAC_INTR_RX_SUMMARY | MAC_INTR_ENABLE;

	/* setup transmit ring buffer */
	priv->out_buffer.first = 0;
	priv->out_buffer.length = 0;
	/* we have to enable the TX summary bit else no interrupt will be generated at all */
	intr_enb |= MAC_INTR_TX_SENT | MAC_INTR_TX_SUMMARY | MAC_INTR_ENABLE;

	err = request_irq(dev->irq, lm32mac_interrupt, IRQF_DISABLED,
			dev->name, (void*)dev);
	if (err) {
		printk(KERN_ERR "lm32mac: failed to request irq %d for device %s.\n",
				dev->irq, dev->name);
		return err;
	}

	/* configure interrupt enable register */
	LM32_OUTL(priv->idx, INTR_ENB, intr_enb);

	if (!is_valid_ether_addr(dev->dev_addr)) {
	        printk("%s: Invalid ethernet MAC address.  Please set using ifconfig\n", dev->name);
	}

	/* configure for receiving normal packets + broadcast */
	macout(priv->idx, MAC_TX_RX_CTL, MAC_TXRXCTL_RX_BCAST);

	{
		/* configure receive and transmit thresholds
		 * the thresholds are set to 1/4 and 3/4 of their allowed ranges which is a useful setting
		 * note: the max range is fifo_depth */
		u32 fifo_depth_div_4 = lm32tag_trispeedmac[priv->idx]->fifo_depth / 4;
		u32 fifo_depth_mul3_div_4 = fifo_depth_div_4 * 3;
		u32 fifo_th = (fifo_depth_div_4 << 16) | (fifo_depth_mul3_div_4);

		printk(KERN_DEBUG "lm32mac[%d]:setting fifo threshold mask to 0x%x\n", priv->idx, fifo_th);
		LM32_OUTL(priv->idx, RX_FIFO_TH, fifo_th);
		LM32_OUTL(priv->idx, TX_FIFO_TH, fifo_th);
	}

	mac_enable(dev);

	mac_reset(dev);

	netif_start_queue(dev);

	/* enable interrupt */
	lm32_irq_unmask(dev->irq);

	return 0;
}

static int lm32mac_close(struct net_device *dev)
{
	printk(KERN_DEBUG "closing device %s\n", dev->name);

	mac_disable(dev);
	free_irq(dev->irq, dev);
	netif_stop_queue(dev);

	return 0;
}

static struct net_device_stats* lm32mac_get_stats(struct net_device *dev)
{
	struct LM32MAC_Priv* priv = (struct LM32MAC_Priv*) netdev_priv(dev);
	return &priv->stats;
}

/*
 * register all TriSpeedMACs found in the hardware configuration structures.
 */
static int __init lm32mac_probe(struct platform_device *dev)
{
	unsigned long idx;
	struct net_device *netdev;
	struct LM32MAC_Priv* priv;
	int err;
	ulong verid;
	unsigned char* ap;
	ushort a0, a1, a2;
	int i;

	printk("lm32mac: %lu devices\n", lm32tag_num_trispeedmac);
	for(idx = 0; idx < lm32tag_num_trispeedmac; ++idx) {
		netdev = alloc_etherdev(sizeof(struct LM32MAC_Priv));
		if (!netdev) {
			err = -ENOMEM;
			return err;
		}

		netdev->open		= lm32mac_open;
		netdev->stop		= lm32mac_close;
		netdev->hard_start_xmit	= lm32mac_send;
		netdev->get_stats	= lm32mac_get_stats;
		netdev->set_mac_address = lm32mac_set_mac_address;

		netdev->base_addr       = lm32tag_trispeedmac[idx]->addr;
		netdev->irq             = lm32tag_trispeedmac[idx]->irq;

		priv = (struct LM32MAC_Priv*)netdev_priv(netdev);
		priv->idx = idx;
		priv->in_buffer.first = priv->in_buffer.length = 0;
		priv->out_buffer.first = priv->out_buffer.length = 0;
		spin_lock_init(&priv->out_buffer_lock);
		memset(&priv->stats, 0, sizeof(struct net_device_stats));

		// version info output
		verid = LM32_INL(idx, VERID);
		pr_info("lm32mac version 0x%lx @ 0x%lx\n", verid, netdev->base_addr);

		SET_NETDEV_DEV(netdev, &dev->dev);

		err = register_netdev(netdev);
		if (err) {
			printk(KERN_ERR "lm32mac: failed to register netdev.\n");
			free_netdev(netdev);
			return err;
		}

		/* Retrieve the configure MAC address from the MAC */
		ap = (unsigned char*)(&netdev->dev_addr[0]);
		a0 = macin(priv->idx, MAC_MAC_ADDR_0); 
		a1 = macin(priv->idx, MAC_MAC_ADDR_1); 
		a2 = macin(priv->idx, MAC_MAC_ADDR_2); 
		*ap++ = a0 >> 8;
		*ap++ = a0 & 0xFF;
		*ap++ = a1 >> 8;
		*ap++ = a1 & 0xFF;
		*ap++ = a2 >> 8;
		*ap++ = a2 & 0xFF;

		printk("%s: LM32 Tri-Speed MAC (version 0x%lx) at 0x%lx IRQ %d MAC: ", 
		       netdev->name, verid, netdev->base_addr, netdev->irq);

		for (i = 0; i < 5; i++)
			printk("%02x:", netdev->dev_addr[i]);
		printk("%02x\n", netdev->dev_addr[5]);
	}

	return 0;
}

static int __exit lm32mac_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver lm32mac_driver = {
	.probe	= lm32mac_probe,
	.remove	= __devexit_p(lm32mac_remove),
	.driver = {
		.name	= "lm32mac",
	}
};

static int __init lm32mac_init_module(void)
{
	int err;

	pr_info("%s\n", DRIVER_NAME);

	err = platform_driver_probe(&lm32mac_driver, lm32mac_probe);
	if( err )
		goto out;

	return 0;

out:
	return err;
}

static void __exit lm32mac_exit_module(void)
{
	platform_driver_unregister(&lm32mac_driver);
}

module_init(lm32mac_init_module);
module_exit(lm32mac_exit_module);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Lattice Mico 32 Tri Speed Ethernet MAC driver");
MODULE_AUTHOR("Theobroma Systems mico32@theobroma-systems.com");
