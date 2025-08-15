#ifndef __EMAC_REG_H__
#define __EMAC_REG_H__

// Generic index increment macro
#define NEXT_INDEX(idx, size) (((idx) + 1) % (size))
/* Value Definitions */
#define TX_IRQ_PACKET_THRESHOLD   8       
#define TX_IRQ_TIME_THRESHOLD_NS (10 * 1000 * 1000) 
/* PACKET LENGTH */
#define EMAC_MAXFL       (1536)   
#define EMAC_MINFL       (64)    
/* RX Buffer Size for dma mapping */
#define RX_BUF_SIZE (1536)

/* Register offsets *********************************************************/
#define EMAC_MODE_OFFSET       (0x0)
#define EMAC_INT_SOURCE_OFFSET (0x4)
#define EMAC_INT_MASK_OFFSET   (0x8)
#define EMAC_IPGT_OFFSET       (0xC)
#define EMAC_PACKETLEN_OFFSET  (0x18)
#define EMAC_COLLCONFIG_OFFSET (0x1C)
#define EMAC_TX_BD_NUM_OFFSET  (0x20)
#define EMAC_MIIMODE_OFFSET    (0x28)
#define EMAC_MIICOMMAND_OFFSET (0x2C)
#define EMAC_MIIADDRESS_OFFSET (0x30)
#define EMAC_MIITX_DATA_OFFSET (0x34)
#define EMAC_MIIRX_DATA_OFFSET (0x38)
#define EMAC_MIISTATUS_OFFSET  (0x3C)
#define EMAC_MAC_ADDR0_OFFSET  (0x40)
#define EMAC_MAC_ADDR1_OFFSET  (0x44)
#define EMAC_HASH0_ADDR_OFFSET (0x48)
#define EMAC_HASH1_ADDR_OFFSET (0x4C)
#define EMAC_TXCTRL_OFFSET     (0x50)
#define EMAC_DMA_DESC_OFFSET   (0x400)

/* 0x0 : EMAC MODE config */
#define EMAC_RX_EN    (1 << 0U)
#define EMAC_TX_EN    (1 << 1U)
#define EMAC_NOPRE    (1 << 2U)
#define EMAC_BRO      (1 << 3U)
#define EMAC_PRO      (1 << 5U)
#define EMAC_IFG      (1 << 6U)
#define EMAC_FULLD    (1 << 10U)
#define EMAC_CRCEN    (1 << 13U)
#define EMAC_HUGEN    (1 << 14U)
#define EMAC_PAD      (1 << 15U)
#define EMAC_RECSMALL (1 << 16U)
#define EMAC_RMII_EN  (1 << 17U)

/* 0x4 : INT_SOURCE */
#define EMAC_TXB  (1 << 0U)
#define EMAC_TXE  (1 << 1U)
#define EMAC_RXB  (1 << 2U)
#define EMAC_RXE  (1 << 3U)
#define EMAC_BUSY (1 << 4U)
#define EMAC_TXC  (1 << 5U)
#define EMAC_RXC  (1 << 6U)

/* 0x8 : INT_MASK */
#define EMAC_TXB_M  (1 << 0U)
#define EMAC_TXE_M  (1 << 1U)
#define EMAC_RXB_M  (1 << 2U)
#define EMAC_RXE_M  (1 << 3U)
#define EMAC_BUSY_M (1 << 4U)
#define EMAC_TXC_M  (1 << 5U)
#define EMAC_RXC_M  (1 << 6U)

/* 0xC : IPGT */
#define EMAC_IPGT_SHIFT (0U)
#define EMAC_IPGT_MASK  (0x7f << EMAC_IPGT_SHIFT)

/* 0x18 : PACKETLEN */
#define EMAC_MAXFL_SHIFT (0U)
#define EMAC_MAXFL_MASK  (0xffff << EMAC_MAXFL_SHIFT)
#define EMAC_MINFL_SHIFT (16U)
#define EMAC_MINFL_MASK  (0xffff << EMAC_MINFL_SHIFT)

/* 0x1C : COLLCONFIG */
#define EMAC_COLLVALID_SHIFT (0U)
#define EMAC_COLLVALID_MASK  (0x3F << EMAC_COLLVALID_SHIFT)
#define EMAC_MAXRET_SHIFT    (16U)
#define EMAC_MAXRET_MASK     (0xF << EMAC_MAXRET_SHIFT)

/* 0x20 : TX_BD_NUM */
#define EMAC_TXBDNUM_SHIFT (0U)
#define EMAC_TXBDNUM_MASK  (0xff << EMAC_TXBDNUM_SHIFT)
#define EMAC_TXBDPTR_SHIFT (16U)
#define EMAC_TXBDPTR_MASK  (0x7f << EMAC_TXBDPTR_SHIFT)
#define EMAC_RXBDPTR_SHIFT (24U)
#define EMAC_RXBDPTR_MASK  (0x7f << EMAC_RXBDPTR_SHIFT)

/* 0x28 : MIIMODE */
#define EMAC_CLKDIV_SHIFT (0U)
#define EMAC_CLKDIV_MASK  (0xff << EMAC_CLKDIV_SHIFT)
#define EMAC_MIINOPRE     (1 << 8U)

/* 0x2C : MIICOMMAND */
#define EMAC_SCANSTAT  (1 << 0U)
#define EMAC_RSTAT     (1 << 1U)
#define EMAC_WCTRLDATA (1 << 2U)

/* 0x30 : MIIADDRESS */
#define EMAC_FIAD_SHIFT (0U)
#define EMAC_FIAD_MASK  (0x1f << EMAC_FIAD_SHIFT)
#define EMAC_RGAD_SHIFT (8U)
#define EMAC_RGAD_MASK  (0x1f << EMAC_RGAD_SHIFT)

/* 0x34 : MIITX_DATA */
#define EMAC_CTRLDATA_SHIFT (0U)
#define EMAC_CTRLDATA_MASK  (0xffff << EMAC_CTRLDATA_SHIFT)

/* 0x38 : MIIRX_DATA */
#define EMAC_PRSD_SHIFT (0U)
#define EMAC_PRSD_MASK  (0xffff << EMAC_PRSD_SHIFT)

/* 0x3C : MIISTATUS */
#define EMAC_MIIM_LINKFAIL (1 << 0U)
#define EMAC_MIIM_BUSY     (1 << 1U)

/* 0x40 : MAC_ADDR0 */
#define EMAC_MAC_B5_SHIFT (0U)
#define EMAC_MAC_B5_MASK  (0xff << EMAC_MAC_B5_SHIFT)
#define EMAC_MAC_B4_SHIFT (8U)
#define EMAC_MAC_B4_MASK  (0xff << EMAC_MAC_B4_SHIFT)
#define EMAC_MAC_B3_SHIFT (16U)
#define EMAC_MAC_B3_MASK  (0xff << EMAC_MAC_B3_SHIFT)
#define EMAC_MAC_B2_SHIFT (24U)
#define EMAC_MAC_B2_MASK  (0xff << EMAC_MAC_B2_SHIFT)

/* 0x44 : MAC_ADDR1 */
#define EMAC_MAC_B1_SHIFT (0U)
#define EMAC_MAC_B1_MASK  (0xff << EMAC_MAC_B1_SHIFT)
#define EMAC_MAC_B0_SHIFT (8U)
#define EMAC_MAC_B0_MASK  (0xff << EMAC_MAC_B0_SHIFT)

/* 0x48 : HASH0_ADDR */
#define EMAC_HASH0_SHIFT (0U)
#define EMAC_HASH0_MASK  (0xffffffff << EMAC_HASH0_SHIFT)

/* 0x4C : HASH1_ADDR */
#define EMAC_HASH1_SHIFT (0U)
#define EMAC_HASH1_MASK  (0xffffffff << EMAC_HASH1_SHIFT)

/* 0x50 : TXCTRL */
#define EMAC_TXPAUSETV_SHIFT (0U)
#define EMAC_TXPAUSETV_MASK  (0xffff << EMAC_TXPAUSETV_SHIFT)
#define EMAC_TXPAUSERQ_SHIFT (16U)
#define EMAC_TXPAUSERQ_MASK  (0x1 << EMAC_TXPAUSETV_SHIFT)

/* 0x400 ：EAMC DMA BD DESC */
/* EMAC TX BD DESC BASE: (TX_BD_NUM * 8) */
#define EMAC_BD_TX_CS_SHIFT   (0) /*!< Carrier Sense Lost */
#define EMAC_BD_TX_CS_MASK    (1 << EMAC_BD_TX_CS_SHIFT)
#define EMAC_BD_TX_DF_SHIFT   (1) /*!< Defer Indication */
#define EMAC_BD_TX_DF_MASK    (1 << EMAC_BD_TX_DF_SHIFT)
#define EMAC_BD_TX_LC_SHIFT   (2) /*!< Late Collision */
#define EMAC_BD_TX_LC_MASK    (1 << EMAC_BD_TX_LC_SHIFT)
#define EMAC_BD_TX_RL_SHIFT   (3) /*!< Retransmission Limit */
#define EMAC_BD_TX_RL_MASK    (1 << EMAC_BD_TX_RL_SHIFT)
#define EMAC_BD_TX_RTRY_SHIFT (4) /*!< Retry Count */
#define EMAC_BD_TX_RTRY_MASK  (4 << EMAC_BD_TX_RTRY_SHIFT)
#define EMAC_BD_TX_UR_SHIFT   (8) /*!< Underrun */
#define EMAC_BD_TX_UR_MASK    (1 << EMAC_BD_TX_UR_SHIFT)
#define EMAC_BD_TX_EOF_SHIFT  (10) /*!< EOF */
#define EMAC_BD_TX_EOF_MASK   (1 << EMAC_BD_TX_EOF_SHIFT)
#define EMAC_BD_TX_CRC_SHIFT  (11) /*!< CRC Enable */
#define EMAC_BD_TX_CRC_MASK   (1 << EMAC_BD_TX_CRC_SHIFT)
#define EMAC_BD_TX_PAD_SHIFT  (12) /*!< PAD enable */
#define EMAC_BD_TX_PAD_MASK   (1 << EMAC_BD_TX_PAD_SHIFT)
#define EMAC_BD_TX_WR_SHIFT   (13) /*!< Wrap */
#define EMAC_BD_TX_WR_MASK    (1 << EMAC_BD_TX_WR_SHIFT)
#define EMAC_BD_TX_IRQ_SHIFT  (14) /*!< Interrupt Request Enable */
#define EMAC_BD_TX_IRQ_MASK   (1 << EMAC_BD_TX_IRQ_SHIFT)
#define EMAC_BD_TX_RD_SHIFT   (15) /*!< The data buffer is ready for transmission or is currently being transmitted. You are not allowed to change it */
#define EMAC_BD_TX_RD_MASK    (1 << EMAC_BD_TX_RD_SHIFT)
#define EMAC_BD_TX_LEN_SHIFT  (16) /*!< TX Data buffer length */
#define EMAC_BD_TX_LEN_MASK   (0xffff << EMAC_BD_TX_LEN_SHIFT)

/* RX BD DESC BASE: ((TX_BD_NUM + RX_BD_NUM) * 8) */
#define EMAC_BD_RX_LC_SHIFT  (0) /*!< Late Collision */
#define EMAC_BD_RX_LC_MASK   (1 << EMAC_BD_RX_LC_SHIFT)
#define EMAC_BD_RX_CRC_SHIFT (1) /*!< RX CRC Error */
#define EMAC_BD_RX_CRC_MASK  (1 << EMAC_BD_RX_CRC_SHIFT)
#define EMAC_BD_RX_SF_SHIFT  (2) /*!< Short Frame */
#define EMAC_BD_RX_SF_MASK   (1 << EMAC_BD_RX_SF_SHIFT)
#define EMAC_BD_RX_TL_SHIFT  (3) /*!< Too Long */
#define EMAC_BD_RX_TL_MASK   (1 << EMAC_BD_RX_TL_SHIFT)
#define EMAC_BD_RX_DN_SHIFT  (4) /*!< Dribble Nibble */
#define EMAC_BD_RX_DN_MASK   (1 << EMAC_BD_RX_DN_SHIFT)
#define EMAC_BD_RX_RE_SHIFT  (5) /*!< Receive Error */
#define EMAC_BD_RX_RE_MASK   (1 << EMAC_BD_RX_RE_SHIFT)
#define EMAC_BD_RX_OR_SHIFT  (6) /*!< Overrun */
#define EMAC_BD_RX_OR_MASK   (1 << EMAC_BD_RX_OR_SHIFT)
#define EMAC_BD_RX_M_SHIFT   (7) /*!< Miss */
#define EMAC_BD_RX_M_MASK    (1 << EMAC_BD_RX_M_SHIFT)
#define EMAC_BD_RX_CF_SHIFT  (8) /*!< Control Frame Received */
#define EMAC_BD_RX_CF_MASK   (1 << EMAC_BD_RX_CF_SHIFT)
#define EMAC_BD_RX_WR_SHIFT  (13) /*!< Wrap */
#define EMAC_BD_RX_WR_MASK   (1 << EMAC_BD_RX_WR_SHIFT)
#define EMAC_BD_RX_IRQ_SHIFT (14) /*!< Interrupt Request Enable */
#define EMAC_BD_RX_IRQ_MASK  (1 << EMAC_BD_RX_IRQ_SHIFT)
#define EMAC_BD_RX_E_SHIFT   (15) /*!< The data buffer is empty (and ready for receiving data) or currently receiving data */
#define EMAC_BD_RX_E_MASK    (1 << EMAC_BD_RX_E_SHIFT)
#define EMAC_BD_RX_LEN_SHIFT (16) /*!< RX Data buffer length */
#define EMAC_BD_RX_LEN_MASK  (0xffff << EMAC_BD_RX_LEN_SHIFT)

/* MAX BD DESC 0x7FF */

#endif /* __EMAC_REG_H__ */
