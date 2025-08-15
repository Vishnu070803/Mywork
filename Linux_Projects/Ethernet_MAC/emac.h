#ifndef __LINUX_EMAC_DRIVER_H__
#define __LINUX_EMAC_DRIVER_H__


#include <linux/phy.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/net_tstamp.h>
#include <linux/interrupt.h>

#define EMAC_BASE_ADDR 0xF4048000 
#define EMAC_REG_SIZE       0x1000   
#define EMAC_DO_FLUSH_DATA (1)

#define EMAC_MAX_QUEUES 8


/* EMAC clock use external or use internal; 0: used external 1: used internal */
#define EMAC_CLK_USE_EXTERNAL (0)
#define EMAC_CLK_USE_INTERNAL (1)

/** @defgroup EMAC_CMD emac feature control cmd definition
  * @{
  */
#define EMAC_CMD_NO_PREAMBLE_MODE (0x01)
#define EMAC_CMD_EN_PROMISCUOUS   (0x02)
#define EMAC_CMD_FRAME_GAP_CHECK  (0x03)
#define EMAC_CMD_FULL_DUPLEX      (0x04)
#define EMAC_CMD_EN_TX_CRC_FIELD  (0x05)
#define EMAC_CMD_RECV_HUGE_FRAMES (0x06)
#define EMAC_CMD_EN_AUTO_PADDING  (0x07)
#define EMAC_CMD_RECV_SMALL_FRAME (0x08)
#define EMAC_CMD_SET_PHY_ADDRESS  (0x09)
#define EMAC_CMD_SET_MAC_ADDRESS  (0x0A)
#define EMAC_CMD_SET_PACKET_GAP   (0x0B)
#define EMAC_CMD_SET_MIN_FRAME    (0x0C)
#define EMAC_CMD_SET_MAX_FRAME    (0x0D)
#define EMAC_CMD_SET_MAXRET       (0x0E)
#define EMAC_CMD_SET_COLLVALID    (0x0F)
/**
  * @}
  */

/** @defgroup PHY_STATE phy state definition
  * @{
  */
#define PHY_STATE_DOWN    (0) /* PHY is not usable */
#define PHY_STATE_READY   (1) /* PHY is OK, wait for controller */
#define PHY_STATE_UP      (2) /* Network is ready for TX/RX */
#define PHY_STATE_RUNNING (3) /* working */
#define PHY_STATE_NOLINK  (4) /* no cable connected */
#define PHY_STATE_STOPPED (5) /* PHY has been stopped */
#define PHY_STATE_TESTING (6) /* in test mode */
/**
  * @}
  */

/* EMAC PACKET */
#define EMAC_NORMAL_PACKET   (uint32_t)(0)
#define EMAC_FRAGMENT_PACKET (uint32_t)(0x01)
#define EMAC_NOCOPY_PACKET   (uint32_t)(0x02)

/* ETH packet size */
/* ETH     | Header | Extra | VLAN tag | Payload   | CRC | */
/* Size    | 14     | 2     | 4        | 46 ~ 1500 | 4   | */
#define ETH_MAX_PACKET_SIZE          ((uint32_t)1524U) /*!< ETH_HEADER + ETH_EXTRA + ETH_VLAN_TAG + ETH_MAX_ETH_PAYLOAD + ETH_CRC */
#define ETH_MAX_PKT_LEN          ((uint32_t)1524U) /*!< ETH_HEADER + ETH_EXTRA + ETH_VLAN_TAG + ETH_MAX_ETH_PAYLOAD + ETH_CRC */
#define ETH_HEADER_SZIE              ((uint32_t)14U)   /*!< 6 byte Dest addr, 6 byte Src addr, 2 byte length/type */
#define ETH_CRC_SIZE                 ((uint32_t)4U)    /*!< Ethernet CRC */
#define ETH_EXTRA_SIZE               ((uint32_t)2U)    /*!< Extra bytes in some cases */
#define ETH_VLAN_TAG_SIZE            ((uint32_t)4U)    /*!< optional 802.1q VLAN Tag */
#define ETH_MIN_ETH_PAYLOAD_SIZE     ((uint32_t)46U)   /*!< Minimum Ethernet payload size */
#define ETH_MAX_ETH_PAYLOAD_SIZE     ((uint32_t)1500U) /*!< Maximum Ethernet payload size */
#define ETH_JUMBO_FRAME_PAYLOAD_SIZE ((uint32_t)9000U) /*!< Jumbo frame payload size */

/* ETH tx & rx buffer size */
#ifndef ETH_TX_BUFFER_SIZE
#define ETH_TX_BUFFER_SIZE (ETH_MAX_PACKET_SIZE)
#endif
#ifndef ETH_RX_BUFFER_SIZE
#define ETH_RX_BUFFER_SIZE (ETH_MAX_PACKET_SIZE)
#endif

/* emac interrupt UNMASK/MASK define */
#define EMAC_INT_EN_TX_DONE  (1 << 0)
#define EMAC_INT_EN_TX_ERROR (1 << 1)
#define EMAC_INT_EN_RX_DONE  (1 << 2)
#define EMAC_INT_EN_RX_ERROR (1 << 3)
#define EMAC_INT_EN_RX_BUSY  (1 << 4)
#define EMAC_INT_EN_TX_CTRL  (1 << 5)
#define EMAC_INT_EN_RX_CTRL  (1 << 6)
#define EMAC_INT_EN_ALL      (0x7f << 0)

/* emac interrupt status define */
#define EMAC_INT_STS_TX_DONE  (1 << 0)
#define EMAC_INT_STS_TX_ERROR (1 << 1)
#define EMAC_INT_STS_RX_DONE  (1 << 2)
#define EMAC_INT_STS_RX_ERROR (1 << 3)
#define EMAC_INT_STS_RX_BUSY  (1 << 4)
#define EMAC_INT_STS_TX_CTRL  (1 << 5)
#define EMAC_INT_STS_RX_CTRL  (1 << 6)
#define EMAC_INT_STS_ALL      (0x7f << 0)

/* emac buffer descriptors type define */
#define EMAC_BD_TYPE_INVLAID (0)
#define EMAC_BD_TYPE_TX      (1)
#define EMAC_BD_TYPE_RX      (2)
#define EMAC_BD_TYPE_NONE    (3)
#define EMAC_BD_TYPE_MAX     (0x7FFFFFFF)
#define EMAC_DESC_NUM_TOTAL 128
#define EMAC_TX_DESC_NUM 32   // For example, 32 TX descriptors
#define EMAC_RX_DESC_NUM (EMAC_DESC_NUM_TOTAL - EMAC_TX_DESC_NUM)
/* Buffer descriptor structure */
struct emac_bd_desc {
    u32 c_s_l;      /* Control/Status/Length */
    u32 buffer;     /* Buffer address */
};

/* EMAC private data */
struct emac_priv {
    struct net_device *ndev;
    struct platform_device *pdev;
    struct device *dev;
    void __iomem *base;
    int irq;
    
    /* Buffer descriptors */
    struct emac_bd_desc *bd_base;
    dma_addr_t bd_dma;
    
    /* TX/RX buffers */
    u8 *tx_bufs;
    u8 *rx_bufs;
    dma_addr_t tx_dma;
    dma_addr_t rx_dma;
    
    /* Buffer management */
    u8 tx_index_emac;
    u8 tx_index_cpu;
    u8 tx_buff_limit;
    u8 rx_index_emac;
    u8 rx_index_cpu;
    u8 rx_buff_limit;
    
    /* Locking */
    spinlock_t lock;
    struct napi_struct napi;
    
    /* PHY */
    struct phy_device *phydev;
    phy_interface_t  phy_interface; 
    struct mii_bus	*mii_bus;
    struct device_node	*phy_node;
    struct phy		*mii_phy;
    struct phylink	*phylink;
    int  link;
    int   speed;
    int   duplex;

    /* Configuration */
    u8 mac_addr[6];
    u8 mii_clk_div ;
    u16 min_frame_len;
    u16 max_frame_len;
    
    /* Statistics */
    unsigned long rx_packets;
    unsigned long tx_packets;
};
static int emac_phylink_connect(struct emac_priv *priv);
static int emac_init_bd_list(struct emac_priv *priv);
static void emac_reset_hw(struct emac_priv *priv);
static void emac_free_bd_list(struct emac_priv *priv);




/* Register offsets and bit definitions */
#include "emac_reg.h"
#if 0
/* Driver functions */
int emac_probe(struct platform_device *pdev);
int emac_remove(struct platform_device *pdev);

/* Network interface functions */
int emac_open(struct net_device *dev);
int emac_stop(struct net_device *dev);
netdev_tx_t emac_start_xmit(struct sk_buff *skb, struct net_device *dev);
void emac_set_rx_mode(struct net_device *dev);
struct net_device_stats *emac_get_stats(struct net_device *dev);

/* Interrupt handler */
irqreturn_t emac_interrupt(int irq, void *dev_id);

/* NAPI poll function */
int emac_poll(struct napi_struct *napi, int budget);

/* PHY functions */
int emac_mdio_read(struct mii_bus *bus, int phy_id, int regnum);
int emac_mdio_write(struct mii_bus *bus, int phy_id, int regnum, u16 val);
#endif
#endif /* __LINUX_EMAC_DRIVER_H__ */
