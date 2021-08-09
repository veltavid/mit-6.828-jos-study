#include <kern/pci.h>
#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/env.h>
#include <kern/sched.h>
#include <kern/trap.h>
#include <inc/string.h>

// LAB 6: Your driver code here
volatile uint32_t *mapped_va;
struct e1000_tx_desc tx_desc_array[MAX_TX_DESC];
char tx_buf[MAX_TX_DESC][MAX_PACKET_LEN];
struct e1000_rx_desc rx_desc_array[MAX_RX_DESC];
char rx_buf[MAX_RX_DESC][MAX_PACKET_LEN];

static void
tx_init()
{
    int i;

    mapped_va[E1000_TDBAL/4]=PADDR(tx_desc_array);
    mapped_va[E1000_TDBAH/4]=0;
    mapped_va[E1000_TDLEN/4]=MAX_TX_DESC*sizeof(struct e1000_tx_desc);
    mapped_va[E1000_TDH/4]=0;
    mapped_va[E1000_TDT/4]=0;
    mapped_va[E1000_TCTL/4]|=E1000_TCTL_EN;
    mapped_va[E1000_TCTL/4]|=E1000_TCTL_PSP;
    mapped_va[E1000_TCTL/4]|=E1000_TCTL_CT;
    mapped_va[E1000_TCTL/4]|=E1000_TCTL_COLD;
    mapped_va[E1000_TIPG/4]=E1000_TIPG_VAL;
    for(i=0;i<MAX_TX_DESC;i++)
    tx_desc_array[i].status|=E1000_TXD_STAT_DD;
}

static void
rx_init()
{
    int i;

    mapped_va[E1000_RA/4]=0x12005452;
    mapped_va[E1000_RA/4+1]=0x5634 | E1000_RAH_AV;
    mapped_va[E1000_MTA/4]=0;

    //mapped_va[E1000_IMC/4]=0xffff;
    //mapped_va[E1000_IMS/4]|=E1000_IMS_RXT0;
    //mapped_va[E1000_RDTR/4]|=E1000_ICR_INT_ASSERTED;

    mapped_va[E1000_RDBAL/4]=PADDR(rx_desc_array);
    mapped_va[E1000_RDBAH/4]=0;
    mapped_va[E1000_RDLEN/4]=MAX_RX_DESC*sizeof(struct e1000_rx_desc);
    mapped_va[E1000_RDH/4]=0;
    mapped_va[E1000_RDT/4]=MAX_RX_DESC-1;//sizeof(struct e1000_rx_desc)*(MAX_RX_DESC-1);

    mapped_va[E1000_RCTL/4]=E1000_RCTL_LBM_NO;
    mapped_va[E1000_RCTL/4]|=E1000_RCTL_EN;
    mapped_va[E1000_RCTL/4]|=E1000_RCTL_BAM;
    mapped_va[E1000_RCTL/4]|=E1000_RCTL_SECRC;
    for(i=0;i<MAX_RX_DESC;i++)
    rx_desc_array[i].addr=PADDR(rx_buf[i]);
}

int pci_e1000_attach(struct pci_func *pcif)
{
    char buf[MAX_PACKET_LEN];

    pci_func_enable(pcif);
    mapped_va=mmio_map_region(pcif->reg_base[0],pcif->reg_size[0]);
    tx_init();
    rx_init();

    return 0;
}

int tx_packet(void *buf,uint32_t size)
{
    int c_tdt;
    
    c_tdt=mapped_va[E1000_TDT/4];
    
    if(tx_desc_array[c_tdt].status & E1000_TXD_STAT_DD)
    {
        memmove(tx_buf[c_tdt],buf,size);
        tx_desc_array[c_tdt].addr=PADDR(tx_buf[c_tdt]);
        tx_desc_array[c_tdt].length=(uint16_t)size;
        tx_desc_array[c_tdt].cmd=E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
        mapped_va[E1000_TDT/4]=(c_tdt+1)%MAX_TX_DESC;
        return 0;
    }
    else
    return -E_TX_FAIL;
}

int rx_packet(void *buf,int *size)
{
    struct e1000_rx_desc *rx_desc;
    int c_rdt;

    c_rdt=mapped_va[E1000_RDT/4];
    c_rdt=(c_rdt+1)%MAX_RX_DESC;
    if(rx_desc_array[c_rdt].status & E1000_TXD_STAT_DD)
    {
        //cprintf("sss:%s\n%d %d %d\n",rx_buf[c_rdt],rx_desc_array[c_rdt].length,mapped_va[E1000_RDH/4],mapped_va[E1000_RDT/4]);
        memmove(buf,KADDR(rx_desc_array[c_rdt].addr),rx_desc_array[c_rdt].length);
        *size=rx_desc_array[c_rdt].length;
        mapped_va[E1000_RDT/4]=c_rdt;
        return 0;
    }
    else
    return -E_RX_FAIL;
}

/*void rsrpd_handler()
{
    int i;
    struct Env *target_env=0;
    cprintf("abcdddddddddddddddddddddddddddddddddddddddddddddddddd\n");
    for(i=0;i<NENV;i++)
    {
        if(envs[i].env_status==ENV_NOT_RUNNABLE && !envs[i].env_ipc_recving)
        {
            target_env=&envs[i];
            break;
        }
    }
    if(target_env)
    {
        rx_packet(target_env->env_ipc_dstva);
        target_env->env_status=ENV_RUNNABLE;
    }
    sched_yield();
}*/