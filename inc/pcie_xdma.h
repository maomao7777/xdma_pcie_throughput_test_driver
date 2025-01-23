#ifndef __XDMA_H__
#define __XDMA_H__

#include <linux/module.h>
#include <linux/kernel.h>

#include "pcie_xdma.h"
#include "phy_driver.h"



// /////////////////////////////////////////////////////////////////////////////
//    Macro declarations
// /////////////////////////////////////////////////////////////////////////////


// /////////////////////////////////////////////////////////////////////////////
//    Type declarations
// /////////////////////////////////////////////////////////////////////////////
#define XDMA_DESC_STOPPED	(1UL << 0)
#define XDMA_DESC_COMPLETED	(1UL << 1)
#define XDMA_DESC_EOP		(1UL << 4)


#define XDMA_CTRL_RUN_STOP			(1UL << 0)
#define XDMA_CTRL_IE_DESC_STOPPED		(1UL << 1)
#define XDMA_CTRL_IE_DESC_COMPLETED		(1UL << 2)
#define XDMA_CTRL_IE_DESC_ALIGN_MISMATCH	(1UL << 3)
#define XDMA_CTRL_IE_MAGIC_STOPPED		(1UL << 4)
#define XDMA_CTRL_IE_IDLE_STOPPED		(1UL << 6)
#define XDMA_CTRL_IE_READ_ERROR			(0x1FUL << 9)
#define XDMA_CTRL_IE_DESC_ERROR			(0x1FUL << 19)
#define XDMA_CTRL_NON_INCR_ADDR			(1UL << 25)
#define XDMA_CTRL_POLL_MODE_WB			(1UL << 26)
#define XDMA_CTRL_STM_MODE_WB			(1UL << 27)

#define DESC_MAGIC 0xAD4B0000UL
#define NUM_ENGINE_DMA_BUFFR 2048

#define NUM_USR_IRQ 4
#define NUM_TXCH_IRQ 2
#define NUM_RXCH_IRQ 2
#define MSIX_VEC_COUNT (NUM_USR_IRQ+NUM_TXCH_IRQ+NUM_RXCH_IRQ)
#define XDMA_TRANS_DEBUG 0


// /////////////////////////////////////////////////////////////////////////////
//    Variables declarations
// /////////////////////////////////////////////////////////////////////////////
#define DEFINE_KOBJ_ATTR(name, show_fn, store_fn) \
static struct kobj_attribute name##_attr = __ATTR(name, 0664, show_fn, store_fn);

#if XDMA_TRANS_DEBUG == 0
#define _XDMA_RESET_ALL() do { \
    _xdmaWrite(0x000c,1); \
    _xdmaWrite(0x0094,0x1f); \
    _xdmaWrite(0x010c,1); \
    _xdmaWrite(0x0194,0x1f); \
    _xdmaWrite(0x100c, 1); \
    _xdmaWrite(0x1094, 0x1f); \
    _xdmaWrite(0x110c, 1); \
    _xdmaWrite(0x1194, 0x1f); \
    _xdmaWrite(0x2014, 0xffffffff); \
} while (0)
#define _XDMA_RESET_TX0() do { \
    _xdmaWrite(0x0094,0x1f); \
    _xdmaWrite(0x2014, 0x1); \
} while (0)
#define _XDMA_RESET_TX1() do { \
    _xdmaWrite(0x0194,0x1f); \
    _xdmaWrite(0x2014, 0x2); \
} while (0)
#define _XDMA_RESET_RX0() do { \
    _xdmaWrite(0x1094, 0x1f); \
    _xdmaWrite(0x2014, 0x4); \
} while (0)
#define _XDMA_RESET_RX1() do { \
    _xdmaWrite(0x1194, 0x1f); \
    _xdmaWrite(0x2014, 0x8); \
} while (0)
#else
#define _XDMA_RESET_ALL() do { \
    _xdmaWrite(0x000c,1); \
    _xdmaWrite(0x0094,0xffffff); \
    _xdmaWrite(0x010c,1); \
    _xdmaWrite(0x0194,0xffffff); \
    _xdmaWrite(0x100c, 1); \
    _xdmaWrite(0x1094, 0xffffff); \
    _xdmaWrite(0x110c, 1); \
    _xdmaWrite(0x1194, 0xffffff); \
    _xdmaWrite(0x2014, 0xffffffff); \
} while (0)
#define _XDMA_RESET_TX0() do { \
    _xdmaWrite(0x0094,0xffffff); \
    _xdmaWrite(0x2014, 0x1); \
} while (0)
#define _XDMA_RESET_TX1() do { \
    _xdmaWrite(0x0194,0xffffff); \
    _xdmaWrite(0x2014, 0x2); \
} while (0)
#define _XDMA_RESET_RX0() do { \
    _xdmaWrite(0x1094, 0xffffff); \
    _xdmaWrite(0x2014, 0x4); \
} while (0)
#define _XDMA_RESET_RX1() do { \
    _xdmaWrite(0x1194, 0xffffff); \
    _xdmaWrite(0x2014, 0x8); \
} while (0)
#endif


#define XDMA_STAT_COMP(stat) ((stat&0x6)==0x6)


// /////////////////////////////////////////////////////////////////////////////
//    Functions
// /////////////////////////////////////////////////////////////////////////////

struct xdma_desc {
	u32 control;
	u32 bytes;		/* transfer length in bytes */
	u32 src_addr_lo;	/* source address (low 32-bit) */
	u32 src_addr_hi;	/* source address (high 32-bit) */
	u32 dst_addr_lo;	/* destination address (low 32-bit) */
	u32 dst_addr_hi;	/* destination address (high 32-bit) */
	/*
	 * next descriptor in the single-linked list of descriptors;
	 * this is the PCIe (bus) address of the next descriptor in the
	 * root complex memory
	 */
	u32 next_lo;		/* next desc address (low 32-bit) */
	u32 next_hi;		/* next desc address (high 32-bit) */
} __packed;
struct interrupt_regs {
	u32 identifier;
	u32 user_int_enable;
	u32 user_int_enable_w1s;
	u32 user_int_enable_w1c;
	u32 channel_int_enable;
	u32 channel_int_enable_w1s;
	u32 channel_int_enable_w1c;
	u32 reserved_1[9];	/* padding */

	u32 user_int_request;
	u32 channel_int_request;
	u32 user_int_pending;
	u32 channel_int_pending;
	u32 reserved_2[12];	/* padding */

	u32 user_msi_vector[8];
	u32 channel_msi_vector[8];
} __packed;
struct engine_regs {
	u32 identifier;
	u32 control;
	u32 control_w1s;
	u32 control_w1c;
	u32 reserved_1[12];	/* padding */

	u32 status;
	u32 status_rc;
	u32 completed_desc_count;
	u32 alignments;
	u32 reserved_2[14];	/* padding */

	u32 poll_mode_wb_lo;
	u32 poll_mode_wb_hi;
	u32 interrupt_enable_mask;
	u32 interrupt_enable_mask_w1s;
	u32 interrupt_enable_mask_w1c;
	u32 reserved_3[9];	/* padding */

	u32 perf_ctrl;
	u32 perf_cyc_lo;
	u32 perf_cyc_hi;
	u32 perf_dat_lo;
	u32 perf_dat_hi;
	u32 perf_pnd_lo;
	u32 perf_pnd_hi;
} __packed;

struct xdma_chnl {
    dma_addr_t dma_hdl;
    uint64_t __iomem *dma_buf;
    size_t dma_size;
    dma_addr_t xdma_desc_hdl;
    uint64_t __iomem *xdma_desc_buf;
    struct xdma_desc *pxdesc;
    int num_xdesc;
    int dir;
    int vector;
    struct workqueue_struct *wq;
    struct work_struct work;
    int wtype;
};
struct xdma_engine {
	dma_addr_t dma_hdl;
    uint64_t __iomem *dma_buf;
    dma_addr_t dma_nhdl[NUM_ENGINE_DMA_BUFFR];
    uint64_t __iomem *dma_nbuf[NUM_ENGINE_DMA_BUFFR];
	int dma_nbuf_alloced;
	size_t dma_nbuf_total_size;
    dma_addr_t xdma_desc_hdl;
    uint64_t __iomem *xdma_desc_buf;
    struct xdma_desc *pxdesc;
    int num_xdesc;
	int last_xdesc_tr;
    int dir;
	int chnl_idx;
    int vec_rel;
};

struct phy_pcie_dev {
    struct pci_dev *pdev;
    uint64_t __iomem *mmio_base0;
    uint64_t __iomem *mmio_base1;
    uint64_t __iomem *mmio_base2;
    dma_addr_t dma_handle;
    uint64_t __iomem *dma_buffer;
    size_t dma_size;
    struct msix_entry msix_entries[MSIX_VEC_COUNT];
	struct workqueue_struct *irq_wq[MSIX_VEC_COUNT];
	struct work_struct irq_works[MSIX_VEC_COUNT];
	spinlock_t lock_works [MSIX_VEC_COUNT]; 
    struct xdma_chnl xdma_test_ch[4];
    int vecusr1;
    int vecusr2;
	int num_xdesc;
    int num_xdesc_tx;
    int num_xdesc_rx;
};
int xdma_desc_control_set(struct xdma_desc *first, u32 control_field);
void xdma_desc_set(struct xdma_desc *desc, dma_addr_t rc_bus_addr,
			  u64 ep_addr, int len, int dir);
void prog_irq_msix_user(uint32_t h2c_chnl_max,uint32_t c2h_chnl_max, uint32_t user_max,bool clear);
void prog_irq_msix_channel(uint32_t h2c_chnl_max,uint32_t c2h_chnl_max, uint32_t user_max,bool clear);
int xdma_chnl_init_regs(int dir,int hw_indx);
void pci_enable_capability(struct pci_dev *pdev, int cap);
void pci_keep_intx_enabled(struct pci_dev *pdev);
void pci_check_intr_pend(struct pci_dev *pdev);

uint32_t _xdmaRead(int offst);
void _xdmaWrite(int offst, uint32_t value);
uint64_t _pifRead(int offst);
void _pifWrite(int offst, uint64_t value);
uint64_t _dmaRead(int offst);
void _dmaWrite(int offst, uint64_t value);
int xdma_engine_alloc_buf(struct xdma_engine * pxdma_eng, tPhyReqData *pReq);
int xdma_engine_free_buf(struct xdma_engine * pxdma_eng, tPhyReqData *pReq);
int xdma_engine_setup(struct xdma_engine * pxdma_eng, int ndes_tr, tPhyReqData *pReq);
int xdma_engine_trans(struct xdma_engine * pxdma_eng);

int xdma_engine_trans(struct xdma_engine * pxdma_eng)
{
    uint64_t v64;
    int off;

    if(!pxdma_eng->last_xdesc_tr)
        return -1;
    v64=(uint64_t)pxdma_eng->xdma_desc_hdl;
    if(pxdma_eng->dir==DMA_TO_DEVICE)
        off=0x4000+0x100*pxdma_eng->chnl_idx;
    else
        off=0x5000+0x100*pxdma_eng->chnl_idx;
    _xdmaWrite(0x80+off,cpu_to_le32(PCI_DMA_L(v64)));
    _xdmaWrite(0x84+off,cpu_to_le32(PCI_DMA_H(v64)));
    _xdmaWrite(0x88+off,pxdma_eng->last_xdesc_tr-1);
    if(pxdma_eng->dir==DMA_TO_DEVICE)
        off=0x8+0x100*pxdma_eng->chnl_idx;
    else
        off=0x1008+0x100*pxdma_eng->chnl_idx;
#if XDMA_TRANS_DEBUG
    _xdmaWrite(off,0xffffff);
#else
    _xdmaWrite(off,0xff);
#endif
    pxdma_eng->last_xdesc_tr=0;
    return 0;
}

int xdma_engine_setup(struct xdma_engine * pxdma_eng,int ndes_tr, tPhyReqData pReq[])
{
    uint64_t v64;
    uint32_t v32;
    struct xdma_engine *base= &phyDev->xdma_eng[0];
    struct xdma_desc *pxdesc;
    int i;
    int egidx;
    int cell=pReq->cellNum;
    dma_addr_t root_host_addr;
	u64 endpoint_addr;
    egidx = pxdma_eng - base;
    if (!ndes_tr)
        return -1;
    if(pxdma_eng->last_xdesc_tr>0)
    {
        pxdesc=pxdma_eng->pxdesc+(pxdma_eng->last_xdesc_tr-1);
        pxdesc->control=cpu_to_le32(DESC_MAGIC);
        v64=(uint64_t)(pxdma_eng->xdma_desc_hdl+(pxdma_eng->last_xdesc_tr)*sizeof(struct xdma_desc));
        pxdesc->next_lo=cpu_to_le32(PCI_DMA_L(v64));
        pxdesc->next_hi=cpu_to_le32(PCI_DMA_H(v64));
    }

    for(i=0;i<ndes_tr;i++)
    {
        pxdesc=pxdma_eng->pxdesc+(pxdma_eng->last_xdesc_tr+i);
        pxdesc->control=cpu_to_le32(DESC_MAGIC);
        root_host_addr=(pxdma_eng->dir==DMA_TO_DEVICE)?pReq[i].srcAddr[cell]:pReq[i].dstAddr[cell];
        endpoint_addr=(pxdma_eng->dir==DMA_TO_DEVICE)?pReq[i].dstAddr[cell]:pReq[i].srcAddr[cell];
        xdma_desc_set(pxdesc
        ,root_host_addr
        ,endpoint_addr
        ,pReq[i].dataLen[cell]
        ,pxdma_eng->dir);
        if(i<ndes_tr-1)
        {
            v64=(uint64_t)(pxdma_eng->xdma_desc_hdl+(i+1)*sizeof(struct xdma_desc));
            pxdesc->next_lo=cpu_to_le32(PCI_DMA_L(v64));
            pxdesc->next_hi=cpu_to_le32(PCI_DMA_H(v64));
        }
        else
        {
		        //printk("set chnl last desc[%d] eng %d\n",i,egidx);
                v32 = XDMA_DESC_STOPPED;
                v32 |= XDMA_DESC_EOP;
                v32 |= XDMA_DESC_COMPLETED;
                xdma_desc_control_set(pxdesc,v32);
        }
    }
    
    pxdma_eng->last_xdesc_tr+=ndes_tr;
    return 0;
}
int xdma_engine_free_buf(struct xdma_engine * pxdma_eng, tPhyReqData *pReq)
{
    int cell=pReq->cellNum;
    struct pci_dev *pdev=phyDev->pdev;
    dma_free_coherent(&pdev->dev, pReq->bufSize[cell], pReq->pBuf[cell], pReq->srcAddr[cell]);
    pxdma_eng->dma_nbuf_total_size-=pReq->bufSize[cell];
    pxdma_eng->dma_nbuf_alloced--;
    return 0;
}
int xdma_engine_alloc_buf(struct xdma_engine * pxdma_eng, tPhyReqData *pReq)
{
    struct pci_dev *pdev=phyDev->pdev;
    struct xdma_engine *base= &phyDev->xdma_eng[0];
    int cell=pReq->cellNum;
    int egidx = pxdma_eng - base;
    int n=pxdma_eng->dma_nbuf_alloced;

    if(n>=NUM_ENGINE_DMA_BUFFR){
        printk("failed xdma_eng[%d]->nbuf[%d] alloc \n",egidx,n);
        return -1;
    }

    pxdma_eng->dma_nbuf[n] = dma_alloc_coherent(&pdev->dev, pReq->bufSize[cell],
                                                &pxdma_eng->dma_nhdl[n], GFP_KERNEL);
    if (!pxdma_eng->dma_nbuf[n]) {
        printk("failed xdma_eng[%d]->nbuf[%d] alloc \n",egidx,n);
        return -1;
    }
    //pxdma_eng->dma_nsize[n]=pReq->bufSize[cell];
    pxdma_eng->dma_buf=pxdma_eng->dma_nbuf[n];
    pxdma_eng->dma_hdl=pxdma_eng->dma_nhdl[n];
    pxdma_eng->dma_nbuf_total_size+=pReq->bufSize[cell];
    pxdma_eng->dma_nbuf_alloced++;
    return  0;
}
int xdma_engine_init(struct xdma_engine * pxdma_eng, int dir, int hw_index)
{
    struct pci_dev *pdev=phyDev->pdev;
    struct xdma_engine *base= &phyDev->xdma_eng[0];
    int ret=0;
    int egidx;
    egidx = pxdma_eng - base;
    pxdma_eng->vec_rel = (dir-DMA_TO_DEVICE)*2+hw_index ;
    pxdma_eng->dir=dir;
    pxdma_eng->chnl_idx=hw_index;
    pxdma_eng->num_xdesc= num_eg_desc_array[egidx];
    pxdma_eng->dma_nbuf_total_size = 0;
    pxdma_eng->last_xdesc_tr = 0;

    pxdma_eng->xdma_desc_buf = dma_alloc_coherent(&pdev->dev, pxdma_eng->num_xdesc*sizeof(struct xdma_desc),
                                            &pxdma_eng->xdma_desc_hdl, GFP_KERNEL);
    if (!pxdma_eng->xdma_desc_buf) {
        ret = -ENOMEM;
        goto err_dma;
    }

    printk(KERN_INFO "Allocated XDMA engine[%d] Desc Start at 0x%llx, vmem 0x%llx\n", 
    egidx,
    (unsigned long long)pxdma_eng->xdma_desc_hdl,
    (unsigned long long)pxdma_eng->xdma_desc_buf);
    pxdma_eng->pxdesc=(struct xdma_desc *)pxdma_eng->xdma_desc_buf;
    memset(pxdma_eng->pxdesc,0,pxdma_eng->num_xdesc*sizeof(struct xdma_desc));
    return ret;

    err_dma:
    return ret;
}
void xdma_engine_free(struct xdma_engine * pxdma_eng)
{
    struct pci_dev *pdev=phyDev->pdev;
    dma_free_coherent(&pdev->dev, pxdma_eng->num_xdesc*sizeof(struct xdma_desc), pxdma_eng->xdma_desc_buf, pxdma_eng->xdma_desc_hdl);
}


#endif 
