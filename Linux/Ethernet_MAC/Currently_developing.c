// SPDX-License-Identifier: GPL-2.0-only
/*
 * BL702 EMAC Ethernet Controller driver
 *
 * Copyright (C) 2024 Your Company / Your Name
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/phy.h> // For phylink and MII operations
#include <linux/of.h> // For device tree parsing
#include <linux/of_net.h> // For device tree MAC address
#include <linux/gpio/consumer.h> // For GPIOs if needed for reset/power
#include <linux/clk.h> // For clock management
#include <linux/jiffies.h> // For mdelay/udelay
#include <linux/iopoll.h> // For readx_poll_timeout

// Include our specific register definitions (assuming these are in a kernel-accessible path)
#include "emac_reg.h" // Contains EMAC_MODE_OFFSET, EMAC_DMA_DESC_OFFSET, EMAC_BD_TX_RD, etc.
#include "bflb_emac.h" // Contains EMAC_TX_BD_BUM_MAX, EMAC_RX_BD_BUM_MAX etc.
// NOTE: In a real driver, you'd likely integrate these directly or create a bl702_emac_regs.h
// and bl702_emac_hw.h specific for the kernel driver.

#define DRV_NAME "bl702-emac"
#define DRV_VERSION "0.1.0"

// --- Driver Private Data Structure ---
struct bl702_emac_priv {
    struct net_device *netdev;
    struct device *dev;
    void __iomem *base_addr; // Memory-mapped EMAC peripheral base address

    struct clk *emac_clk; // EMAC core clock
    struct clk *mii_clk;  // MII/MDIO clock
    unsigned int tx_irq_countdown;// packet counter
    ktime_t last_tx_irq_time; // last irq time
    // TX Ring
    struct sk_buff *tx_skb[EMAC_TX_BD_BUM_MAX]; // Array to hold skb pointers
    dma_addr_t tx_skb_dma_addr[EMAC_TX_BD_BUM_MAX]; // DMA addresses of skbs
    unsigned int tx_head; // Next BD to hand to hardware
    unsigned int tx_tail; // Next BD to free after hardware done
    size_t tx_skb_dma_len[EMAC_TX_BD_BUM_MAX];// To hold mapping length for unmapping

    // RX Ring
    struct sk_buff *rx_skb[EMAC_RX_BD_BUM_MAX]; // Array to hold skb pointers
    dma_addr_t rx_skb_dma_addr[EMAC_RX_BD_BUM_MAX]; // DMA addresses of skbs
    unsigned int rx_head; // Next BD to hand to software
    unsigned int rx_tail; // Next BD to fill for hardware
    size_t rx_skb_dma_len[EMAC_RX_BD_BUM_MAX];// To hold mapping length for unmapping

    // NAPI
    struct napi_struct napi;

    // PHY Link
    struct phylink *phylink;
    struct phylink_config phylink_config;
    unsigned int link_an_modstatic bool bl702_rx_pending(struct bl702_emac_priv *priv)
{
	unsigned int entry;
	u32 status;
	entry = NEXT_INDEX(priv->rx_tail, EMAC_RX_BD_BUM_MAX);
	status = bl702_emac_read_bd_word(priv, entry, false, 0)
	
	rmb();  // Make sure DMA writes are visible

	return (status & ~EMAC_BD_RX_E_MASK);  // Adjust this based on your descriptor
}
e;
    int link_speed;
    int link_duplex;
    bool link_is_up;

    // MAC Address
    u8 mac_addr[ETH_ALEN];
};

// --- Helper Functions for Register Access ---

static inline u32 bl702_emac_readl(struct bl702_emac_priv *priv, unsigned long offset)
{
    return readl(priv->base_addr + offset);
}

static inline void bl702_emac_writel(struct bl702_emac_priv *priv, u32 val, unsigned long offset)
{
    writel(val, priv->base_addr + offset);
}

// Helper to write to internal BD
static inline void bl702_emac_write_bd_word(struct bl702_emac_priv *priv, unsigned int bd_idx,
                                             bool is_tx, unsigned int word_offset, u32 val)
{
    unsigned long bd_base_offset = EMAC_DMA_DESC_OFFSET;
    unsigned long offset;

    if (!is_tx) {
        // RX BDs come after TX BDs in the internal memory
        bd_base_offset += (EMAC_TX_BD_BUM_MAX * 8); // Each BD is 8 bytes
    }

    offset = bd_base_offset + (bd_idx * 8) + word_offset; // word_offset is 0 for word0, 4 for word1
    bl702_emac_writel(priv, val, offset);
}

// Helper to read from internal BD
static inline u32 bl702_emac_read_bd_word(struct bl702_emac_priv *priv, unsigned int bd_idx,
                                            bool is_tx, unsigned int word_offset)
{
    unsigned long bd_base_offset = EMAC_DMA_DESC_OFFSET;
    unsigned long offset;

    if (!is_tx) {
        bd_base_offset += (EMAC_TX_BD_BUM_MAX * 8);
    }

    offset = bd_base_offset + (bd_idx * 8) + word_offset;
    return bl702_emac_readl(priv, offset);
}


// --- PHY/MII/MDIO Helper Functions ---
// These are simplified and assume phylink handles much of the complexity.

static int bl702_emac_mdio_read(struct bl702_emac_priv *priv, int phy_addr, int reg_addr)
{
    u32 regval;
    int ret;

    // Set MII address and command (read)
    regval = FIELD_PREP(EMAC_FIAD_MASK, phy_addr) |
             FIELD_PREP(EMAC_RGAD_MASK, reg_addr);
    bl702_emac_writel(priv, regval, EMAC_MIIADDRESS_OFFSET);

    bl702_emac_writel(priv, EMAC_RSTAT, EMAC_MIICOMMAND_OFFSET); // Start read

    // Wait for MII busy to clear
    ret = readx_poll_timeout(bl702_emac_readl, priv, regval, !(regval & EMAC_MIIM_BUSY),
                             10, 100000, EMAC_MIISTATUS_OFFSET); // Poll every 10us, timeout 100ms
    if (ret) {
        dev_err(priv->dev, "MDIO read timeout!\n");
        return ret;
    }

    // Read data
    regval = bl702_emac_readl(priv, EMAC_MIIRX_DATA_OFFSET);
    return FIELD_GET(EMAC_PRSD_MASK, regval);
}

static int bl702_emac_mdio_write(struct bl702_emac_priv *priv, int phy_addr, int reg_addr, u16 val)
{
    u32 regval;
    int ret;

    // Set MII address
    regval = FIELD_PREP(EMAC_FIAD_MASK, phy_addr) |
             FIELD_PREP(EMAC_RGAD_MASK, reg_addr);
    bl702_emac_writel(priv, regval, EMAC_MIIADDRESS_OFFSET);

    // Set data to write
    regval = FIELD_PREP(EMAC_CTRLDATA_MASK, val);
    bl702_emac_writel(priv, regval, EMAC_MIITX_DATA_OFFSET);

    bl702_emac_writel(priv->base_addr, EMAC_WCTRLDATA, EMAC_MIICOMMAND_OFFSET); // Start write

    // Wait for MII busy to clear
    ret = readx_poll_timeout(bl702_emac_readl, priv, regval, !(regval & EMAC_MIIM_BUSY),
                             10, 100000, EMAC_MIISTATUS_OFFSET);
    if (ret) {
        dev_err(priv->dev, "MDIO write timeout!\n");
    }
    return ret;
}

// phylink callbacks
static void bl702_emac_mac_config(struct phylink_config *config, unsigned int mode,
                                   const struct phylink_link_state *state)
{
    struct net_device *netdev = to_net_dev(config->dev);
    struct bl702_emac_priv *priv = netdev_priv(netdev);
    u32 mode_reg;

    mode_reg = bl702_emac_readl(priv, EMAC_MODE_OFFSET);

    // Clear previous settings
    mode_reg &= ~(EMAC_FULLD | EMAC_RMII_EN);

    // Apply duplex
    if (state->duplex == DUPLEX_FULL) {
        mode_reg |= EMAC_FULLD;
    }

    // Apply interface mode (RMII)
    // Assuming RMII is the only supported mode for BL702 from the registers.
    // If MII is also supported, need to differentiate.
    mode_reg |= EMAC_RMII_EN;

    // Apply speed (if relevant for MAC side, BL702 seems to just handle MII/RMII mode)
    // For BL702, speed settings are mostly PHY-side or implicitly handled by MII/RMII mode.
    // The EMAC_MODE register doesn't have explicit 10/100M speed bits.
    // If there were EMAC_CMD_SET_SPEED_100M/10M in bflb_emac.h, we'd use them.
    priv->link_speed = state->speed;
    priv->link_duplex = state->duplex;

    bl702_emac_writel(priv, mode_reg, EMAC_MODE_OFFSET);

    dev_info(priv->dev, "MAC configured: speed %d, duplex %s, mode %s\n",
             state->speed, state->duplex == DUPLEX_FULL ? "full" : "half",
             state->interface == PHY_INTERFACE_MODE_RMII ? "RMII" : "Unknown");
}

static void bl702_emac_mac_link_state(struct phylink_config *config,
                                       const struct phylink_link_state *state)
{
    struct net_device *netdev = to_net_dev(config->dev);
    struct bl702_emac_priv *priv = netdev_priv(netdev);

    // Update link status
    if (state->link && !priv->link_is_up) {
        pr_info("%s: Link is UP - %s/%s\n", netdev->name,
                phy_speed_to_str(state->speed),
                phy_duplex_to_str(state->duplex));
        netif_start_queue(netdev);
        priv->link_is_up = true;
    } else if (!state->link && priv->link_is_up) {
        pr_info("%s: Link is DOWN\n", netdev->name);
        netif_stop_queue(netdev);
        priv->link_is_up = false;
    }
}

static const struct phylink_mac_ops bl702_emac_phylink_ops = {
    .mac_config = bl702_emac_mac_config,
    .mac_link_state = bl702_emac_mac_link_state,
    // .mac_an_restart, .mac_link_up, .mac_link_down could be added if needed
};

// --- EMAC Core Operations ---

static void bl702_emac_init_hw(struct bl702_emac_priv *priv)
{
    u32 regval;
    int i;

    // Configure EMAC Mode 
    // Enable CRC, PAD, Huge Frame, Receive Small Frame
    regval = EMAC_CRCEN | EMAC_PAD | EMAC_HUGEN | EMAC_RECSMALL;
    // Set MII mode by default
    regval |= ~EMAC_RMII_EN;
    // Enable broadcast by default
    regval |= EMAC_BRO;
    bl702_emac_writel(priv, regval, EMAC_MODE_OFFSET);

    // Set MAC Address
    // MAC_ADDR0 (bytes 2,3,4,5)
    regval = priv->mac_addr[2] |
             (priv->mac_addr[3] << 8) |
             (priv->mac_addr[4] << 16) |
             (priv->mac_addr[5] << 24);
    bl702_emac_writel(priv, regval, EMAC_MAC_ADDR0_OFFSET);

    // MAC_ADDR1 (bytes 0,1)
    regval = priv->mac_addr[0] |
             (priv->mac_addr[1] << 8);
    bl702_emac_writel(priv, regval, EMAC_MAC_ADDR1_OFFSET);

    // Configure Packet Lengths
    // Max/Min frame length
    //regval = FIELD_PREP(EMAC_MAXFL_MASK, priv->netdev->mtu + ETH_HLEN + ETH_FCS_LEN); // MTU + Eth Header + FCS
    //regval |= FIELD_PREP(EMAC_MINFL_MASK, ETH_ZLEN); // Minimum Ethernet frame length
    regval = FIELD_PREP(EMAC_MAXFL_MASK, ETH_MAXFL); // Maximum Ethernet frame length
    regval |= FIELD_PREP(EMAC_MINFL_MASK, ETH_MINFL); // Minimum Ethernet frame length

    bl702_emac_writel(priv, regval, EMAC_PACKETLEN_OFFSET);

    // 5. MII Clock Divider (for MDIO speed)
    // This value needs to be tuned based on actual clock.
    regval = FIELD_PREP(EMAC_CLKDIV_MASK, 10);
    bl702_emac_writel(priv, regval, EMAC_MIIMODE_OFFSET);

    // 6. Initialize TX Descriptors (in EMAC's internal memory)
    for (i = 0; i < EMAC_TX_BD_BUM_MAX; i++) {
        // Clear descriptor (Word 0 & Word 1)
        bl702_emac_write_bd_word(priv, i, true, 0, 0); // attribute + length word
        bl702_emac_write_bd_word(priv, i, true, 4, 0); // address word

    /*    // Allocate skb for TX (will be populated just before transmit)
        priv->tx_skb[i] = netdev_alloc_skb(priv->netdev, priv->netdev->mtu + ETH_HLEN + ETH_FCS_LEN);
        if (!priv->tx_skb[i]) {
            dev_err(priv->dev, "Failed to allocate TX skb %d\n", i);
            // Handle error: free previously allocated skbs and exit
            goto err_tx_skb;
        }
        // Map the SKB data buffer for DMA
        priv->tx_skb_dma_addr[i] = dma_map_single(priv->dev, priv->tx_skb[i]->data,
                                                   priv->netdev->mtu + ETH_HLEN + ETH_FCS_LEN,
                                                   DMA_TO_DEVICE);
        if (dma_mapping_error(priv->dev, priv->tx_skb_dma_addr[i])) {
            dev_err(priv->dev, "Failed to map TX skb %d for DMA\n", i);
            kfree_skb(priv->tx_skb[i]);
            priv->tx_skb[i] = NULL;
            // Handle error
            goto err_tx_dma;
        }
        // Write the DMA address to the internal BD (Word 1)
        bl702_emac_write_bd_word(priv, i, true, 4, priv->tx_skb_dma_addr[i]);
	*/
        // Set Wrap bit for the last BD
        if (i == (EMAC_TX_BD_BUM_MAX - 1)) {
            u32 attr_len_word = bl702_emac_read_bd_word(priv, i, true, 0);
            attr_len_word |= EMAC_BD_TX_WR;
            bl702_emac_write_bd_word(priv, i, true, 0, attr_len_word);
        }
    }
    priv->tx_head = 0;
    priv->tx_tail = 0;

    // 7. Initialize RX Descriptors (in EMAC's internal memory)
    for (i = 0; i < EMAC_RX_BD_BUM_MAX; i++) {
        // Clear descriptor (Word 0 & Word 1)
        bl702_emac_write_bd_word(priv, i, false, 0, 0); // attribute + length word
        bl702_emac_write_bd_word(priv, i, false, 4, 0); // address word

        // Allocate skb for RX
        priv->rx_skb[i] = netdev_alloc_skb(priv->netdev, priv->netdev->mtu + ETH_HLEN + ETH_FCS_LEN + NET_IP_ALIGN);
        if (!priv->rx_skb[i]) {
            dev_err(priv->dev, "Failed to allocate RX skb %d\n", i);
            // Handle error: free previously allocated skbs and exit
            goto err_rx_skb;
        }
        skb_reserve(priv->rx_skb[i], NET_IP_ALIGN); // Align data for IP header
	priv->rx_skb_dma_len[i] = priv->netdev->mtu + ETH_HLEN + ETH_FCS_LEN;
        // Map the SKB data buffer for DMA
        priv->rx_skb_dma_addr[i] = dma_map_single(priv->dev, priv->rx_skb[i]->data,
                                                   priv->rx_skb_dma_len[i],
                                                   DMA_FROM_DEVICE);
        if (dma_mapping_error(priv->dev, priv->rx_skb_dma_addr[i])) {
            dev_err(priv->dev, "Failed to map RX skb %d for DMA\n", i);
            kfree_skb(priv->rx_skb[i]);
            priv->rx_skb[i] = NULL;
            // Handle error
            goto err_rx_dma;
        }

        // Write the DMA address to the internal BD (Word 1)
        bl702_emac_write_bd_word(priv, i, false, 4, priv->rx_skb_dma_addr[i]);

        // Set attribute: Empty, Interrupt, and Wrap for last BD
        u32 attr_len_word = EMAC_BD_RX_E | EMAC_BD_RX_IRQ;
        if (i == (EMAC_RX_BD_BUM_MAX - 1)) {
            attr_len_word |= EMAC_BD_RX_WR;
        }
        bl702_emac_write_bd_word(priv, i, false, 0, attr_len_word);
    }
    priv->rx_head = 0;
    priv->rx_tail = 0;

    // 8. Enable TX/RX
    // Don't enable yet, will be done in netdev_open after phylink config.
    return;

err_rx_dma:
    for (i--; i >= 0; i--) {
        if (priv->rx_skb[i]) {
            dma_unmap_single(priv->dev, priv->rx_skb_dma_addr[i],
                             priv->netdev->mtu + ETH_HLEN + ETH_FCS_LEN, DMA_FROM_DEVICE);
            kfree_skb(priv->rx_skb[i]);
        }
    }
err_rx_skb:
err_tx_dma:
    for (i = EMAC_TX_BD_BUM_MAX - 1; i >= 0; i--) {
        if (priv->tx_skb[i]) {
            dma_unmap_single(priv->dev, priv->tx_skb_dma_addr[i],
                             priv->netdev->mtu + ETH_HLEN + ETH_FCS_LEN, DMA_TO_DEVICE);
            kfree_skb(priv->tx_skb[i]);
        }
    }
err_tx_skb:
    // More robust cleanup needed here for a production driver
    return; // Indicate failure
}

static void bl702_emac_deinit_hw(struct bl702_emac_priv *priv)
{
    int i;
    // Disable EMAC TX/RX
    u32 regval = bl702_emac_readl(priv, EMAC_MODE_OFFSET);
    regval &= ~(EMAC_TX_EN | EMAC_RX_EN);
    bl702_emac_writel(priv, regval, EMAC_MODE_OFFSET);

    // Clear interrupts (optional, but good practice)
    bl702_emac_writel(priv, 0xFFFFFFFF, EMAC_INT_SOURCE_OFFSET); // Clear all pending

    // Unmap and free SKBs and DMA resources
    for (i = 0; i < EMAC_TX_BD_BUM_MAX; i++) {
        if (priv->tx_skb[i]) {
            dma_unmap_single(priv->dev, priv->tx_skb_dma_addr[i],
                             priv->netdev->mtu + ETH_HLEN + ETH_FCS_LEN, DMA_TO_DEVICE);
            kfree_skb(priv->tx_skb[i]);
            priv->tx_skb[i] = NULL;
        }
    }
    for (i = 0; i < EMAC_RX_BD_BUM_MAX; i++) {
        if (priv->rx_skb[i]) {
            dma_unmap_single(priv->dev, priv->rx_skb_dma_addr[i],
                             priv->netdev->mtu + ETH_HLEN + ETH_FCS_LEN, DMA_FROM_DEVICE);
            kfree_skb(priv->rx_skb[i]);
            priv->rx_skb[i] = NULL;
        }
    }
}

// --- Netdev Operations ---

static int bl702_emac_open(struct net_device *netdev)
{
    struct bl702_emac_priv *priv = netdev_priv(netdev);
    int ret;
    u32 regval;

    bl702_emac_init_hw(priv);

    // 2. Request IRQ
    ret = request_irq(netdev->irq, bl702_emac_irq, 0, netdev->name, netdev);
    if (ret) {
        dev_err(priv->dev, "Failed to request IRQ %d\n", netdev->irq);
        bl702_emac_deinit_hw(priv);
        clk_disable_unprepare(priv->mii_clk);
        clk_disable_unprepare(priv->emac_clk);
        return ret;
    }

    // 3. Enable NAPI
    napi_enable(&priv->napi);

    // 5. Enable EMAC TX/RX
    regval = bl702_emac_readl(priv, EMAC_MODE_OFFSET);
    regval |= (EMAC_TX_EN | EMAC_RX_EN);
    bl702_emac_writel(priv, regval, EMAC_MODE_OFFSET);

    // 6. Enable all EMAC interrupts (TXB, TXE, RXB, RXE, BUSY, TXC, RXC)
    // Mask in the INT_MASK register means "enable"
    bl702_emac_writel(priv, EMAC_TXB_M | EMAC_TXE_M | EMAC_RXB_M | EMAC_RXE_M |
                              EMAC_BUSY_M | EMAC_TXC_M | EMAC_RXC_M,
                      EMAC_INT_MASK_OFFSET);

    // 7. Start the network queue
    netif_start_queue(netdev); // netif_start_queue can be deferred until link up by phylink_mac_link_state

    return 0;
}

static int bl702_emac_stop(struct net_device *netdev)
{
    struct bl702_emac_priv *priv = netdev_priv(netdev);
    u32 regval;

    // 1. Stop the network queue
    netif_stop_queue(netdev);

    // 2. Disable all EMAC interrupts
    bl702_emac_writel(priv, 0, EMAC_INT_MASK_OFFSET);

    // 3. Disable EMAC TX/RX
    regval = bl702_emac_readl(priv, EMAC_MODE_OFFSET);
    regval &= ~(EMAC_TX_EN | EMAC_RX_EN);
    bl702_emac_writel(priv, regval, EMAC_MODE_OFFSET);

    // 4. Stop PHY link via phylink
    phylink_stop(priv->phylink);

    // 5. Disable NAPI
    napi_disable(&priv->napi);

    // 6. Free IRQ
    free_irq(netdev->irq, netdev);

    // 7. De-initialize hardware (free buffers, disable clocks)
    bl702_emac_deinit_hw(priv);

    clk_disable_unprepare(priv->mii_clk);
    clk_disable_unprepare(priv->emac_clk);

    return 0;
}
/* Single descriptor not proper
static netdev_tx_t bl702_emac_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
    struct bl702_emac_priv *priv = netdev_priv(netdev);
    unsigned int entry;
    u32 attr_len_word;
    dma_addr_t dma_addr;
    ktime_t now = ktime_get();
    s64 delta_ns = ktime_to_ns(ktime_sub(now, priv->last_tx_irq_time));

    entry = priv->tx_head;

    // Check if descriptor is still owned by hardware
    attr_len_word = bl702_emac_read_bd_word(priv, entry, true, 0);
    if (attr_len_word & EMAC_BD_TX_RD) {
        netif_stop_queue(netdev);
        pr_warn("%s: TX queue full\n", netdev->name);
        return NETDEV_TX_BUSY;
    }

 //  No need i am freeing this properly in tx cleanup  
    // Unmap and free old skb if present
    //if (priv->tx_skb[entry]) {
      //  dma_unmap_single(priv->dev, priv->tx_skb_dma_addr[entry],
    //                     priv->netdev->mtu + ETH_HLEN + ETH_FCS_LEN, DMA_TO_DEVICE);
  //      kfree_skb(priv->tx_skb[entry]);
//    } 

    priv->tx_skb_dma_len[entry] = skb->len;
    // Map current skb for DMA
    dma_addr = dma_map_single(priv->dev, skb->data, priv->tx_skb_dma_len[entry], DMA_TO_DEVICE);
    if (dma_mapping_error(priv->dev, dma_addr)) {
        dev_err(priv->dev, "DMA mapping failed\n");
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }

    priv->tx_skb[entry] = skb;
    priv->tx_skb_dma_addr[entry] = dma_addr;

    // Write DMA address to BD
    bl702_emac_write_bd_word(priv, entry, true, 4, dma_addr);

    // Prepare attribute/length word
    attr_len_word = FIELD_PREP(EMAC_BD_TX_LEN_MASK, skb->len);
    attr_len_word |= EMAC_BD_TX_CRC | EMAC_BD_TX_PAD;

    // Decide if this descriptor should trigger an interrupt
    if (priv->tx_irq_countdown >= 8 || delta_ns >= 10 * 1000 * 1000) { // 10ms
        attr_len_word |= EMAC_BD_TX_IRQ;
        priv->tx_irq_countdown = 0;
        priv->last_tx_irq_time = now;
    } else {
        priv->tx_irq_countdown++;
    }

    if (entry == (EMAC_TX_BD_BUM_MAX - 1)) {
        attr_len_word |= EMAC_BD_TX_WR;
    }

    attr_len_word |= EMAC_BD_TX_RD; // Give ownership to hardware
    bl702_emac_write_bd_word(priv, entry, true, 0, attr_len_word);

    netdev->stats.tx_packets++;
    netdev->stats.tx_bytes += skb->len;
    // Advance tx_head
    priv->tx_head = NEXT_INDEX(priv->tx_head, EMAC_TX_RING_SIZE);
    // Stop queue if full
    if (NEXT_INDEX(priv->tx_head, EMAC_TX_RING_SIZE) == priv->tx_tail) {
    netif_stop_queue(netdev);
    }
    return NETDEV_TX_OK;
}*/


static netdev_tx_t bl702_emac_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
    struct bl702_emac_priv *priv = netdev_priv(netdev);
    unsigned int entry = priv->tx_head;
    unsigned int i, desc_count = 0, needed_desc = 0;
    unsigned int len, offset, frag_size, ring_size = EMAC_TX_RING_SIZE;
    dma_addr_t dma_addr;
    u32 attr_len_word;
    const skb_frag_t *frag;

    // 1. Estimate needed descriptors for skb->data
    len = skb_headlen(skb);
    needed_desc += DIV_ROUND_UP(len, EMAC_TX_BD_BUF_SIZE);

    // 2. Estimate needed descriptors for frags
    for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
        frag_size = skb_frag_size(&skb_shinfo(skb)->frags[i]);
        needed_desc += DIV_ROUND_UP(frag_size, EMAC_TX_BD_BUF_SIZE);
    }

    // 3. Check if enough descriptors are available
    for (i = 0; i < needed_desc; i++) {
	    unsigned int idx = (entry + i) % ring_size;
	    u32 status = bl702_emac_read_bd_word(priv, idx, true, 0);
	    if (status & EMAC_BD_TX_RD) {
		netif_stop_queue(netdev);

		// Optional: recheck once to avoid false positive
		smp_rmb();
		status = bl702_emac_read_bd_word(priv, idx, true, 0);
		if ((status & EMAC_BD_TX_RD) == 0)
		    netif_wake_queue(netdev);  // Rare, but helps

		return NETDEV_TX_BUSY;
	    }
     }

    // 4. Map skb->data
    len = skb_headlen(skb);
    offset = 0;
    while (len) {
        unsigned int size = min(len, EMAC_TX_BD_BUF_SIZE);
        dma_addr = dma_map_single(priv->dev, skb->data + offset, size, DMA_TO_DEVICE);
        if (dma_mapping_error(priv->dev, dma_addr)) {
            dev_kfree_skb_any(skb);
            return NETDEV_TX_OK;
        }

        priv->tx_skb_dma_addr[entry] = dma_addr;
        priv->tx_skb[entry] = (desc_count == 0) ? skb : NULL;  // only first stores skb
        bl702_emac_write_bd_word(priv, entry, true, 4, dma_addr);

        attr_len_word = FIELD_PREP(EMAC_BD_TX_LEN_MASK, size);
        if (entry == ring_size - 1)
            attr_len_word |= EMAC_BD_TX_WR;
        if (desc_count == needed_desc - 1)
            attr_len_word |= EMAC_BD_TX_EOF_MASK;  // Final desc: EOF
        attr_len_word |= EMAC_BD_TX_CRC | EMAC_BD_TX_PAD | EMAC_BD_TX_RD;

        bl702_emac_write_bd_word(priv, entry, true, 0, attr_len_word);

        len -= size;
        offset += size;
        entry = (entry + 1) % ring_size;
        desc_count++;
    }

    // 5. Map fragments
    for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
        frag = &skb_shinfo(skb)->frags[i];
        frag_size = skb_frag_size(frag);
        offset = 0;

        while (frag_size) {
            unsigned int size = min(frag_size, EMAC_TX_BD_BUF_SIZE);
            dma_addr = skb_frag_dma_map(priv->dev, frag, offset, size, DMA_TO_DEVICE);
            if (dma_mapping_error(priv->dev, dma_addr)) {
                dev_kfree_skb_any(skb);
                return NETDEV_TX_OK;
            }

            priv->tx_skb_dma_addr[entry] = dma_addr;
            priv->tx_skb[entry] = NULL;
            bl702_emac_write_bd_word(priv, entry, true, 4, dma_addr);

            attr_len_word = FIELD_PREP(EMAC_BD_TX_LEN_MASK, size);
            if (entry == ring_size - 1)
                attr_len_word |= EMAC_BD_TX_WR;
            if (desc_count == needed_desc - 1)
                attr_len_word |= EMAC_BD_TX_EOF_MASK;
            attr_len_word |= EMAC_BD_TX_CRC | EMAC_BD_TX_PAD | EMAC_BD_TX_RD;

            bl702_emac_write_bd_word(priv, entry, true, 0, attr_len_word);

            frag_size -= size;
            offset += size;
            entry = (entry + 1) % ring_size;
            desc_count++;
        }
    }

    // 6. Update statistics and tx_head
    netdev->stats.tx_packets++;
    netdev->stats.tx_bytes += skb->len;
    priv->tx_head = entry;

    // 7. Stop queue if no room for at least 1 full frame (assume 2 descs min)
    unsigned int next = (priv->tx_head + 2) % ring_size;
    u32 next_status = bl702_emac_read_bd_word(priv, next, true, 0);
    if (next_status & EMAC_BD_TX_RD)
        netif_stop_queue(netdev);

    return NETDEV_TX_OK;
}

/*
// --- NAPI Poll Function (for RX) ---
static int bl702_emac_poll(struct napi_struct *napi, int budget)
{
    struct bl702_emac_priv *priv = container_of(napi, struct bl702_emac_priv, napi);
    struct net_device *netdev = priv->netdev;
    int received_packets = 0;
    unsigned int entry;
    u32 attr_len_word;
    u32 rx_len;
    struct sk_buff *skb;

    while (received_packets < budget) {
        entry = priv->rx_head;
        attr_len_word = bl702_emac_read_bd_word(priv, entry, false, 0);

        // Check if BD is still Empty (hardware owned)
        if (attr_len_word & EMAC_BD_RX_E) {
            // No more packets to process in this poll cycle
            break;
        }

        // Hardware has filled this BD. Process it.
        rx_len = FIELD_GET(EMAC_BD_RX_LEN_MASK, attr_len_word);
        skb = priv->rx_skb[entry];

        // Unmap the DMA buffer
        dma_unmap_single(priv->dev, priv->rx_skb_dma_addr[entry],
                         priv->rx_skb_dma_len[entry], DMA_FROM_DEVICE);

        // Check for RX errors (Simplified)
        if (attr_len_word & (EMAC_BD_RX_CRC | EMAC_BD_RX_SF | EMAC_BD_RX_TL |
                             EMAC_BD_RX_DN | EMAC_BD_RX_RE | EMAC_BD_RX_OR | EMAC_BD_RX_M)) {
            netdev->stats.rx_errors++;
            if (attr_len_word & EMAC_BD_RX_CRC) netdev->stats.rx_crc_errors++;
            if (attr_len_word & EMAC_BD_RX_OR) netdev->stats.rx_over_errors++;
            // Don't pass up error frames, just free skb
            dev_kfree_skb_any(skb);
            skb = NULL; // Mark as consumed
        } else {
            // Packet received successfully
            skb_put(skb, rx_len); // Adjust skb length to actual received data
            skb->protocol = eth_type_trans(skb, netdev); // Determine protocol
            netif_receive_skb(skb); // Pass to higher layers
            netdev->stats.rx_packets++;
            netdev->stats.rx_bytes += rx_len;
            received_packets++;
            skb = NULL; // Mark as consumed
        }

        // Re-arm the descriptor for the next reception
        // Allocate a new skb for the freed BD slot
        priv->rx_skb[entry] = netdev_alloc_skb(netdev, netdev->mtu + ETH_HLEN + ETH_FCS_LEN + NET_IP_ALIGN);
        if (!priv->rx_skb[entry]) {
            dev_err(priv->dev, "Failed to allocate new RX skb\n");
            netdev->stats.rx_dropped++;
            // This is critical. We are out of RX buffers.
            // Consider stopping NAPI or trying again later.
            // For now, leave BD in "non-empty" state to avoid HW issues.
            break; // Stop processing and try again next poll
        }
        skb_reserve(priv->rx_skb[entry], NET_IP_ALIGN);

        // Map new skb for DMA
        priv->rx_skb_dma_addr[entry] = dma_map_single(priv->dev, priv->rx_skb[entry]->data,
                                                       priv->rx_skb_dma_len[entry],
                                                       DMA_FROM_DEVICE);
        if (dma_mapping_error(priv->dev, priv->rx_skb_dma_addr[entry])) {
            dev_err(priv->dev, "Failed to map new RX skb for DMA\n");
            kfree_skb(priv->rx_skb[entry]);
            priv->rx_skb[entry] = NULL;
            netdev->stats.rx_dropped++;
            break; // Stop processing
        }
        // Write new DMA address to internal BD (Word 1)
        bl702_emac_write_bd_word(priv, entry, false, 4, priv->rx_skb_dma_addr[entry]);

        // Re-set attribute: Empty and Interrupt (and Wrap for last BD)
        attr_len_word = EMAC_BD_RX_E | EMAC_BD_RX_IRQ;
        if (entry == (EMAC_RX_BD_BUM_MAX - 1)) {
            attr_len_word |= EMAC_BD_RX_WR;
        }
        bl702_emac_write_bd_word(priv, entry, false, 0, attr_len_word);

        // Advance RX head pointer
        priv->rx_head = (priv->rx_head + 1) & EMAC_RX_BD_BUM_MASK;
    }

    // If we didn't exhaust budget, disable interrupts and complete NAPI cycle
    if (received_packets < budget) {
        napi_complete_done(napi, received_packets);
        // Re-enable RX B/C interrupts (masked earlier by NAPI poll scheduling)
        // Read current mask, set RXB_M | RXC_M
        u32 mask = bl702_emac_readl(priv, EMAC_INT_MASK_OFFSET);
        mask |= (EMAC_RXB_M | EMAC_RXC_M);
        bl702_emac_writel(priv, mask, EMAC_INT_MASK_OFFSET);
    }

    return received_packets;
}
*/
static int bl702_emac_replenish_rx_skb(struct bl702_emac_priv *priv, unsigned int entry)
{
    struct net_device *netdev = priv->netdev;
    struct sk_buff *new_skb;
    u32 attr_len_word;
    new_skb = netdev_alloc_skb(netdev, netdev->mtu + ETH_HLEN + ETH_FCS_LEN + NET_IP_ALIGN);
    if (!new_skb) {
        dev_err(priv->dev, "Failed to allocate new RX skb\n");
        netdev->stats.rx_dropped++;
        return -ENOMEM;
    }

    skb_reserve(new_skb, NET_IP_ALIGN);
    priv->rx_skb[entry] = new_skb;

    priv->rx_skb_dma_addr[entry] = dma_map_single(priv->dev, new_skb->data,
                                                  priv->rx_skb_dma_len[entry],
                                                  DMA_FROM_DEVICE);
    if (dma_mapping_error(priv->dev, priv->rx_skb_dma_addr[entry])) {
        dev_err(priv->dev, "Failed to map RX skb for DMA\n");
        dev_kfree_skb_any(new_skb);
        priv->rx_skb[entry] = NULL;
        netdev->stats.rx_dropped++;
        return -EIO;
    }

    bl702_emac_write_bd_word(priv, entry, false, 4, priv->rx_skb_dma_addr[entry]);

    attr_len_word = EMAC_BD_RX_E | EMAC_BD_RX_IRQ;
    if (entry == (EMAC_RX_BD_BUM_MAX - 1)) {
        attr_len_word |= EMAC_BD_RX_WR;
    }

    bl702_emac_write_bd_word(priv, entry, false, 0, attr_len_word);
    return 0;
}

static bool bl702_emac_handle_rx_errors(struct bl702_emac_priv *priv, u32 attr_len_word, struct sk_buff *skb)
{
	struct net_device *ndev = priv->ndev;
	struct net_device_stats *stats = &ndev->stats;

	if (attr_len_word & EMAC_BD_RX_CRC_MASK) {
		stats->rx_crc_errors++;
		stats->rx_errors++;
	}

	if (attr_len_word & EMAC_BD_RX_SF_MASK) {
		stats->rx_length_errors++;
		stats->rx_errors++;
	}

	if (attr_len_word & EMAC_BD_RX_TL_MASK) {
		stats->rx_length_errors++;
		stats->rx_errors++;
	}

	if (attr_len_word & EMAC_BD_RX_DN_MASK) {
		stats->rx_frame_errors++;
		stats->rx_errors++;
	}

	if (attr_len_word & EMAC_BD_RX_RE_MASK) {
		stats->rx_errors++;
	}

	if (attr_len_word & EMAC_BD_RX_OR_MASK) {
		stats->rx_over_errors++;
		stats->rx_fifo_errors++; // Optional, if both are semantically meaningful
		stats->rx_errors++;
	}

	if (attr_len_word & EMAC_BD_RX_M_MASK) {
		stats->rx_missed_errors++;
		stats->rx_errors++;
	}

	if (attr_len_word & (EMAC_BD_RX_CRC_MASK | EMAC_BD_RX_SF_MASK | EMAC_BD_RX_TL_MASK |
	                     EMAC_BD_RX_DN_MASK | EMAC_BD_RX_RE_MASK | EMAC_BD_RX_OR_MASK |
	                     EMAC_BD_RX_M_MASK)) {
		dev_kfree_skb_any(skb);
		return true;
	}

	return false;
}
static int bl702_emac_process_rx_entry(struct bl702_emac_priv *priv, struct napi_struct *napi)
{
    struct net_device *netdev = priv->netdev;
    unsigned int entry = priv->rx_head;
    u32 attr_len_word = bl702_emac_read_bd_word(priv, entry, false, 0);

    if (attr_len_word & EMAC_BD_RX_E)
        return 0; // No more packets

    u32 rx_len = FIELD_GET(EMAC_BD_RX_LEN_MASK, attr_len_word);
    struct sk_buff *skb = priv->rx_skb[entry];

    dma_unmap_single(priv->dev, priv->rx_skb_dma_addr[entry],
                     priv->rx_skb_dma_len[entry], DMA_FROM_DEVICE);

    if (bl702_emac_handle_rx_errors(priv, attr_len_word, skb)) {
        priv->rx_skb[entry] = NULL;
        goto replenish;
    }

    skb_put(skb, rx_len);
    skb->protocol = eth_type_trans(skb, netdev);
    napi_gro_receive(napi, skb);

    netdev->stats.rx_packets++;
    netdev->stats.rx_bytes += rx_len;

    priv->rx_skb[entry] = NULL;

replenish:
    if (bl702_emac_replenish_rx_skb(priv, entry) < 0)
        return -1;

    priv->rx_head = NEXT_INDEX(priv->rx_head, EMAC_RX_BD_BUM_MAX); 
    return 1;
}
static bool bl702_rx_pending(struct bl702_emac_priv *priv)
{
	unsigned int entry;
	u32 status;
	entry = priv->rx_head;
	status = bl702_emac_read_bd_word(priv, entry, false, 0)
	
	rmb();  // Make sure DMA writes are visible

	return !(status & EMAC_BD_RX_E_MASK);  // Adjust this based on your descriptor
}

static int bl702_emac_poll(struct napi_struct *napi, int budget)
{
    struct bl702_emac_priv *priv = container_of(napi, struct bl702_emac_priv, napi);
    int received_packets = 0;

    while (received_packets < budget) {
        int ret = bl702_emac_process_rx_entry(priv, napi);
        if (ret == 0)  // No more packets
            break;
        if (ret < 0)   // Replenishment failed or error
            break;

        received_packets++;
    }

    if (received_packets < budget) {
        napi_complete_done(napi, received_packets);
        u32 mask = bl702_emac_readl(priv, EMAC_INT_MASK_OFFSET);
        mask |= (EMAC_RXB_M | EMAC_RXC_M);
        bl702_emac_writel(priv, mask, EMAC_INT_MASK_OFFSET);
	
    }

    return received_packets;
}

static inline unsigned int tx_ring_space(unsigned int tx_head, unsigned int tx_tail, unsigned int ring_size)
{
    if (tx_tail > tx_head)
        return tx_tail - tx_head - 1;
    else
        return ring_size + tx_tail - tx_head - 1;
}

// Helper: TX cleanup function
static void bl702_emac_tx_cleanup(struct net_device *netdev)
{
    struct bl702_emac_priv *priv = netdev_priv(netdev);
    unsigned int entry = priv->tx_tail;
    u32 attr_len_word;

    while (entry != priv->tx_head) {
        attr_len_word = bl702_emac_read_bd_word(priv, entry, true, 0);
        if (attr_len_word & EMAC_BD_TX_RD) {
            // Hardware still owns descriptor, stop cleanup
            break;
        }

        // Descriptor is done: unmap and free skb
        dma_unmap_single(priv->dev, priv->tx_skb_dma_addr[entry],
                         priv->tx_skb_dma_len[entry], DMA_TO_DEVICE);
        dev_kfree_skb_any(priv->tx_skb[entry]);
        priv->tx_skb[entry] = NULL;

        // Update error stats if any errors flagged
        if (attr_len_word & (EMAC_BD_TX_CS | EMAC_BD_TX_DF | EMAC_BD_TX_LC |
                             EMAC_BD_TX_RL | EMAC_BD_TX_UR)) {
            netdev->stats.tx_errors++;
        }

        // Advance tail pointer with wrap-around
        entry = NEXT_INDEX(entry, EMAC_TX_BD_BUM_MAX);
    }

    priv->tx_tail = entry;

    // Wake queue if it was stopped and space is available
    if (netif_queue_stopped(netdev) && tx_ring_space(priv->tx_head, priv->tx_tail, EMAC_TX_BD_BUM_MAX) > 0) {
       netif_wake_queue(netdev);
    }

}
// --- Interrupt Handler ---
static irqreturn_t bl702_emac_irq(int irq, void *dev_id)
{
    struct net_device *netdev = (struct net_device *)dev_id;
    struct bl702_emac_priv *priv = netdev_priv(netdev);
    u32 int_status;
    irqreturn_t ret = IRQ_NONE;

    // Read and clear interrupt status
    int_status = bl702_emac_readl(priv, EMAC_INT_SOURCE_OFFSET);
    bl702_emac_writel(priv, int_status, EMAC_INT_SOURCE_OFFSET);

    if (int_status == 0)
        return IRQ_NONE;

    ret = IRQ_HANDLED;

    // Handle RX frame interrupt - schedule NAPI poll
    if (int_status & EMAC_RXB) {
        if (napi_schedule_prep(&priv->napi)) {
            __napi_schedule(&priv->napi);
            // Mask RX interrupts until NAPI done
            u32 mask = bl702_emac_readl(priv, EMAC_INT_MASK_OFFSET);
            mask &= ~(EMAC_RXB_M | EMAC_RXC_M | EMAC_RXE_M);
            bl702_emac_writel(priv, mask, EMAC_INT_MASK_OFFSET);
        }
    }

    // Handle RX error interrupt
    if (int_status & EMAC_RXE) {
        netdev->stats.rx_errors++;
        dev_warn(priv->dev, "EMAC RX error interrupt\n");
        // Optionally add further RX error recovery here
    }

    // Handle TX complete or TX buffer interrupt - clean up TX
    if (int_status & (EMAC_TXB | EMAC_TXC)) {
        bl702_emac_tx_cleanup(netdev);
    }

    // Handle TX error interrupt
    if (int_status & EMAC_TXE) {
        netdev->stats.tx_errors++;
        dev_warn(priv->dev, "EMAC TX error interrupt\n");
        // Optionally add further TX error recovery here
    }

    // Handle BUSY interrupt - no empty RX BD available
    if (int_status & EMAC_BUSY) {
        netdev->stats.rx_fifo_errors++;
        dev_warn(priv->dev, "EMAC RX Busy interrupt - no empty RX buffers\n");
        // Could trigger some recovery or notification here
    }

    // Optionally handle RXC, TXC if you want logging or stats

    return ret;
}

static const struct net_device_ops bl702_emac_netdev_ops = {
    .ndo_open = bl702_emac_open,
    .ndo_stop = bl702_emac_stop,
    .ndo_start_xmit = bl702_emac_start_xmit,
    .ndo_set_mac_address = eth_mac_addr, // Use common helper
    .ndo_validate_addr = eth_validate_addr, // Use common helper
    // .ndo_get_stats64 = eth_get_stats64, // If you need 64-bit stats
};

// --- Platform Driver Probe/Remove ---

static int bl702_emac_probe(struct platform_device *pdev)
{
    struct net_device *netdev;
    struct bl702_emac_priv *priv;
    struct resource *res;
    int irq;
    int ret;
    u8 mac_addr[ETH_ALEN];
    const void *mac_addr_prop;
    int len;

    // 1. Allocate net_device structure
    netdev = alloc_etherdev(sizeof(*priv));
    if (!netdev) {
        return -ENOMEM;
    }
    platform_set_drvdata(pdev, netdev);
    SET_NETDEV_DEV(netdev, &pdev->dev);
    priv = netdev_priv(netdev);
    priv->netdev = netdev;
    priv->dev = &pdev->dev;

    // 2. Get resources from Device Tree
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    priv->base_addr = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(priv->base_addr)) {
        ret = PTR_ERR(priv->base_addr);
        goto err_free_netdev;
    }

    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
        ret = irq;
        goto err_free_netdev;
    }
    netdev->irq = irq;

    // Get MAC address from device tree
    mac_addr_prop = of_get_mac_address(pdev->dev.of_node);
    if (IS_ERR(mac_addr_prop)) {
        // If not found, generate a random one and log a warning
        eth_hw_addr_random(netdev);
        memcpy(priv->mac_addr, netdev->dev_addr, ETH_ALEN);
        dev_warn(&pdev->dev, "MAC address not found in device tree, using random: %pM\n",
                 priv->mac_addr);
    } else {
        memcpy(priv->mac_addr, mac_addr_prop, ETH_ALEN);
        memcpy(netdev->dev_addr, priv->mac_addr, ETH_ALEN);
    }


    // 3. Setup net_device operations
    netdev->netdev_ops = &bl702_emac_netdev_ops;

    // 4. Initialize NAPI
    netif_napi_add(netdev, &priv->napi, bl702_emac_poll, 1518); // NAPI weight (budget)

    // 6. Register net_device
    ret = register_netdev(netdev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to register netdev: %d\n", ret);
	goto err_free_netdev;
    }

    pr_info("%s: BL702 EMAC driver loaded. MAC: %pM\n", netdev->name, netdev->dev_addr);

    return 0;


err_free_netdev:
    free_netdev(netdev);
    return ret;
}

static void bl702_emac_remove(struct platform_device *pdev)
{
    struct net_device *netdev = platform_get_drvdata(pdev);
    struct bl702_emac_priv *priv = netdev_priv(netdev);

    unregister_netdev(netdev); // Also calls netdev->netdev_ops->ndo_stop()

    netif_napi_del(&priv->napi);

    free_netdev(netdev);
}

// --- Device Tree Matching ---
static const struct of_device_id bl702_emac_of_match[] = {
    { .compatible = "bouffalolab,bl702-emac", },
    {},
};
MODULE_DEVICE_TABLE(of, bl702_emac_of_match);

// --- Platform Driver Structure ---
static struct platform_driver bl702_emac_driver = {
    .probe      = bl702_emac_probe,
    .remove     = bl702_emac_remove,
    .driver     = {
        .name           = DRV_NAME,
        .of_match_table = bl702_emac_of_match,
    },
};

module_platform_driver(bl702_emac_driver);

MODULE_AUTHOR("Vishnu S");
MODULE_DESCRIPTION("Bouffalo Lab BL702 EMAC Ethernet driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
