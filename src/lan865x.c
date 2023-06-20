// SPDX-License-Identifier: GPL-2.0+
/*
 * Microchip LAN865x 10BASE-T1S Ethernet driver (MAC + PHY)
 *
 * (c) Copyright 2023 Microchip Technology Inc.
 * Author: Parthiban Veerasooran <parthiban.veerasooran@microchip.com>
 * using Open Alliance tc6 library written by Thorsten Kummermehr
 * <thorsten.kummermehr@microchip.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/property.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/kthread.h>

#include "tc6.h"

#define DRV_NAME		"lan865x"
#define DRV_VERSION		"0.1"

#define PLCA_TO_TIMER_VAL	0x20
#define PLCA_BURST_CNT_VAL	0x00
#define PLCA_BURST_TMR_VAL	0x80

#define REG_STDR_RESET		0x3
#define REG_STDR_CONFIG0	0x4
#define REG_STDR_STATUS0	0x8

#define REG_MAC_NW_CTRL		0x00010000
#define REG_MAC_NW_CONFIG	0x00010001
#define REG_MAC_HASHL		0x00010020
#define REG_MAC_HASHH		0x00010021
#define REG_MAC_ADDR_BO		0x00010022
#define REG_MAC_ADDR_L		0x00010024
#define REG_MAC_ADDR_H		0x00010025

#define REG_PHY_PLCA_VER	0x0004ca00
#define REG_PHY_PLCA_CTRL0	0x0004ca01
#define REG_PHY_PLCA_CTRL1	0x0004ca02
#define REG_PHY_PLCA_TOTIME	0x0004ca04
#define REG_PHY_PLCA_BURST	0x0004ca05

#define MAC_PROMISCUOUS_MODE	0x00000010
#define MAC_MULTICAST_MODE	0x00000040
#define MAC_UNICAST_MODE	0x00000080
#define TX_CUT_THROUGH_MODE	0x9226
#define TX_STORE_FWD_MODE	0x9026
#define PLCA_ENABLE		0x8000
#define ETH_BUF_LEN		1536
#define ETH_MIN_HEADER_LEN	42
#define TX_TIMEOUT		(4 * HZ)

#define TX_TS_FLAG_TIMESTAMPING_ENABLED	BIT(0)

#define LAN865X_MSG_DEFAULT	\
	(NETIF_MSG_PROBE | NETIF_MSG_IFUP | NETIF_MSG_IFDOWN | NETIF_MSG_LINK)

/* driver local data */
struct lan865x_priv {
	u8 eth_rx_buf[ETH_BUF_LEN];
	struct completion tc6_completion;
	struct net_device *netdev;
	struct spi_device *spi;
	struct spi_message msg;
	struct spi_transfer transfer;
	/* To synchronize concurrent tc6 service from interrupt and need service operations */
	struct mutex lock;
	wait_queue_head_t irq_wq;
	wait_queue_head_t ns_wq;
	struct tc6_t tc6;
	struct timer_list tc6_timer;
	struct task_struct *ns_task;
	struct task_struct *irq_task;
	bool irq_flag;
	bool ns_flag;
	bool int_pending;
	u32 ts_flags;
	u32 msg_enable;
	u32 read_value;
	u16 eth_rxlength;
	u8 plca_enable;
	u8 plca_node_id;
	u8 plca_node_count;
	u8 plca_burst_count;
	u8 plca_burst_timer;
	u8 plca_to_timer;
	u8 tx_cut_thr_mode;
	u8 rx_cut_thr_mode;
	bool enable_data;
	bool spibusy;
	bool eth_rxinvalid;
	bool callbacksuccess;
};

/* use ethtool to change the level for any given device */
static struct {
	u32 msg_enable;
} debug = { -1 };

const struct memorymap_t tc6_memmap[]  =  {
	{ .address = 0x00000003, .value = 0x00000001, .mask = 0x00000000,
	  .op = memop_write, .secure = false }, /* RESET */
	{ .address = 0x00000003, .value = 0x00000001, .mask = 0x00000000,
	  .op = memop_write, .secure = true  }, /* RESET */
	{ .address = 0x00000004, .value = 0x00000026, .mask = 0x00000000,
	  .op = memop_write, .secure = false }, /* CONFIG0 */
    { .address = 0x00040091, .value = 0x00009660, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x00040081, .value = 0x000000C0, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x00010077, .value = 0x00000028, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x00040043, .value = 0x000000FF, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x00040044, .value = 0x0000FFFF, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x00040045, .value = 0x00000000, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x00040053, .value = 0x000000FF, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x00040054, .value = 0x0000FFFF, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x00040055, .value = 0x00000000, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x00040040, .value = 0x00000002, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x00040050, .value = 0x00000002, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400D0, .value = 0x00005F21, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400E9, .value = 0x00009E50, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400F5, .value = 0x00001CF8, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400F4, .value = 0x0000C020, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400F8, .value = 0x00009B00, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400F9, .value = 0x00004E53, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400B0, .value = 0x00000103, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400B1, .value = 0x00000910, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400B2, .value = 0x00001D26, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400B3, .value = 0x0000002A, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400B4, .value = 0x00000103, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400B5, .value = 0x0000070D, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400B6, .value = 0x00001720, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400B7, .value = 0x00000027, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400B8, .value = 0x00000509, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400B9, .value = 0x00000E13, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400BA, .value = 0x00001C25, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x000400BB, .value = 0x0000002B, .mask = 0x00000000,
	  .op = memop_write, .secure = true  },
    { .address = 0x00040087, .value = 0x00000083, .mask = 0x00000000,
	  .op = memop_write, .secure = true  }, /* COL_DET_CTRL0 */
    { .address = 0x0000000C, .value = 0x00000100, .mask = 0x00000000,
	  .op = memop_write, .secure = true  }, /* IMASK0 */
    { .address = 0x00040081, .value = 0x000000E0, .mask = 0x00000000,
	  .op = memop_write, .secure = true  }, /* DEEP_SLEEP_CTRL_1 */
    { .address = 0x00010000, .value = 0x0000000C, .mask = 0x00000000,
	  .op = memop_write, .secure = true  }, /* NETWORK_CONTROL */
};

const u32 tc6_memmap_length  =  (sizeof(tc6_memmap) / sizeof(struct memorymap_t));
/******************************************************************************
 * function name      : tc6_memmap_callback
 * description        : callback function from memmap register operations
 ******************************************************************************/
void tc6_memmap_callback(struct tc6_t *pinst, bool success, u32 addr,
		       u32 value, void *ptag, void *pglobaltag)
{
	struct lan865x_priv *priv = container_of(pinst, struct lan865x_priv, tc6);
	if (!success && (addr != REG_STDR_RESET)) {
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "spi memmap register failed for addr: %x", addr);
	}
}

/******************************************************************************
 * function name      : tc6_read_callback
 * description        : callback function from register read
 ******************************************************************************/
void tc6_read_callback(struct tc6_t *pinst, bool success, u32 addr,
		       u32 value, void *ptag, void *pglobaltag)
{
	struct lan865x_priv *priv = container_of(pinst, struct lan865x_priv, tc6);

	priv->callbacksuccess = success;
	priv->read_value = value;
	complete(&priv->tc6_completion);
}

/******************************************************************************
 * function name      : tc6_write_callback
 * description        : callback function from register write
 ******************************************************************************/
void tc6_write_callback(struct tc6_t *pinst, bool success, u32 addr, u32 value,
			void *ptag, void *pglobaltag)
{
	struct lan865x_priv *priv = container_of(pinst, struct lan865x_priv, tc6);

	priv->callbacksuccess = success;
	complete(&priv->tc6_completion);
}

/******************************************************************************
 * function name      : tc6_read_register
 * description        : function to read register
 ******************************************************************************/
bool tc6_read_register(struct lan865x_priv *priv, u32 addr, u32 *value)
{
	bool ret;
	long wait_remaining;

	reinit_completion(&priv->tc6_completion);
	ret = tc6_readregister(&priv->tc6, addr, tc6_read_callback, priv);
	if (ret) {
		priv->ns_flag = true;
		wake_up_interruptible(&priv->ns_wq);
		wait_remaining = wait_for_completion_interruptible_timeout(&priv->tc6_completion,
									   msecs_to_jiffies(100));
		if ((wait_remaining == 0) || (wait_remaining == -ERESTARTSYS)) {
			ret = false;
		} else {
			*value = priv->read_value;
			ret = priv->callbacksuccess;
		}
	}
	if (!ret) {
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "spi read register failed for addr: %x", addr);
	}
	return ret;
}

/******************************************************************************
 * function name      : tc6_write_register
 * description        : function to write register
 ******************************************************************************/
bool tc6_write_register(struct lan865x_priv *priv, u32 addr, u32 value)
{
	bool ret;
	long wait_remaining;

	reinit_completion(&priv->tc6_completion);
	ret = tc6_writeregister(&priv->tc6, addr, value, tc6_write_callback, priv);
	if (ret) {
		priv->ns_flag = true;
		wake_up_interruptible(&priv->ns_wq);
		wait_remaining = wait_for_completion_interruptible_timeout(&priv->tc6_completion,
									   msecs_to_jiffies(100));
		if ((wait_remaining == 0) || (wait_remaining == -ERESTARTSYS))
			ret = false;
		else
			ret = priv->callbacksuccess;
	}
	if (!ret) {
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "spi write register failed for addr: %x", addr);
	}
	return ret;
}

void tc6_cb_onrxethernetslice(struct tc6_t *pinst, const u8 *prx, u16 offset, u16 len,
			      void *pglobaltag)
{
	struct lan865x_priv *priv = container_of(pinst, struct lan865x_priv, tc6);

	if (priv->eth_rxinvalid)
		return;
	if (offset + len > ETH_BUF_LEN) {
		priv->eth_rxinvalid = true;
		return;
	}
	if (offset) {
		if (!priv->eth_rxlength) {
			priv->eth_rxinvalid = true;
			return;
		}
	} else {
		if (priv->eth_rxlength) {
			priv->eth_rxinvalid = true;
			return;
		}
	}
	memcpy(priv->eth_rx_buf + offset, prx, len);
	priv->eth_rxlength += len;
}

static void send_rx_eth_pkt(struct lan865x_priv *priv, u8 *buf, u16 len, u64 *rxtimestamp)
{
	struct sk_buff *skb = NULL;

	skb = netdev_alloc_skb(priv->netdev, len + NET_IP_ALIGN);
	if (!skb) {
		if (netif_msg_rx_err(priv)) {
			if (printk_ratelimit())
				netdev_err(priv->netdev, "out of memory for rx'd frame");
		}
		priv->netdev->stats.rx_dropped++;
	} else {
		skb_reserve(skb, NET_IP_ALIGN);
		/* copy the packet from the receive buffer */
		memcpy(skb_put(skb, len), buf, len);
		skb->protocol = eth_type_trans(skb, priv->netdev);
		/* update statistics */
		priv->netdev->stats.rx_packets++;
		priv->netdev->stats.rx_bytes += len;
		netif_rx_ni(skb);
	}
}

void tc6_cb_onrxethernetpacket(struct tc6_t *pinst, bool success, u16 len,
			       u64 *rxtimestamp, void *pglobaltag)
{
	struct lan865x_priv *priv = container_of(pinst, struct lan865x_priv, tc6);

	if (len > ETH_BUF_LEN) {
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "Packet length greater than MTU: %d\n\r", len);
	}

	if (!success || priv->eth_rxinvalid || !priv->eth_rxlength)
	{
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "Packet drop\n\r");
		priv->netdev->stats.rx_dropped++;
		goto end;
	}

	if (len < ETH_MIN_HEADER_LEN)
	{
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "Received invalid small packet length: %d\n\r", len);
		priv->netdev->stats.rx_dropped++;
		goto end;
	}
	send_rx_eth_pkt(priv, priv->eth_rx_buf, len, rxtimestamp);

end:
	priv->eth_rxlength = 0;
	priv->eth_rxinvalid = false;
}

void tc6_cb_onerror(struct tc6_t *pinst, enum tc6_error_t err, void *pglobaltag)
{
	struct lan865x_priv *priv = container_of(pinst, struct lan865x_priv, tc6);

	switch (err) {
	case tc6error_succeeded:
		netdev_info(priv->netdev, "no error occurred\n");
		break;
	case tc6error_nohardware:
		netdev_info(priv->netdev, "miso data implies that there is no macphy hardware available\n");
		break;
	case tc6error_unexpectedsv:
		netdev_info(priv->netdev, "unexpected start valid flag\n");
		break;
	case tc6error_unexpecteddvev:
		netdev_info(priv->netdev, "unexpected data valid or end valid flag\n");
		break;
	case tc6error_badchecksum:
		netdev_info(priv->netdev, "checksum in footer is wrong\n");
		break;
	case tc6error_unexpectedctrl:
		netdev_info(priv->netdev, "unexpected control packet received\n");
		break;
	case tc6error_badtxdata:
		netdev_info(priv->netdev, "header bad flag received\n");
		break;
	case tc6error_synclost:
		netdev_info(priv->netdev, "sync flag is no longer set\n");
		break;
	case tc6error_spierror:
		netdev_info(priv->netdev, "spi transaction failed\n");
		break;
	case tc6error_controltxfail:
		netdev_info(priv->netdev, "control tx failure\n");
		break;
	default:
		netdev_info(priv->netdev, "unknown tc6 error occurred=%d\n", err);
		break;
	}
}

static void lan865x_spi_transfer_complete(void *context)
{
	struct lan865x_priv *priv = context;

	priv->spibusy = false;
	mutex_lock(&priv->lock);
	tc6_spibufferdone(&priv->tc6, true);
	mutex_unlock(&priv->lock);
	if (priv->int_pending) {
		priv->int_pending = false;
		priv->irq_flag = true;
		wake_up_interruptible(&priv->irq_wq);
	}
}

bool tc6_cb_onspitransaction(struct tc6_t *pinst, u8 *ptx, u8 *prx, u16 len,
			     void *pglobaltag)
{
	struct lan865x_priv *priv = container_of(pinst, struct lan865x_priv, tc6);
	int status = 0;

	if (priv->spibusy)
		return false;

	priv->spibusy = true;
	spi_message_init(&priv->msg);

	priv->transfer.tx_nbits = 1; /* 1 mosi line */
	priv->transfer.rx_nbits = 1; /* 1 miso line */
	priv->transfer.speed_hz = 0; /* use device setting */
	priv->transfer.bits_per_word = 0; /* use device setting */
	priv->transfer.tx_buf = ptx;
	priv->transfer.rx_buf = prx;
	priv->transfer.delay.value = 0;
	priv->transfer.delay.unit = SPI_DELAY_UNIT_USECS;
	priv->transfer.cs_change = 0;
	priv->transfer.len = len;
	priv->msg.complete = lan865x_spi_transfer_complete;
	priv->msg.context = priv;

	spi_message_add_tail(&priv->transfer, &priv->msg);
	status = spi_async(priv->spi, &priv->msg);
	if (status < 0) {
		pr_err_ratelimited("lan865x:spy_async failed, status: %d", status);
		return false;
	}
	return true;
}

static void onclearstatus0(struct tc6_t *pinst, bool success, u32 addr, u32 value,
			   void *tag, void *pglobaltag)
{
	tc6_unlockextendedstatus(pinst);
}

void onstatus0(struct tc6_t *pinst, bool success, u32 addr, u32 value, void *ptag,
	       void *pglobaltag)
{
	struct lan865x_priv *priv = container_of(pinst, struct lan865x_priv, tc6);

	if (printk_ratelimit())
		netdev_info(priv->netdev, "EXT STS0: %x\n", value);

	if (success)
		tc6_writeregister(pinst, addr, value, onclearstatus0, NULL);
	else
		tc6_unlockextendedstatus(pinst);
}

void tc6_cb_onextendedstatus(struct tc6_t *pinst, void *pglobaltag)
{
	struct lan865x_priv *priv = container_of(pinst, struct lan865x_priv, tc6);

	tc6_readregister(pinst, REG_STDR_STATUS0, onstatus0, priv);
}

static bool readefusereg(struct lan865x_priv *priv, u32 addr, u32 *pval)
{
	u32 val = 0x0;
	bool success = true;

	success = tc6_write_register(priv, 0x000400d8, addr & 0x000f);
	if (success)
		success = tc6_write_register(priv, 0x000400da, 0x0002);
	mdelay(1);
	if (success)
		success = tc6_read_register(priv, 0x000400d9, &val);
	if (success)
		*pval = val;
	return success;
}

static bool writeregisterbits(struct lan865x_priv *priv, u32 addr, u8 start,
			      u8 end, u32 value)
{
	long wait_remaining;
	u32 mask = 0;
	u8 i;
	bool ret;

	for (i = start; i <= end; i++)
		mask |= (1 << i);

	reinit_completion(&priv->tc6_completion);
	ret = tc6_readmodifywriteregister(&priv->tc6, addr, (value << start),
					  mask, tc6_write_callback, priv);
	if (ret) {
		priv->ns_flag = true;
		wake_up_interruptible(&priv->ns_wq);
		wait_remaining = wait_for_completion_interruptible_timeout(&priv->tc6_completion,
									   msecs_to_jiffies(100));
		if ((wait_remaining == 0) || (wait_remaining == -ERESTARTSYS))
			ret = false;
		else
			ret = true;
	}
	if (!ret) {
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "spi write register failed for addr: %x", addr);
	}
	return ret;
}

static bool lan865x_setup_efuse(struct lan865x_priv *priv)
{
	s8 efuse_a4_offset = 0;
	s8 efuse_a8_offset = 0;
	bool success = true;
	u32 val = 0x0;

	if (success) {
		success = readefusereg(priv, 0x4, &val);
		if (success) {
			if ((val & (1 << 4)) == (1 << 4)) {
				/* negative value */
				efuse_a4_offset = val | 0xe0;
				if (efuse_a4_offset < -5)
					efuse_a4_offset = -5;
			} else {
				/* positive value */
				efuse_a4_offset = val;
			}
		}
	}
	if (success) {
		success = readefusereg(priv, 0x8, &val);
		if (success) {
			if ((val & (1 << 4)) == (1 << 4)) {
				/* negative value */
				efuse_a8_offset = val | 0xe0;
				if (efuse_a8_offset < -5)
					efuse_a8_offset = -5;
			} else {
				/* positive value */
				efuse_a8_offset = val;
			}
		}
	}
	if (success)
		success = writeregisterbits(priv, 0x00040084, 10, 15,
					    0x9 + efuse_a4_offset);
	if (success)
		success = writeregisterbits(priv, 0x00040084, 4, 9,
					    0xe + efuse_a4_offset);
	if (success)
		success = writeregisterbits(priv, 0x0004008a, 10, 15,
					    0x28 + efuse_a8_offset);
	if (success)
		success = writeregisterbits(priv, 0x000400ad, 8, 13,
					    0x5 + efuse_a4_offset);
	if (success)
		success = writeregisterbits(priv, 0x000400ad, 0, 5,
					    0x9 + efuse_a4_offset);
	if (success)
		success = writeregisterbits(priv, 0x000400ae, 8, 13,
					    0x9 + efuse_a4_offset);
	if (success)
		success = writeregisterbits(priv, 0x000400ae, 0, 5,
					    0xe + efuse_a4_offset);
	if (success)
		success = writeregisterbits(priv, 0x000400af, 8, 13,
					    0x11 + efuse_a4_offset);
	if (success)
		success = writeregisterbits(priv, 0x000400af, 0, 5,
					    0x16 + efuse_a4_offset);

	return success;
}

static bool lan865x_init(struct lan865x_priv *priv)
{
	u32 regval;
	bool success = true;

	tc6_init(&priv->tc6);
	if (success) {
		u32 i = 0;

		while (i < tc6_memmap_length) {
			i += tc6_multipleregisteraccess(&priv->tc6,
							&tc6_memmap[i],
							(tc6_memmap_length - i),
							tc6_memmap_callback, NULL);
			if (i != tc6_memmap_length)
				mdelay(1); /* avoid high cpu load */
		}
	}
	if (success) {
		u32 val = 0x0;
		bool success = readefusereg(priv, 0x5, &val);
		bool istrimmed = success && (0 != (val & 0x40));

		if (istrimmed) {
			dev_info(&priv->spi->dev, "phy is trimmed\r\n");
			success = lan865x_setup_efuse(priv);
		} else if (success) {
			dev_info(&priv->spi->dev, "phy is not trimmed!!\r\n");
		}
	}
	if (success) {
		if (priv->tx_cut_thr_mode) {
			success = tc6_write_register(priv, REG_STDR_CONFIG0,
						     TX_CUT_THROUGH_MODE);
			if (success)
				dev_info(&priv->spi->dev, "Tx cut through mode enabled\n");
		}
		else {
			success = tc6_write_register(priv, REG_STDR_CONFIG0,
						     TX_STORE_FWD_MODE);
			if (success)
				dev_info(&priv->spi->dev, "Store and forward mode enabled\n");
		}
	}
	if (success) {
		/* unmasking receive buffer overflow int*/
		success = tc6_write_register(priv, 0x0000000c, (1 << 3));
	}
	if (success) {
		if (priv->plca_enable) {
			dev_info(&priv->spi->dev, "plca nodeid=%d\n",
				 priv->plca_node_id);
			dev_info(&priv->spi->dev, "plca maxid=%d\n",
				 priv->plca_node_count);
			dev_info(&priv->spi->dev, "plca to-timer=%d\n",
				 priv->plca_to_timer);
			dev_info(&priv->spi->dev, "plca burst-cnt=%d\n",
				 priv->plca_burst_count);
			dev_info(&priv->spi->dev, "plca burst-timer=%d\n",
				 priv->plca_burst_timer);
			/* setting max id and local node id */
			regval = (priv->plca_node_count << 8) |
				 priv->plca_node_id;
			if (success)
				success = tc6_write_register(priv,
							     REG_PHY_PLCA_CTRL1,
							     regval);
			/* setting burst values */
			regval = (priv->plca_burst_count << 8) |
				 priv->plca_burst_timer;
			if (success)
				success = tc6_write_register(priv,
							     REG_PHY_PLCA_BURST,
							     regval);
			/* setting to-timer */
			if (success)
				success = tc6_write_register(priv,
							     REG_PHY_PLCA_TOTIME,
							     priv->plca_to_timer);
			/* enable plca */
			if (success)
				success = tc6_write_register(priv,
							     REG_PHY_PLCA_CTRL0,
							     PLCA_ENABLE);
		}
	}
	if (success) {
		success = tc6_read_register(priv, REG_STDR_STATUS0, &regval);
		if (success) {
			if (regval == 0x0 || regval == 0xffffffff)
				success = false;
		}
	}

	return success;
}

static int lan865x_set_hw_macaddr(struct net_device *netdev)
{
	u32 regval;
	bool ret;
	struct lan865x_priv *priv = netdev_priv(netdev);
	struct device *dev = &priv->spi->dev;
	u8 *mac = netdev->dev_addr;

	if (!priv->enable_data) {
		if (netif_msg_drv(priv))
			dev_info(dev, "%s: setting mac address to %pm",
				 netdev->name, netdev->dev_addr);
		/* mac address setting */
		regval = (mac[3] << 24) | (mac[2] << 16) | (mac[1] << 8) | mac[0];
		ret = tc6_write_register(priv, REG_MAC_ADDR_L, regval);
		if (ret) {
			regval = (mac[5] << 8) | mac[4];
			ret = tc6_write_register(priv, REG_MAC_ADDR_H, regval);
		}
		if (ret) {
			/* MAC address setting, setting unique lower MAC address, back off time is generated out of that */
			regval = (mac[5] << 24) | (mac[4] << 16) | (mac[3] << 8) | mac[2];
			ret = tc6_write_register(priv, REG_MAC_ADDR_BO, regval);
		}
		if (!ret) {
			dev_dbg(dev,
				"%s() failed to set mac address",
				   __func__);
			return -EIO;
		}
	} else {
		if (netif_msg_drv(priv))
			dev_dbg(dev,
				"%s() hardware must be disabled to set mac address",
				   __func__);
		return -EBUSY;
	}
	return 0;
}

static int ns_handler(void *data)
{
	struct lan865x_priv *priv = data;

	while (likely(!kthread_should_stop())) {
		wait_event_interruptible(priv->ns_wq,  priv->ns_flag ||
							kthread_should_stop());
		priv->ns_flag = false;
		mutex_lock(&priv->lock);
		tc6_service(&priv->tc6, true);
		mutex_unlock(&priv->lock);
	}
	return 0;
}

static int irq_handler(void *data)
{
	struct lan865x_priv *priv = data;

	while (likely(!kthread_should_stop())) {
		wait_event_interruptible(priv->irq_wq,  priv->irq_flag ||
							kthread_should_stop());
		priv->irq_flag = false;
		mutex_lock(&priv->lock);
		if (!tc6_service(&priv->tc6, false))
			priv->int_pending = true;
		mutex_unlock(&priv->lock);
	}
	return 0;
}

static irqreturn_t lan865x_irq(int irq, void *dev_id)
{
	struct lan865x_priv *priv = dev_id;

	if (priv->enable_data) {
		priv->irq_flag = true;
		wake_up_interruptible(&priv->irq_wq);
	}
	return IRQ_HANDLED;
}

void tc6_cb_onneedservice(struct tc6_t *pinst, void *pglobaltag)
{
	struct lan865x_priv *priv = container_of(pinst, struct lan865x_priv, tc6);

	priv->ns_flag = true;
	wake_up_interruptible(&priv->ns_wq);
}

static int lan865x_hw_enable(struct lan865x_priv *priv)
{
	if (!tc6_write_register(priv, REG_MAC_NW_CTRL, 0xc))
		return -EIO;
	tc6_enabledata(&priv->tc6, true);
	priv->enable_data = true;

	return 0;
}

static int lan865x_hw_disable(struct lan865x_priv *priv)
{
	tc6_enabledata(&priv->tc6, false);
	priv->enable_data = false;
	if (!tc6_write_register(priv, REG_MAC_NW_CTRL, 0x0))
		return -EIO;

	return 0;
}

static int
lan865x_set_link_ksettings(struct net_device *netdev,
			   const struct ethtool_link_ksettings *cmd)
{
	struct lan865x_priv *priv = netdev_priv(netdev);
	int ret = 0;

	if (!priv->enable_data) {
		if ((cmd->base.autoneg != AUTONEG_DISABLE) ||
		    (cmd->base.speed != SPEED_10) ||
		    (cmd->base.duplex != DUPLEX_HALF)) {
			if (netif_msg_link(priv))
				netdev_warn(netdev, "unsupported link setting");
			ret = -EOPNOTSUPP;
		}
	} else {
		if (netif_msg_link(priv))
			netdev_warn(netdev, "warning: hw must be disabled to set link mode");
		ret = -EBUSY;
	}
	return ret;
}

static int
lan865x_get_link_ksettings(struct net_device *netdev,
			   struct ethtool_link_ksettings *cmd)
{
	ethtool_link_ksettings_zero_link_mode(cmd, supported);
	ethtool_link_ksettings_add_link_mode(cmd, supported, 10baseT_Half);
	ethtool_link_ksettings_add_link_mode(cmd, supported, TP);

	cmd->base.speed = SPEED_10;
	cmd->base.duplex = DUPLEX_HALF;
	cmd->base.port	= PORT_TP;
	cmd->base.autoneg = AUTONEG_DISABLE;

	return 0;
}

static void lan865x_set_msglevel(struct net_device *netdev, u32 val)
{
	struct lan865x_priv *priv = netdev_priv(netdev);

	priv->msg_enable = val;
}

static u32 lan865x_get_msglevel(struct net_device *netdev)
{
	struct lan865x_priv *priv = netdev_priv(netdev);

	return priv->msg_enable;
}

static void
lan865x_get_drvinfo(struct net_device *netdev, struct ethtool_drvinfo *info)
{
	strscpy(info->driver, DRV_NAME, sizeof(info->driver));
	strscpy(info->version, DRV_VERSION, sizeof(info->version));
	strscpy(info->bus_info,
		dev_name(netdev->dev.parent), sizeof(info->bus_info));
}

static const struct ethtool_ops lan865x_ethtool_ops = {
	.get_drvinfo	= lan865x_get_drvinfo,
	.get_msglevel	= lan865x_get_msglevel,
	.set_msglevel	= lan865x_set_msglevel,
	.get_link_ksettings = lan865x_get_link_ksettings,
	.set_link_ksettings = lan865x_set_link_ksettings,
};

static int lan865x_set_mac_address(struct net_device *netdev, void *addr)
{
	struct sockaddr *address = addr;

	if (netif_running(netdev))
		return -EBUSY;
	if (!is_valid_ether_addr(address->sa_data))
		return -EADDRNOTAVAIL;

	ether_addr_copy(netdev->dev_addr, address->sa_data);
	return lan865x_set_hw_macaddr(netdev);
}

/* returns hash bit number for given mac address
 * example:
 * 01 00 5e 00 00 01 -> returns bit number 31
 */
static u32 lan865x_hash(u8 addr[ETH_ALEN])
{
	return (ether_crc(ETH_ALEN, addr) >> 26) & 0x3f;
}

static void lan865x_set_multicast_list(struct net_device *netdev)
{
	struct lan865x_priv *priv = netdev_priv(netdev);
	u32 regval = 0;

	if (netdev->flags & IFF_PROMISC) {
		/* enabling promiscuous mode */
		regval |= MAC_PROMISCUOUS_MODE;
		regval &= (~MAC_MULTICAST_MODE);
		regval &= (~MAC_UNICAST_MODE);
	} else if (netdev->flags & IFF_ALLMULTI) {
		/* enabling all multicast mode */
		regval &= (~MAC_PROMISCUOUS_MODE);
		regval |= MAC_MULTICAST_MODE;
		regval &= (~MAC_UNICAST_MODE);
	} else if (!netdev_mc_empty(netdev)) {
		/* enabling specific multicast addresses */
		struct netdev_hw_addr *ha;
		u32 hash_lo = 0;
		u32 hash_hi = 0;

		netdev_for_each_mc_addr(ha, netdev) {
			u32 bit_num = lan865x_hash(ha->addr);
			u32 mask = 1 << (bit_num & 0x1f);

			if (bit_num & 0x20)
				hash_hi |= mask;
			else
				hash_lo |= mask;
		}
		if (!tc6_writeregister(&priv->tc6, REG_MAC_HASHH, hash_hi, NULL, NULL)) {
			if (netif_msg_timer(priv))
				netdev_err(netdev, "failed to write reg_hashh");
		}
		if (!tc6_writeregister(&priv->tc6, REG_MAC_HASHL, hash_lo, NULL, NULL)) {
			if (netif_msg_timer(priv))
				netdev_err(netdev, "failed to write reg_hashl");
		}
		regval &= (~MAC_PROMISCUOUS_MODE);
		regval &= (~MAC_MULTICAST_MODE);
		regval |= MAC_UNICAST_MODE;
	} else {
		/* enabling local mac address only */
		if (!tc6_writeregister(&priv->tc6, REG_MAC_HASHH, 0, NULL, NULL)) {
			if (netif_msg_timer(priv))
				netdev_err(netdev, "failed to write reg_hashh");
		}
		if (!tc6_writeregister(&priv->tc6, REG_MAC_HASHL, 0, NULL, NULL)) {
			if (netif_msg_timer(priv))
				netdev_err(netdev, "failed to write reg_hashl");
		}
		regval &= (~MAC_PROMISCUOUS_MODE);
		regval &= (~MAC_MULTICAST_MODE);
		regval &= (~MAC_UNICAST_MODE);
	}
	if (!tc6_writeregister(&priv->tc6, REG_MAC_NW_CONFIG, regval, NULL, NULL)) {
		if (netif_msg_timer(priv))
			netdev_err(netdev, "failed to enable promiscuous mode");
	}
}

void onrawtx(struct tc6_t *pinst, const u8 *ptx, u16 len, void *ptag, void *pglobaltag)
{
	struct lan865x_priv *priv = container_of(pinst, struct lan865x_priv, tc6);
	struct sk_buff *tx_skb = ptag;

	priv->netdev->stats.tx_packets++;
	priv->netdev->stats.tx_bytes += len;
	dev_kfree_skb(tx_skb);
	if (netif_queue_stopped(priv->netdev))
		netif_wake_queue(priv->netdev);
}

static netdev_tx_t lan865x_send_packet(struct sk_buff *skb,
				       struct net_device *netdev)
{
	struct lan865x_priv *priv = netdev_priv(netdev);
	bool ret;

	ret = tc6_sendrawethernetpacket(&priv->tc6, skb->data, skb->len, 0, onrawtx, skb);
	if (!ret) {
		netif_stop_queue(priv->netdev);
		return NETDEV_TX_BUSY;
	}
	return NETDEV_TX_OK;
}

static int lan865x_net_close(struct net_device *netdev)
{
	struct lan865x_priv *priv = netdev_priv(netdev);

	if (lan865x_hw_disable(priv) != 0) {
		if (netif_msg_ifup(priv))
			netdev_err(netdev, "lan865x_hw_disable() failed");
		return -EIO;
	}
	netif_stop_queue(netdev);

	return 0;
}

static int lan865x_net_open(struct net_device *netdev)
{
	struct lan865x_priv *priv = netdev_priv(netdev);
	int ret;

	if (!is_valid_ether_addr(netdev->dev_addr)) {
		if (netif_msg_ifup(priv))
			netdev_err(netdev, "invalid mac address %pm", netdev->dev_addr);
		return -EADDRNOTAVAIL;
	}
	if (lan865x_hw_disable(priv) != 0) {
		if (netif_msg_ifup(priv))
			netdev_err(netdev, "lan865x_hw_disable() failed");
		return -EIO;
	}
	ret = lan865x_set_hw_macaddr(netdev);
	if (ret != 0)
		return ret;
	if (lan865x_hw_enable(priv) != 0) {
		if (netif_msg_ifup(priv))
			netdev_err(netdev, "lan865x_hw_enable() failed");
		return -EIO;
	}
	netif_start_queue(netdev);

	return 0;
}

static void lan865x_tx_timeout(struct net_device *netdev, unsigned int txqueue)
{
	struct lan865x_priv *priv = netdev_priv(netdev);

	if (netif_msg_timer(priv)) {
		if (printk_ratelimit())
			netdev_err(netdev, "tx timeout");
	}
	netdev->stats.tx_errors++;
}

static const struct net_device_ops lan865x_netdev_ops = {
	.ndo_open		= lan865x_net_open,
	.ndo_stop		= lan865x_net_close,
	.ndo_start_xmit		= lan865x_send_packet,
	.ndo_set_rx_mode	= lan865x_set_multicast_list,
	.ndo_set_mac_address	= lan865x_set_mac_address,
	.ndo_tx_timeout		= lan865x_tx_timeout,
	.ndo_validate_addr	= eth_validate_addr,
};

#ifdef CONFIG_OF
static int lan865x_get_dt_data(struct lan865x_priv *priv)
{
	struct spi_device *spi = priv->spi;
	int ret;

	if (!spi->dev.of_node)
		return -EINVAL;

	ret = of_property_read_u8(spi->dev.of_node, "plca-enable", &priv->plca_enable);
	if (ret < 0) {
		dev_err(&spi->dev, "plca-enable property is not found in device tree");
		return ret;
	}
	if (priv->plca_enable > 1) {
		dev_err(&spi->dev, "bad value in plca-enable property");
		return -EINVAL;
	}
	if (priv->plca_enable) {
		ret = of_property_read_u8(spi->dev.of_node, "plca-node-id", &priv->plca_node_id);
		if (ret < 0) {
			dev_err(&spi->dev, "plca-node-id property is not found in device tree");
			return ret;
		}
		if (priv->plca_node_id > 254) {
			dev_err(&spi->dev, "bad value in plca-node-id property");
			return -EINVAL;
		}
		if (priv->plca_node_id == 0) {
			ret = of_property_read_u8(spi->dev.of_node,
						  "plca-node-count",
						  &priv->plca_node_count);
			if (ret < 0) {
				dev_err(&spi->dev, "plca-node-count property is not found in device tree");
				return ret;
			}
			if (priv->plca_node_count < 1) {
				dev_err(&spi->dev, "bad value in plca-node-count property");
				return -EINVAL;
			}
		}
		ret = of_property_read_u8(spi->dev.of_node, "plca-burst-count", &priv->plca_burst_count);
		if (ret < 0) {
			dev_err(&spi->dev, "plca-burst-count property is not found in device tree");
			return ret;
		}
		ret = of_property_read_u8(spi->dev.of_node, "plca-burst-timer", &priv->plca_burst_timer);
		if (ret < 0) {
			dev_err(&spi->dev, "plca-burst-timer property is not found in device tree");
			return ret;
		}
		ret = of_property_read_u8(spi->dev.of_node, "plca-to-timer", &priv->plca_to_timer);
		if (ret < 0) {
			dev_err(&spi->dev, "plca-to-timer property is not found in device tree");
			return ret;
		}
	}
	ret = of_property_read_u8(spi->dev.of_node, "tx-cut-through-mode", &priv->tx_cut_thr_mode);
	if (ret < 0) {
		dev_err(&spi->dev, "tx-cut-through-mode property is not found in device tree");
		return ret;
	}
	if (priv->tx_cut_thr_mode > 1) {
		dev_err(&spi->dev, "bad value in tx-cut-through-mode property");
		return -EINVAL;
	}
	ret = of_property_read_u8(spi->dev.of_node, "rx-cut-through-mode", &priv->rx_cut_thr_mode);
	if (ret < 0) {
		dev_err(&spi->dev, "rx-cut-through-mode property is not found in device tree");
		return ret;
	}
	if (priv->rx_cut_thr_mode > 1) {
		dev_err(&spi->dev, "bad value in rx-cut-through-mode property");
		return -EINVAL;
	}
	return 0;
}
#else
static int lan865x_get_dt_data(struct lan865x_priv *priv)
{
	return -EINVAL;
}
#endif

#ifdef CONFIG_ACPI
static int lan865x_get_acpi_data(struct lan865x_priv *priv)
{
	priv->plca_enable = 1;
	priv->plca_node_id = 0;
	priv->plca_node_count = 8;
	priv->plca_burst_count = PLCA_BURST_CNT_VAL;
	priv->plca_burst_timer = PLCA_BURST_TMR_VAL;
	priv->plca_to_timer = PLCA_TO_TIMER_VAL;
	priv->tx_cut_thr_mode = 1;
	priv->rx_cut_thr_mode = 1;

	return 0;
}
#else
static int lan865x_get_acpi_data(struct lan865x_priv *priv)
{
	return -EINVAL;
}
#endif

static int lan865x_probe(struct spi_device *spi)
{
	struct lan865x_priv *priv;
	struct net_device *netdev;
	unsigned char macaddr[ETH_ALEN];
	int ret;

	netdev = alloc_etherdev(sizeof(struct lan865x_priv));
	if (!netdev) {
		ret = -ENOMEM;
		goto error_alloc;
	}
	priv = netdev_priv(netdev);
	priv->netdev = netdev;	/* priv to netdev reference */
	priv->spi = spi;	/* priv to spi reference */
	priv->msg_enable = netif_msg_init(debug.msg_enable, LAN865X_MSG_DEFAULT);
	mutex_init(&priv->lock);
	spi_set_drvdata(spi, priv);	/* spi to priv reference */
	SET_NETDEV_DEV(netdev, &spi->dev);

	init_completion(&priv->tc6_completion);
	init_waitqueue_head(&priv->irq_wq);
	init_waitqueue_head(&priv->ns_wq);

#ifdef CONFIG_OF
	ret = lan865x_get_dt_data(priv);
#endif
#ifdef CONFIG_ACPI
	ret = lan865x_get_acpi_data(priv);
#endif
	if (ret) {
		if (netif_msg_probe(priv))
			dev_err(&spi->dev, "no platform data for lan865x");
		ret = -ENODEV;
		goto error_platform_data;
	}

	ret = devm_request_irq(&spi->dev, spi->irq, lan865x_irq, 0, "tc6 int", priv);
        if ((ret != -ENOTCONN) && (ret < 0)) {
                dev_err(&spi->dev, "error attaching irq %d\n", ret);
                goto error_request_irq;
        }

	priv->irq_task = kthread_run(irq_handler, priv, "irq_task");
	if (IS_ERR(priv->irq_task)) {
		ret = PTR_ERR(priv->irq_task);
		goto error_irq_task;
	}
	sched_set_fifo(priv->irq_task);

	priv->ns_task = kthread_run(ns_handler, priv, "ns_task");
	if (IS_ERR(priv->ns_task)) {
		ret = PTR_ERR(priv->ns_task);
		goto error_ns_task;
	}
	sched_set_fifo(priv->ns_task);

	spi->rt = true;
	spi_setup(spi);

	ret = lan865x_init(priv);
	if (!ret) {
		if (netif_msg_probe(priv))
			dev_err(&spi->dev, "lan865x init failed, hardware not found");
		ret = -ENODEV;
		goto error_init;
	}

	if (device_get_mac_address(&spi->dev, macaddr, sizeof(macaddr)))
		ether_addr_copy(netdev->dev_addr, macaddr);
	else
		eth_hw_addr_random(netdev);

	ret = lan865x_set_hw_macaddr(netdev);
	if (ret) {
		if (netif_msg_probe(priv))
			dev_err(&spi->dev, "lan865x mac addr config failed");
		ret = -EIO;
		goto error_set_mac;
	}

	netdev->if_port = IF_PORT_10BASET;
	netdev->irq = spi->irq;
	netdev->netdev_ops = &lan865x_netdev_ops;
	netdev->watchdog_timeo = TX_TIMEOUT;
	netdev->ethtool_ops = &lan865x_ethtool_ops;
	ret = register_netdev(netdev);
	if (ret) {
		if (netif_msg_probe(priv))
			dev_err(&spi->dev, "register netdev failed (ret = %d)",
				ret);
		goto error_netdev_register;
	}

	if (priv->plca_enable)
		netdev_info(netdev, "plca mode is active");
	else
		netdev_info(netdev, "csma/cd mode is active");

	return 0;

error_netdev_register:
error_set_mac:
	devm_free_irq(&spi->dev, spi->irq, priv);
error_init:
error_request_irq:
	kthread_stop(priv->ns_task);
error_ns_task:
	kthread_stop(priv->irq_task);
error_irq_task:
	del_timer(&priv->tc6_timer);
error_platform_data:
	free_netdev(netdev);
error_alloc:
	return ret;
}

static int lan865x_remove(struct spi_device *spi)
{
	struct lan865x_priv *priv = spi_get_drvdata(spi);

	unregister_netdev(priv->netdev);
	if (spi->irq > 0)
		devm_free_irq(&spi->dev, spi->irq, priv);
	kthread_stop(priv->irq_task);
	kthread_stop(priv->ns_task);
	free_netdev(priv->netdev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lan865x_dt_ids[] = {
	{ .compatible = "microchip,lan865x" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lan865x_dt_ids);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id lan865x_acpi_ids[] = {
	{ .id = "LAN865X",
	},
	{},
};
MODULE_DEVICE_TABLE(acpi, lan865x_acpi_ids);
#endif

static struct spi_driver lan865x_driver = {
	.driver = {
		.name = DRV_NAME,
#ifdef CONFIG_OF
		.of_match_table = lan865x_dt_ids,
#endif
#ifdef CONFIG_ACPI
		   .acpi_match_table = ACPI_PTR(lan865x_acpi_ids),
#endif
	 },
	.probe = lan865x_probe,
	.remove = lan865x_remove,
};
module_spi_driver(lan865x_driver);

MODULE_DESCRIPTION(DRV_NAME " 10Base-T1S MACPHY Ethernet Driver");
MODULE_AUTHOR("Parthiban Veerasooran <parthiban.veerasooran@microchip.com>");
MODULE_AUTHOR("Thorsten Kummermehr <thorsten.kummermehr@microchip.com>");
MODULE_LICENSE("GPL");
module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level in amount of bits set (0=none, ..., 31=all)");
MODULE_ALIAS("spi:" DRV_NAME);
