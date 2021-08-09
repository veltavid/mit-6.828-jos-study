#include <kern/pci.h>

#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#endif  // SOL >= 6

#define MAX_TX_DESC 64
#define MAX_RX_DESC 128
#define MAX_PACKET_LEN 1518

#define E1000_STATUS   0x00008  /* Device Status - RO */
#define E1000_ICR      0x000C0  /* Interrupt Cause Read - R/clr */
#define E1000_ICS      0x000C8  /* Interrupt Cause Set - WO */
#define E1000_IMS      0x000D0  /* Interrupt Mask Set - RW */
#define E1000_IMC      0x000D8  /* Interrupt Mask Clear - WO */
#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */
#define E1000_RDTR     0x02820  /* RX Delay Timer - RW */
#define E1000_RSRPD    0x02C00  /* RX Small Packet Detect - RW */
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */
#define E1000_MTA      0x05200  /* Multicast Table Array - RW Array */
#define E1000_RA       0x05400  /* Receive Address - RW Array */

#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000100    /* collision threshold */
#define E1000_TCTL_COLD   0x00040000    /* collision distance */

#define E1000_TIPG_VAL    (6<<20) | (8<<10) | 10

#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_LBM_NO         0x00000000    /* no loopback mode */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */

#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */

#define E1000_TXD_CMD_EOP    0x01 /* End of Packet */
#define E1000_TXD_CMD_RS     0x08 /* Report Status */
#define E1000_TXD_CMD_RPS    0x10 /* Report Packet Sent */
#define E1000_TXD_STAT_DD    0x01 /* Descriptor Done */

#define E1000_IMS_RXT0      0x00000080      /* rx timer intr */
#define E1000_ICR_INT_ASSERTED  0x80000000 /* If this bit asserted, the driver should claim the interrupt */

#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */

#define E_TX_FAIL 4
#define E_RX_FAIL 5

struct e1000_tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

struct e1000_rx_desc {
    uint64_t addr; /* Address of the descriptor's data buffer */
    uint16_t length;     /* Length of data DMAed into data buffer */
    uint16_t csum;       /* Packet checksum */
    uint8_t status;      /* Descriptor status */
    uint8_t errors;      /* Descriptor Errors */
    uint16_t special;
};

int pci_e1000_attach(struct pci_func *pcif);
int tx_packet(void *buf,uint32_t size);
int rx_packet(void *buf,int *size);
//void rsrpd_handler();