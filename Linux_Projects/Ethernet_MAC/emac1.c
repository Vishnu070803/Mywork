#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/io.h>
#include "emac.h"
#include <linux/phy/phy.h>
#include <linux/phylink.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/mdio.h>
#define DRIVER_NAME "flc_emac"
#define EMAC_MAX_FRAME_LENGTH   (0x600)
#define EMAC_MIN_FRAME_LENGTH   (0x40)
static int emac_open(struct net_device *ndev)
{
    struct emac_priv *priv = netdev_priv(ndev);
    int err;

    pr_info("EMAC: open called\n");

    if (!priv->mii_phy) {
        dev_err(priv->dev, "rmii_phy is NULL! Cannot power on PHY.\n");
        return -ENODEV;
    }

    err = phy_power_on(priv->mii_phy);
    if (err)
        goto reset_hw;

    err = emac_phylink_connect(priv);
    if (err)
        goto phy_off;

    err = emac_init_bd_list(priv);
    if (err) {
        dev_err(priv->dev, "Failed to initialize BD list\n");
        return err;
    }

    netif_start_queue(ndev);
    return 0;

reset_hw:
    emac_reset_hw(priv);
    emac_free_bd_list(priv);
phy_off:
    phy_power_off(priv->mii_phy);
    return err;
}

static int emac_stop(struct net_device *ndev)
{
   struct emac_priv *priv = netdev_priv(ndev);

    netif_stop_queue(ndev);

    emac_reset_hw(priv);
    emac_free_bd_list(priv);
	
    return 0;
}

static netdev_tx_t emac_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    pr_info("EMAC: start_xmit called (dropping packet for now)\n");
    dev_kfree_skb(skb);  // Just drop packets for now
    return NETDEV_TX_OK;
}

irqreturn_t emac_interrupt_handler(int irq, void *dev_id)
{
    //struct net_device *ndev = dev_id;
   // struct emac_priv *priv = netdev_priv(ndev);

    // TODO: Clear IRQ status, process RX/TX

    return IRQ_HANDLED;
}
static bool emac_phy_handle_exists(struct device_node *dn)
{
	dn = of_parse_phandle(dn, "phy-handle", 0);
	of_node_put(dn);
	return dn != NULL;
}

static int emac_phylink_connect(struct emac_priv *priv)
{
	struct device_node *dn = priv->pdev->dev.of_node;
	struct net_device *ndev = priv->ndev;
	struct phy_device *phydev;
	int ret;

	if (dn)
		ret = phylink_of_phy_connect(priv->phylink, dn, 0);

	if (!dn || (ret && !emac_phy_handle_exists(dn))) {
		phydev = phy_find_first(priv->mii_bus);
		if (!phydev) {
			netdev_err(ndev, "no PHY found\n");
			return -ENXIO;
		}

		/* attach the mac to the phy */
		ret = phylink_connect_phy(priv->phylink, phydev);
	}

	if (ret) {
		netdev_err(ndev, "Could not attach PHY (%d)\n", ret);
		return ret;
	}

	/* Since this driver uses runtime handling of clocks, initiate a phy
	 * reset if the attached phy requires it. Check return to see if phy
	 * was reset and then do a phy initialization.
	 */
	if (phy_reset_after_clk_enable(ndev->phydev) == 1)
		phy_init_hw(ndev->phydev);

	phylink_start(priv->phylink);

	return 0;
}

static void emac_free_bd_list(struct emac_priv *priv)
{
    struct net_device *ndev = priv->ndev;
    size_t bd_total_size = EMAC_DESC_NUM_TOTAL * sizeof(struct emac_bd_desc);
    size_t tx_buf_size = EMAC_TX_DESC_NUM * ETH_MAX_PKT_LEN;
    size_t rx_buf_size = EMAC_RX_DESC_NUM * ETH_MAX_PKT_LEN;

    netif_stop_queue(ndev);

    if (priv->bd_base)
        dma_free_coherent(priv->dev, bd_total_size, priv->bd_base, priv->bd_dma);
    if (priv->tx_bufs)
        dma_free_coherent(priv->dev, tx_buf_size, priv->tx_bufs, priv->tx_dma);
    if (priv->rx_bufs)
        dma_free_coherent(priv->dev, rx_buf_size, priv->rx_bufs, priv->rx_dma);
    netif_stop_queue(ndev);



}
static void emac_reset_hw(struct emac_priv *priv)
{
    u32 reg;

    reg = readl(priv->base + EMAC_MODE_OFFSET);
    reg &= ~(EMAC_TX_EN | EMAC_RX_EN);
    writel(reg, priv->base + EMAC_MODE_OFFSET);

    pr_info("EMAC: Hardware reset (TX/RX disabled)\n");
}
static int emac_init_bd_list(struct emac_priv *priv)
{
    size_t bd_total_size = EMAC_DESC_NUM_TOTAL * sizeof(struct emac_bd_desc);
    size_t tx_buf_size = EMAC_TX_DESC_NUM * ETH_MAX_PKT_LEN;
    size_t rx_buf_size = EMAC_RX_DESC_NUM * ETH_MAX_PKT_LEN;
    int i;

    priv->bd_base = dma_alloc_coherent(priv->dev, bd_total_size, &priv->bd_dma, GFP_KERNEL);
    if (!priv->bd_base)
        return -ENOMEM;

    priv->tx_bufs = dma_alloc_coherent(priv->dev, tx_buf_size, &priv->tx_dma, GFP_KERNEL);
    if (!priv->tx_bufs)
        return -ENOMEM;

    priv->rx_bufs = dma_alloc_coherent(priv->dev, rx_buf_size, &priv->rx_dma, GFP_KERNEL);
    if (!priv->rx_bufs)
        return -ENOMEM;

    /* Init TX BDs */
    for (i = 0; i < EMAC_TX_DESC_NUM; i++) {
        priv->bd_base[i].buffer = priv->tx_dma + (i * ETH_MAX_PKT_LEN);
        priv->bd_base[i].c_s_l = 0;
    }
    priv->bd_base[EMAC_TX_DESC_NUM - 1].c_s_l |= EMAC_BD_TX_WR_MASK;

    /* Init RX BDs */
    for (i = EMAC_TX_DESC_NUM; i < EMAC_DESC_NUM_TOTAL; i++) {
        int rx_i = i - EMAC_TX_DESC_NUM;
        priv->bd_base[i].buffer = priv->rx_dma + (rx_i * ETH_MAX_PKT_LEN);
        priv->bd_base[i].c_s_l = (ETH_MAX_PKT_LEN << 16) |
                                 EMAC_BD_RX_IRQ_MASK |
                                 EMAC_BD_RX_E_MASK;
    }
    priv->bd_base[EMAC_DESC_NUM_TOTAL - 1].c_s_l |= EMAC_BD_RX_WR_MASK;

    /* Write TX BD count to hardware */
    writel(EMAC_TX_DESC_NUM, priv->base + EMAC_TX_BD_NUM_OFFSET);

    return 0;
}
static int emac_mdio_read(struct mii_bus *bus, int phy_addr, int regnum)
{
    struct emac_priv *priv = bus->priv;
    uint32_t reg_val;

    reg_val = readl(priv->base + EMAC_MIIADDRESS_OFFSET);
    reg_val &= ~(EMAC_FIAD_MASK | EMAC_RGAD_MASK);
    reg_val |= (phy_addr << EMAC_FIAD_SHIFT) & EMAC_FIAD_MASK;
    reg_val |= (regnum<< EMAC_RGAD_SHIFT) & EMAC_RGAD_MASK;
    writel(reg_val, priv->base + EMAC_MIIADDRESS_OFFSET);

    reg_val = readl(priv->base + EMAC_MIICOMMAND_OFFSET);
    reg_val |= EMAC_RSTAT;
    writel(reg_val, priv->base + EMAC_MIICOMMAND_OFFSET);

    do {
        reg_val = readl(priv->base + EMAC_MIISTATUS_OFFSET);
        udelay(16);
    } while (reg_val & EMAC_MIIM_BUSY);
    
    pr_info("%s and %d\n", __func__, __LINE__);
    return readl(priv->base + EMAC_MIIRX_DATA_OFFSET);
}
static int emac_mdio_write(struct mii_bus *bus, int phy_addr, int regnum, u16 val)
{
    struct emac_priv *priv = bus->priv;
    uint32_t reg_val;

    reg_val = readl(priv->base + EMAC_MIIADDRESS_OFFSET);
    reg_val &= ~(EMAC_FIAD_MASK | EMAC_RGAD_MASK);
    reg_val |= (phy_addr << EMAC_FIAD_SHIFT) & EMAC_FIAD_MASK;
    reg_val |= (regnum << EMAC_RGAD_SHIFT) & EMAC_RGAD_MASK;
    writel(reg_val, priv->base + EMAC_MIIADDRESS_OFFSET);

    /* set write data */
    reg_val = readl((priv-> + EMAC_MIITX_DATA_OFFSET));
    reg_val &= ~EMAC_CTRLDATA_MASK;
    reg_val |= (val << EMAC_CTRLDATA_SHIFT) & EMAC_CTRLDATA_MASK;
    writel(re_val, priv->base + EMAC_MIITX_DATA_OFFSET);

    reg_val = readl(priv->base + EMAC_MIICOMMAND_OFFSET);
    reg_val |= EMAC_WCTRLDATA;
    writel(reg_val, priv->base + EMAC_MIICOMMAND_OFFSET);

    do {
        reg_val = readl(priv->base + EMAC_MIISTATUS_OFFSET);
        udelay(16);
    } while (reg_val & EMAC_MIIM_BUSY);
    pr_info("%s and %d\n", __func__, __LINE__);
    return 0;
}

static void emac_handle_link_change(struct net_device *ndev)
{
	struct emac_priv *priv = netdev_priv(ndev);
	struct phy_device *phydev = ndev->phydev;
	unsigned long flags;
	int status_change = 0;

	spin_lock_irqsave(&priv->lock, flags);

	if (phydev->link) {
		if ((priv->duplex != phydev->duplex)) {
			u32 reg;

			reg=readl(priv->base + EMAC_MODE_OFFSET);

			reg &= ~(EMAC_FULLD);

			if (phydev->duplex)
				reg |= EMAC_FULLD;

			writel(reg, priv->base + EMAC_MODE_OFFSET);
			priv->duplex = phydev->duplex;
			status_change = 1;
		}
	}

	if (phydev->link != priv->link) {
		if (!phydev->link) {
			priv->speed = 0;
			priv->duplex = -1;
		}
		priv->link = phydev->link;

		status_change = 1;
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	if (status_change) {
		if (phydev->link) {
			/* Update the TX clock rate if and only if the link is
			 * up and there has been a link change.
			 */
		//	emac_set_tx_clk(priv->tx_clk, phydev->speed, dev);

			netif_carrier_on(ndev);
			netdev_info(ndev, "link up (%d/%s)\n",
				    phydev->speed,
				    phydev->duplex == DUPLEX_FULL ?
				    "Full" : "Half");
		} else {
			netif_carrier_off(ndev);
			netdev_info(ndev, "link down\n");
		}
	}
}


static int emac_mii_probe(struct net_device *ndev)
{
	struct emac_priv *priv = netdev_priv(ndev);
	struct phy_device *phydev;
	struct device_node *np;
	int ret, i;

	np = priv->pdev->dev.of_node;
	ret = 0;

	if (np) {
		if (of_phy_is_fixed_link(np)) {	
			priv->phy_node = of_node_get(np);
		} else {
			
			priv->phy_node = of_parse_phandle(np, "phy-handle", 0);
			/* fallback to standard phy registration if no
			 * phy-handle was found nor any phy found during
			 * dt phy registration
			 */

			if (!priv->phy_node && !phy_find_first(priv->mii_bus)) {
				for (i = 0; i < PHY_MAX_ADDR; i++) {
					phydev = mdiobus_scan(priv->mii_bus, i);
					if (IS_ERR(phydev) &&
					    PTR_ERR(phydev) != -ENODEV) {
						ret = PTR_ERR(phydev);
						break;
					}

				}

				if (ret)
					return -ENODEV;
			}
		}
	}

	if (priv->phy_node) {

		phydev = of_phy_connect(ndev, priv->phy_node,
					&emac_handle_link_change, 0,
					priv->phy_interface);

		if (!phydev)
			return -ENODEV;
	} else {
		phydev = phy_find_first(priv->mii_bus);
			if (!phydev) {
			netdev_err(ndev, "no PHY found\n");
			return -ENXIO;
		}

		ret = phy_connect_direct(ndev, phydev, &emac_handle_link_change,
					 priv->phy_interface);
	
		if (ret) {
			netdev_err(ndev, "Could not attach to PHY\n");
			return ret;
		}
	}

	/* mask with MAC supported features */
	phy_set_max_speed(phydev, SPEED_100);


	priv->link = 0;
	priv->speed = 0;
	priv->duplex = -1;

	return 0;
}

static int emac_mii_init(struct emac_priv *priv)
{
	struct device *dev = &priv->pdev->dev;
	struct device_node *np = priv->pdev->dev.of_node;
	int err = -ENXIO;

	/* Enable MII management interface if needed (depends on your hardware) */
	// emac_writel(priv, NCR, EMAC_BIT(MPE)); // Uncomment if needed

	priv->mii_bus = mdiobus_alloc();
	if (!priv->mii_bus) {
		dev_err(dev, "Failed to allocate mii_bus\n");
		return -ENOMEM;
	}

	priv->mii_bus->name = "emac_mii_bus";
	priv->mii_bus->read = &emac_mdio_read;
	priv->mii_bus->write = &emac_mdio_write;
	priv->mii_bus->priv = priv;
	priv->mii_bus->parent = dev;

	snprintf(priv->mii_bus->id, MII_BUS_ID_SIZE, "%s-%x",
	         priv->pdev->name, priv->pdev->id);

	// Store it for potential reuse or debugging
	dev_set_drvdata(dev, priv->mii_bus);

	// Check if PHY is fixed-link
	if (np && of_phy_is_fixed_link(np)) {
		if (of_phy_register_fixed_link(np)) {
			dev_err(dev, "Broken fixed-link PHY spec at %pOF\n", np);
		
			goto err_free_bus;
		}

		err = mdiobus_register(priv->mii_bus);
	} else {

		err = of_mdiobus_register(priv->mii_bus, np);
	}

	if (err) {
		dev_err(dev, "Failed to register MDIO bus: %d\n", err);
		goto err_deregister_fixed;
	}

	err = emac_mii_probe(priv->ndev);
	
	pr_info("funtion : %s    and  line : %d\n", __func__, __LINE__);
        
	if(err)
		goto err_unregister_bus;


/*
	// Connect PHY (optional: use emac_adjust_link)
	priv->phydev = of_phy_connect(priv->ndev, np,
	                              &emac_adjust_link,
	                              0, priv->phy_interface);
	if (!priv->phydev) {
		dev_err(dev, "Failed to connect to PHY\n");
		err = -ENODEV;
		goto err_unregister_bus;
	}*/

	//priv->ndev->phydev = priv->phydev;
	return 0;

err_unregister_bus:
	mdiobus_unregister(priv->mii_bus);

err_deregister_fixed:
	if (np && of_phy_is_fixed_link(np))
		of_phy_deregister_fixed_link(np);

err_free_bus:
	mdiobus_free(priv->mii_bus);

	return err;
}
#if 0
int emac_hw_init(struct emac_priv *priv)
{
    u32 reg_val;

    /* Set MAC mode */
    reg_val = EMAC_PRO | EMAC_BRO | EMAC_PAD | 
              EMAC_CRCEN | EMAC_RECSMALL | EMAC_IFG;
    writel(reg_val, priv->base + EMAC_MODE_OFFSET);
    printk(KERN_INFO "EMAC_MODE: 0x%08x\n", readl(priv->base + EMAC_MODE_OFFSET));

    /* Set inter frame gap */
    writel(0x18 << EMAC_IPGT_SHIFT, priv->base + EMAC_IPGT_OFFSET);
    printk(KERN_INFO "EMAC_IPGT: 0x%08x\n", readl(priv->base + EMAC_IPGT_OFFSET));

    /* Set MII interface */
    reg_val = EMAC_MIINOPRE | (priv->mii_clk_div << EMAC_CLKDIV_SHIFT);
    writel(reg_val, priv->base + EMAC_MIIMODE_OFFSET);
    printk(KERN_INFO "EMAC_MIIMODE: 0x%08x\n", readl(priv->base + EMAC_MIIMODE_OFFSET));

    /* Set collision configuration */
    reg_val = (0xf << EMAC_MAXFL_SHIFT) | (0x10 << EMAC_COLLVALID_SHIFT);
    writel(reg_val, priv->base + EMAC_COLLCONFIG_OFFSET);
    printk(KERN_INFO "EMAC_COLLCONFIG: 0x%08x\n", readl(priv->base + EMAC_COLLCONFIG_OFFSET));

    /* Set frame length */
    reg_val = (priv->min_frame_len << EMAC_MINFL_SHIFT) | 
              (priv->max_frame_len << EMAC_MAXFL_SHIFT);
    writel(reg_val, priv->base + EMAC_PACKETLEN_OFFSET);
    printk(KERN_INFO "EMAC_PACKETLEN: 0x%08x\n", readl(priv->base + EMAC_PACKETLEN_OFFSET));
#if 0
    /* Set MAC address */
    reg_val = (priv->mac_addr[5] << EMAC_MAC_B5_SHIFT) |
              (priv->mac_addr[4] << EMAC_MAC_B4_SHIFT) |
              (priv->mac_addr[3] << EMAC_MAC_B3_SHIFT) |
              (priv->mac_addr[2] << EMAC_MAC_B2_SHIFT);
    writel(reg_val, priv->base + EMAC_MAC_ADDR0_OFFSET);
    printk(KERN_INFO "EMAC_MAC_ADDR0: 0x%08x\n", readl(priv->base + EMAC_MAC_ADDR0_OFFSET));

    reg_val = (priv->mac_addr[1] << EMAC_MAC_B1_SHIFT) |
              (priv->mac_addr[0] << EMAC_MAC_B0_SHIFT);
    writel(reg_val, priv->base + EMAC_MAC_ADDR1_OFFSET);
    printk(KERN_INFO "EMAC_MAC_ADDR1: 0x%08x\n", readl(priv->base + EMAC_MAC_ADDR1_OFFSET));
#endif
    return 0;
}
#endif
int emac_hw_init(struct emac_priv *priv)
{
    u32 reg_val;
    const u8 *mac;
    int err;
    priv->mii_clk_div=0xA;

    /* Step 1: Get MAC address from Device Tree */
    mac = of_get_mac_address(priv->pdev->dev.of_node);
	if (PTR_ERR(mac) == -EPROBE_DEFER) {
		err = -EPROBE_DEFER;
		return err;
	} else if (!IS_ERR_OR_NULL(mac)) {
		ether_addr_copy(priv->ndev->dev_addr, mac);
	} else {
		eth_hw_addr_random(priv->ndev);
	}
   /* disbale tx and rx */
    reg_val = readl(priv->base+EMAC_MODE_OFFSET);
    reg_val &= ~EMAC_TX_EN;
    reg_val &= ~EMAC_RX_EN;
    writel(reg_val, priv->base + EMAC_MODE_OFFSET);
 
/* clean all db */
  //  for (int i = 0; i < 128 * 2; i++) {
    //    writel(0, priv->base + EMAC_DMA_DESC_OFFSET + i * 4);
    //}
    
   /* defualt config */
    reg_val = readl(priv->base + EMAC_MODE_OFFSET); 

    /* Step 2: Set MAC mode */
    /* MII interface enabled */
    reg_val &= ~EMAC_RMII_EN;
     /* all additional bytes are dropped */
    reg_val &= ~EMAC_HUGEN;
     /* half duplex mode */
    reg_val &= ~EMAC_FULLD;
     /* enable sent preamble */
    reg_val &= ~EMAC_NOPRE;
    reg_val = EMAC_PRO | EMAC_BRO | EMAC_PAD | 
              EMAC_CRCEN | EMAC_RECSMALL | EMAC_IFG;
    writel(reg_val, priv->base + EMAC_MODE_OFFSET);
    dev_info(priv->dev, "EMAC_MODE: 0x%08x\n", readl(priv->base + EMAC_MODE_OFFSET));

    /* Step 3: Set inter frame gap */
    writel(0x18 << EMAC_IPGT_SHIFT, priv->base + EMAC_IPGT_OFFSET);
    dev_info(priv->dev, "EMAC_IPGT: 0x%08x\n", readl(priv->base + EMAC_IPGT_OFFSET));

    /* Step 4: Set MII interface */
    reg_val = ~EMAC_MIINOPRE | (priv->mii_clk_div << EMAC_CLKDIV_SHIFT);
    writel(reg_val, priv->base + EMAC_MIIMODE_OFFSET);
    dev_info(priv->dev, "EMAC_MIIMODE: 0x%08x\n", readl(priv->base + EMAC_MIIMODE_OFFSET));

    /* Step 5: Set collision configuration */
    reg_val = (0xf << EMAC_MAXFL_SHIFT) | (0x10 << EMAC_COLLVALID_SHIFT);
    writel(reg_val, priv->base + EMAC_COLLCONFIG_OFFSET);
    dev_info(priv->dev, "EMAC_COLLCONFIG: 0x%08x\n", readl(priv->base + EMAC_COLLCONFIG_OFFSET));

    /* Step 6: Set frame length */
    reg_val = (priv->min_frame_len << EMAC_MINFL_SHIFT) | 
              (priv->max_frame_len << EMAC_MAXFL_SHIFT);
    writel(reg_val, priv->base + EMAC_PACKETLEN_OFFSET);
    dev_info(priv->dev, "EMAC_PACKETLEN: 0x%08x\n", readl(priv->base + EMAC_PACKETLEN_OFFSET));
	
    /* Step 7: Set MAC address into hardware registers 
 // Set MAC address - Example: 00:11:22:33:44:55 
    priv->mac_addr[0] = 0x00;  // MAC address byte 0
    priv->mac_addr[1] = 0x11;  // MAC address byte 1
    priv->mac_addr[2] = 0x22;  // MAC address byte 2
    priv->mac_addr[3] = 0x33;  // MAC address byte 3
    priv->mac_addr[4] = 0x44;  // MAC address byte 4
    priv->mac_addr[5] = 0x55;  // MAC address byte 5
    reg_val = (priv->mac_addr[5] << EMAC_MAC_B5_SHIFT) |
              (priv->mac_addr[4] << EMAC_MAC_B4_SHIFT) |
              (priv->mac_addr[3] << EMAC_MAC_B3_SHIFT) |
              (priv->mac_addr[2] << EMAC_MAC_B2_SHIFT);
    writel(reg_val, priv->base + EMAC_MAC_ADDR0_OFFSET);
    dev_info(priv->dev, "EMAC_MAC_ADDR0: 0x%08x\n", readl(priv->base + EMAC_MAC_ADDR0_OFFSET));

    reg_val = (priv->mac_addr[1] << EMAC_MAC_B1_SHIFT) |
              (priv->mac_addr[0] << EMAC_MAC_B0_SHIFT);
    writel(reg_val, priv->base + EMAC_MAC_ADDR1_OFFSET);
    dev_info(priv->dev, "EMAC_MAC_ADDR1: 0x%08x\n", readl(priv->base + EMAC_MAC_ADDR1_OFFSET));
*/
    mac = priv->ndev->dev_addr;
    priv->mac_addr[0] = mac[0];  // MAC address byte 0
    priv->mac_addr[1] = mac[1];  // MAC address byte 1
    priv->mac_addr[2] = mac[2];  // MAC address byte 2
    priv->mac_addr[3] = mac[3];  // MAC address byte 3
    priv->mac_addr[4] = mac[4];  // MAC address byte 4
    priv->mac_addr[5] = mac[5];  // MAC address byte 5
    reg_val = (priv->mac_addr[5] << EMAC_MAC_B5_SHIFT) |
              (priv->mac_addr[4] << EMAC_MAC_B4_SHIFT) |
              (priv->mac_addr[3] << EMAC_MAC_B3_SHIFT) |
              (priv->mac_addr[2] << EMAC_MAC_B2_SHIFT);
    writel(reg_val, priv->base + EMAC_MAC_ADDR0_OFFSET);
    dev_info(priv->dev, "EMAC_MAC_ADDR0: 0x%08x\n", readl(priv->base + EMAC_MAC_ADDR0_OFFSET));

    reg_val = (priv->mac_addr[1] << EMAC_MAC_B1_SHIFT) |
              (priv->mac_addr[0] << EMAC_MAC_B0_SHIFT);
    writel(reg_val, priv->base + EMAC_MAC_ADDR1_OFFSET);
    dev_info(priv->dev, "EMAC_MAC_ADDR1: 0x%08x\n", readl(priv->base + EMAC_MAC_ADDR1_OFFSET));

    dev_info(priv->dev, "Using MAC address: %pM\n", priv->ndev->dev_addr);
    dev_info(priv->dev, "Using MAC address: %pM\n", priv->mac_addr);
    return 0;
}

static const struct net_device_ops emac_netdev_ops = { 
    .ndo_open       = emac_open,
    .ndo_stop       = emac_stop,
    .ndo_start_xmit = emac_start_xmit,
};
#if 0
static int emac_probe(struct platform_device *pdev)
{
    struct emac_priv *priv;
    struct resource *res;
    struct net_device *ndev;
    int ret;
    struct device_node *np = pdev->dev.of_node;
    const char *mac;
    int err;
    pr_info("EMAC: Probing device\n");
    pr_info("EMAC: Probing device//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n");

    ndev = alloc_etherdev(sizeof(struct emac_priv));
    if (!ndev)
        return -ENOMEM;

    platform_set_drvdata(pdev, ndev);
    priv = netdev_priv(ndev);
    priv->ndev = ndev;
    priv->dev = &pdev->dev;
    priv->pdev = pdev;


    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "No memory resource\n");
        ret = -ENODEV;
        goto err_free;
    }

    priv->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(priv->base)) {
        dev_err(&pdev->dev, "Failed to ioremap resource\n");
        ret = PTR_ERR(priv->base);
        goto err_free;
    }
    priv->irq = platform_get_irq(pdev, 0);
    if (priv->irq < 0) {
        ret = priv->irq;
        goto err_free;
    }

    ret = devm_request_irq(&pdev->dev, priv->irq, emac_interrupt_handler, 0,
                           dev_name(&pdev->dev), ndev);
    if (ret)
        goto err_free;
    mac = of_get_mac_address(np);
    if (PTR_ERR(mac) == -EPROBE_DEFER) {
	err = -EPROBE_DEFER;
	goto err_free;
	
     err = of_get_phy_mode(np);
	if (err < 0)
		/* not found in DT, MII by default */
		priv->phy_interface = PHY_INTERFACE_MODE_MII;
	else
		priv->phy_interface = err;



    emac_hw_init(priv);
    //err = emac_mii_init(priv);
   if (err)
	goto err_free;

    
    // Use a random MAC address for now
    eth_hw_addr_random(ndev);
    pr_info("EMAC: Assigned random MAC: %pM\n", ndev->dev_addr);

    ndev->netdev_ops = &emac_netdev_ops;

    ret = register_netdev(ndev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to register net device\n");
        goto err_free;
    }

    pr_info("EMAC: Probe complete, netdev registered as %s\n", ndev->name);
    return 0;

err_free:
    free_netdev(ndev);
    return ret;
}
#endif
static int emac_probe(struct platform_device *pdev)
{
    struct emac_priv *priv;
    struct resource *res;
    struct net_device *ndev;
    struct device_node *np = pdev->dev.of_node;
    struct phy_device *phydev;
    int ret;
    int err;
    pr_info("EMAC: Probing device\n");

    ndev = alloc_etherdev(sizeof(struct emac_priv));
    if (!ndev)
        return -ENOMEM;

    platform_set_drvdata(pdev, ndev);
    priv = netdev_priv(ndev);
    priv->ndev = ndev;
    priv->dev = &pdev->dev;
    priv->pdev = pdev;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "No memory resource\n");
        ret = -ENODEV;
        goto err_free;
    }
    ndev->base_addr = res->start;
    priv->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(priv->base)) {
        dev_err(&pdev->dev, "Failed to ioremap resource\n");
        ret = PTR_ERR(priv->base);
        goto err_free;
    }

    SET_NETDEV_DEV(ndev, &pdev->dev);
    priv->irq = platform_get_irq(pdev, 0);
    if (priv->irq < 0) {
        ret = priv->irq;
        goto err_free;
    }

    ret = devm_request_irq(&pdev->dev, priv->irq, emac_interrupt_handler, 0,
                           dev_name(&pdev->dev), ndev);
    if (ret)
        goto err_free;


    pr_info("EMAC: Assigned random MAC: %pM\n", ndev->dev_addr);
    err = of_get_phy_mode(np);
    if (err < 0){
	pr_info("funtion : %s    and  line : %d\n", __func__, __LINE__);
        priv->phy_interface = PHY_INTERFACE_MODE_MII;
    }
    else{
	pr_info("funtion : %s    and  line : %d\n", __func__, __LINE__);
        priv->phy_interface = err;
    }
    ndev->netdev_ops = &emac_netdev_ops;
    err =  emac_hw_init(priv);
    if(err){
	goto err_free;
    }
     err = emac_mii_init(priv); 

     if (err)
         goto err_free;
     phydev = ndev->phydev;
     netif_carrier_off(ndev);
    //ndev->netdev_ops = &emac_netdev_ops;

    ret = register_netdev(ndev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to register net device\n");
        goto err_free;
    }

    pr_info("EMAC: Probe complete, netdev registered as %s\n", ndev->name);
    return 0;

err_free:
    free_netdev(ndev);
    return ret;
}

static int emac_remove(struct platform_device *pdev)
{
#if 0
    struct net_device *ndev = platform_get_drvdata(pdev);
    struct emac_priv *priv;

    unregister_netdev(ndev);
    free_netdev(ndev);:vsp
#endif
        struct net_device *ndev = platform_get_drvdata(pdev);
	struct emac_priv *priv;
	struct device_node *np = pdev->dev.of_node;

	
	if (ndev) {
		priv = netdev_priv(ndev);
		if (ndev->phydev)
			phy_disconnect(ndev->phydev);
		mdiobus_unregister(priv->mii_bus);
		if (np && of_phy_is_fixed_link(np))
			of_phy_deregister_fixed_link(np);
		ndev->phydev = NULL;
		mdiobus_free(priv->mii_bus);

		unregister_netdev(ndev);
		of_node_put(priv->phy_node);
		free_netdev(ndev);
	}

    pr_info("[%s, %u]EMAC: Device removed\n", __func__, __LINE__);
    return 0;
}

static const struct of_device_id emac_of_match[] = {
    { .compatible = "flc,flc_emac" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, emac_of_match);

static struct platform_driver emac_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = emac_of_match,
    },
    .probe = emac_probe,
    .remove = emac_remove,
};

module_platform_driver(emac_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pavan G & Vishnu S");
MODULE_DESCRIPTION("FLC EMAC Basic Probe Driver (No MAC/IRQ yet)");
#if 0
//module_platform_driver(emac_driver);
static int __init emac_init(void)
{
    pr_info("EMAC built-in driver init\n");
    pr_info("EMAC built-in driver init/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n");
    return platform_driver_register(&emac_driver);
}

static void __exit emac_exit(void)
{
    platform_driver_unregister(&emac_driver);
}

arch_initcall(emac_init);
module_exit(emac_exit);

MODULE_AUTHOR("FLC");
MODULE_DESCRIPTION("EMAC Ethernet driver");
MODULE_LICENSE("GPL");
//MODULE_VERSION(DRV_VERSION);
#endif

