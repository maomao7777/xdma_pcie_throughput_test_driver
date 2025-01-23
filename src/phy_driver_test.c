/** 
 * @file       phy_driver_test.c
 * @brief      Implementation file of PHY driver test.
 * @details    
 * @author     mao
 * @date       $Date$
 * @version    $Revision$

 */


#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/scatterlist.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/aer.h>
#include "pcie_xdma.h"
#include "phy_driver.h"


// /////////////////////////////////////////////////////////////////////////////
//    Macro declarations
// /////////////////////////////////////////////////////////////////////////////


// /////////////////////////////////////////////////////////////////////////////
//    Type declarations
// /////////////////////////////////////////////////////////////////////////////



// /////////////////////////////////////////////////////////////////////////////
//    Variables declarations
// /////////////////////////////////////////////////////////////////////////////


// /////////////////////////////////////////////////////////////////////////////
//    Functions
// /////////////////////////////////////////////////////////////////////////////

#define HL_TEST 0
#define DMA_SIZE_TEST 204800 // should be 2048*n
extern struct phy_pcie_dev *phyDev;
extern struct kobject *phyKobj;

extern uint64_t __iomem *g_pPifReg;

extern int g_msix_vec_cnt;
static int dbg_err_cnt = 0;

#define MAX_ALLOC_DMA 1
uint64_t __iomem *pDmaBuft1[MAX_ALLOC_DMA];
dma_addr_t pDmaHdlt1[MAX_ALLOC_DMA];
#define DMA_TMP_SIZE (1048576*1) //16777216



#define debug_printk(fmt, args...)                     \
do {                                                   \
    if (dbg_err_cnt <30) {                                    \
        printk("DEBUG [%d]: " fmt, dbg_err_cnt, ##args); \
        dbg_err_cnt++;                                         \
    }                                                  \
} while (0)

typedef union REG_debug0 {
	u64 dWordVal;
	struct { 
		u32                              irq0_get                            : 1;
		u32                              irq1_get                            : 1;
		u32                              wdma0_done                          : 1;
		u32                              wdma1_done                          : 1;
		u64                              reg_FILL                            : 60;
	};
 
} REG_debug0;

static struct workqueue_struct *test_wq;
static struct work_struct test_work;
static enum work_op {
    TEST_CPUW,
    TEST_DMAW,
    TEST_DMAW2,
    TEST_DMAW_NORESET,
    TEST_DMAW2_NORESET,
    TEST_DMAR,
    TEST_DMAR2,
    CHKMEM,
    DUMP_DMA,
    FULL_DMA,
    TEST_DMA_TP
} work_op = TEST_CPUW;
static bool testing;
static uint64_t testcnt;
static spinlock_t test_lock;
unsigned long g_tmpJiff_s;
unsigned long g_tmpJiff_e;
ktime_t g_kstartt, g_kendt;
uint64_t g_fpgaintr_reg;
uint32_t g_fpgaintr_0;
uint32_t g_fpgaintr_1;
uint32_t xdmatestThold=1;
uint32_t g_intrstat;



int _xdma_chnl_init_t(struct xdma_chnl * pxdma_chnl, int dir, int hw_index)
{
    uint64_t v64;
    uint32_t v32;
    struct pci_dev *pdev=phyDev->pdev;
    int ret=0;
    int i;
    int off;
    struct xdma_desc *pxdesc;
    size_t trans_len=2048;

    xdma_chnl_init_regs(dir,hw_index);
    off=(dir==DMA_TO_DEVICE)?hw_index:2+hw_index;
    pxdma_chnl->vector = phyDev->msix_entries[off].vector;
    pxdma_chnl->dir=dir;
    pxdma_chnl->num_xdesc= phyDev->dma_size/trans_len;
    pxdma_chnl->dma_size = trans_len * pxdma_chnl->num_xdesc;
    pxdma_chnl->dma_buf = phyDev->dma_buffer;
    pxdma_chnl->dma_hdl = phyDev->dma_handle;
    // for(i=0;i<pxdma_chnl->dma_size/8;i++)
    //     _dmaWrite(i,((uint64_t)hw_index)<<48|0x0000beef00000000|i);


    pxdma_chnl->xdma_desc_buf = dma_alloc_coherent(&pdev->dev, pxdma_chnl->num_xdesc*sizeof(struct xdma_desc),
                                            &pxdma_chnl->xdma_desc_hdl, GFP_KERNEL);
    if (!pxdma_chnl->xdma_desc_buf) {
        ret = -ENOMEM;
        goto err_dma;
    }

    printk(KERN_INFO "Allocated XDMA Desc Start at 0x%llx, vmem 0x%llx\n", 
    (unsigned long long)pxdma_chnl->xdma_desc_hdl,
    (unsigned long long)pxdma_chnl->xdma_desc_buf);
    pxdma_chnl->pxdesc=(struct xdma_desc *)pxdma_chnl->xdma_desc_buf;
    memset(pxdma_chnl->pxdesc,0,pxdma_chnl->num_xdesc*sizeof(struct xdma_desc));
    for(i=0;i<pxdma_chnl->num_xdesc;i++)
    {
        pxdesc=pxdma_chnl->pxdesc+i;
        pxdesc->control=cpu_to_le32(DESC_MAGIC);
        xdma_desc_set(pxdesc
        ,pxdma_chnl->dma_hdl+i*trans_len
        ,AXI_MEM_BASE+i*trans_len
        ,trans_len
        ,pxdma_chnl->dir);
        if(i<pxdma_chnl->num_xdesc-1)
        {
            v64=(uint64_t)(pxdma_chnl->xdma_desc_hdl+(i+1)*sizeof(struct xdma_desc));
            pxdesc->next_lo=cpu_to_le32(PCI_DMA_L(v64));
            pxdesc->next_hi=cpu_to_le32(PCI_DMA_H(v64));
        }
        else
        {
		    printk("set chnl last desc\n");
                v32 = XDMA_DESC_STOPPED;
                v32 |= XDMA_DESC_EOP;
                v32 |= XDMA_DESC_COMPLETED;
                xdma_desc_control_set(pxdesc,v32);
        }
    }
    v64=(uint64_t)pxdma_chnl->xdma_desc_hdl;
    if(dir==DMA_TO_DEVICE)
        off=0x4000+0x100*hw_index;
    else
        off=0x5000+0x100*hw_index;
    _xdmaWrite(0x80+off,cpu_to_le32(PCI_DMA_L(v64)));
    _xdmaWrite(0x84+off,cpu_to_le32(PCI_DMA_H(v64)));
    _xdmaWrite(0x88+off,pxdma_chnl->num_xdesc-1);
    return ret;
    
    err_dma:

    return ret;
}
void _xdma_chnl_free_t(struct xdma_chnl * pxdma_chnl)
{
    struct pci_dev *pdev=phyDev->pdev;
    dma_free_coherent(&pdev->dev, pxdma_chnl->num_xdesc*sizeof(struct xdma_desc), pxdma_chnl->xdma_desc_buf, pxdma_chnl->xdma_desc_hdl);
}

static ssize_t _phy_control_store_t(struct kobject *kobj, struct kobj_attribute *attr,
                             const char *buf, size_t count)
{
    char input[64] = {0};
    uint64_t val;
    int result;
    int offst;
    if (strncmp(buf,"pwrite",strlen("pwrite"))==0)
    {
        
        strncpy(input,buf,count-1);
        result =  sscanf(input, "pwrite offst %x val %llx", &offst, &val);
        if (result != 2) 
        {
            printk("Invalid pwrite input\n");
            return count;
        }
        _pifWrite(offst,val);
        printk("write 0x%x 0x%016llx\n",offst, val);
    }
    else if (strncmp(buf,"pread",strlen("pread"))==0)
    {
        
        strncpy(input,buf,count-1);
        result = sscanf(input, "pread offst %x", &offst);
        if (result != 1) 
        {
            printk("Invalid pread input\n");
            return count;
        }
         printk("read 0x%x 0x%016llx\n",offst,_pifRead(offst));

    }
    else if (strncmp(buf,"dwrite",strlen("dwrite"))==0)
    {
        
        strncpy(input,buf,count-1);
        result =  sscanf(input, "dwrite offst %x val %llx", &offst, &val);
        if (result != 2) 
        {
            printk("Invalid dwrite input\n");
            return count;
        }
        _dmaWrite(offst,val);
        printk("dwrite 0x%x 0x%016llx\n",offst, val);
    }
    else if (strncmp(buf,"dread",strlen("dread"))==0)
    {
        
        strncpy(input,buf,count-1);
        result = sscanf(input, "dread offst %x", &offst);
        if (result != 1) 
        {
            printk("Invalid dread input\n");
            return count;
        }
         printk("dread 0x%x 0x%016llx\n",offst,_dmaRead(offst));

    }
    else if (strncmp(buf,"xwrite",strlen("xwrite"))==0)
    {
        
        strncpy(input,buf,count-1);
        result =  sscanf(input, "xwrite offst %x val %llx", &offst, &val);
        if (result != 2) 
        {
            printk("Invalid xwrite input\n");
            return count;
        }
        _xdmaWrite(offst,(uint32_t)val);
        printk("xwrite 0x%x 0x%08llx\n",offst, val);
    }
    else if (strncmp(buf,"xread",strlen("xread"))==0)
    {
        
        strncpy(input,buf,count-1);
        result = sscanf(input, "xread offst %x", &offst);
        if (result != 1) 
        {
            printk("Invalid xread input\n");
            return count;
        }
         printk("read 0x%x 0x%08x\n",offst,_xdmaRead(offst));
    }
    else if (strncmp(buf,"basebar",strlen("basebar"))==0)
    {
        
        strncpy(input,buf,count-1);
        result = sscanf(input, "basebar %d", &offst);
        if (result != 1) 
        {
            printk("Invalid dread32 input\n");
            return count;
        }
        if (offst==0)
            g_pPifReg=phyDev->mmio_base0;
        else if (offst==1)
            g_pPifReg=phyDev->mmio_base1;
        else if (offst==2)
            g_pPifReg=phyDev->mmio_base2;
        printk("set g_pPifReg to bar[%d]\n",offst);

    }
    else if (strncmp(buf,"starttest",strlen("starttest"))==0)
    {
        if(testing==true)
            printk("testing is running wait it\n");
        else
        {
            printk("start test write\n");
            work_op=TEST_CPUW;
            queue_work(test_wq, &test_work);
        }
    }
    else if (strncmp(buf,"startcheck",strlen("startcheck"))==0)
    {
        if(testing==true)
            printk("checking is running wait it\n");
        else
        {
            printk("start test memchk\n");
            work_op=CHKMEM;
            queue_work(test_wq, &test_work);
        }
    }
    else if (strncmp(buf,"startdwrite",strlen("startdwrite"))==0)
    {
        if(testing==true)
            printk("checking is running wait it\n");
        else
        {
            strncpy(input,buf,count-1);
            result = sscanf(input, "startdwrite %d", &offst);
            if (result != 1) 
            {
                printk("Invalid startdwrite input\n");
                return count;
            }
            if(offst==0)
            {
                printk("start dtestwrite chnl 0\n");
                work_op=TEST_DMAW;
                queue_work(test_wq, &test_work);
            }
            else
            {
                printk("start dtestwrite chnl 1\n");
                work_op=TEST_DMAW2;
                queue_work(test_wq, &test_work);
            }
        }
    }
    else if (strncmp(buf,"startdread",strlen("startdread"))==0)
    {
        if(testing==true)
            printk("checking is running wait it\n");
        else
        {
            strncpy(input,buf,count-1);
            result = sscanf(input, "startdread %d", &offst);
            if (result != 1) 
            {
                printk("Invalid startdread input\n");
                return count;
            }
            if(offst==0)
            {
                printk("start dtest read chnl 0\n");
                work_op=TEST_DMAR;
                queue_work(test_wq, &test_work);
            }
            else
            {
                printk("start dtest read chnl 1\n");
                work_op=TEST_DMAR2;
                queue_work(test_wq, &test_work);
            }
        }
    }
    else if (strncmp(buf,"startdtest",strlen("startdtest"))==0)
    {
        if(testing==true)
            printk("checking is running wait it\n");
        else
        {
            strncpy(input,buf,count-1);
            result = sscanf(input, "startdtest %d", &xdmatestThold);
            if (result != 1 
                || xdmatestThold<1) 
            {
                printk("Invalid startdtest input set  test threshold = 1\n");
                xdmatestThold=1;
            }
            printk("start dtest speed\n");
            testcnt=0;
            work_op=TEST_DMA_TP;
            queue_work(test_wq, &test_work);
        }
    }
    else if (strncmp(buf,"ffdma",strlen("ffdma"))==0)
    {
        if(testing==true)
            printk("checking is running wait it\n");
        else
        {
            printk("write dmabuffer ffff\n");
            work_op=FULL_DMA;
            queue_work(test_wq, &test_work);
        }
    }
    else if (strncmp(buf,"dumpdma",strlen("dumpdma"))==0)
    {
        if(testing==true)
            printk("checking is running wait it\n");
        else
        {
            printk("dump dmabuffer\n");
            work_op=DUMP_DMA;
            queue_work(test_wq, &test_work);
        }
    }
    else
    {
         printk("do nothing \n");
    }
    return count;
}
static ssize_t _phy_control_show_t(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{   
    ssize_t len;
    int base=0;
    if (g_pPifReg==phyDev->mmio_base0)
        base=0;
    else if (g_pPifReg==phyDev->mmio_base1)
        base=1;
    else if (g_pPifReg==phyDev->mmio_base2)
        base=2;
    len=sprintf(buf, "pif sysfs call\n");
    len+=sprintf(buf+len,"now op-basebar [%d]\n",base);
    len+=sprintf(buf+len,"pwrite offst 0x0 val 0x1\n");
    len+=sprintf(buf+len,"pread offst 0x0\n");
    len+=sprintf(buf+len,"dwrite offst 0x0 val 0x1\n");
    len+=sprintf(buf+len,"dread offst 0x0\n");
    len+=sprintf(buf+len,"xwrite offst 0x0 val 0x1\n");
    len+=sprintf(buf+len,"xread offst 0x0\n");
    len+=sprintf(buf+len,"starttest     (write to basespace)\n");
    len+=sprintf(buf+len,"startcheck    (chk basespace mem write/read)\n");
    len+=sprintf(buf+len,"startdwrite 0/1 (test write chnl)\n");
    len+=sprintf(buf+len,"startdread 0/1 (test read chnl)\n");
    len+=sprintf(buf+len,"\n");
    return len;
}
static void test_work_func(struct work_struct *work) {
    unsigned long start_time = jiffies;
    unsigned long end_time = start_time + 3*HZ;  // 3*1s
    size_t bytes_written = 0;
    size_t bytes_chk = 0;
    testing=true;
    if(work_op==TEST_CPUW)
    {
        while (time_before(jiffies, end_time)) {
            if((testcnt%2)==0)
                //writeq(0xDEADBEEF12345678, ptest + bytes_written % (1024*8));
                _pifWrite((bytes_written/8)%1024,0xDEADBEEF12345678);
            else
                _pifWrite((bytes_written/8)%1024,0x0000000000000000);
            bytes_written += 8;  // write 8bytes
        }
        pr_info("Test completed: bytes written = %zu\n", bytes_written);
        testcnt++;
    }
    else if (work_op==CHKMEM)
    {
        
        while (bytes_written < 1024*8) {
            if((testcnt%2)==0)
                _pifWrite((bytes_written/8),0xDEADBEEF12345678);
            else
                _pifWrite((bytes_written/8),0x0000000000000000);
            bytes_written += 8;  // write 8bytes
        }
        while (bytes_chk < bytes_written) {
            uint64_t rval;
            uint64_t chkval= ((testcnt%2)==0?0xDEADBEEF12345678:0x0000000000000000);
            rval=_pifRead((bytes_chk/8));
            if(rval!=chkval){
                printk("read incorrect val = %llx, offset: %ld\n", rval, bytes_chk/8);
                goto end;
            }
            bytes_chk += 8;  // write 8bytes
        }
        pr_info("chk completed: bytes written = %zu\n", bytes_chk);
        testcnt++;   
    }
    else if (work_op==TEST_DMAW)
    {
        int i;
        for(i=0;i<phyDev->dma_size/8;i++)
        {   
            if((testcnt%2)==0)
                _dmaWrite(i,0xaaaaaaaa00000000|i);
            else
                _dmaWrite(i,0x0000000000000000);
        }
        //_XDMA_RESET_ALL();
        _xdmaWrite(0x0008,0xff);
        testcnt++;
    }
    else if (work_op==TEST_DMAW2)
    {
        int i;
        for(i=0;i<phyDev->dma_size/8;i++)
        {   
            if((testcnt%2)==0)
                _dmaWrite(i,0xbbbbbbbb00000000|i);
            else
                _dmaWrite(i,0x0000000000000000);
        }
        //_XDMA_RESET_ALL();
        _xdmaWrite(0x0108,0xff);
        testcnt++;
    }
    else if (work_op==TEST_DMAW_NORESET)
    {
        _xdmaWrite(0x0008,0xff);
        testcnt++;
    }
    else if (work_op==TEST_DMAW2_NORESET)
    {
        _xdmaWrite(0x0108,0xff);
        testcnt++;
    }
    else if (work_op==TEST_DMAR)
    {
        //_XDMA_RESET_ALL();
        _xdmaWrite(0x1008,0xffffff);
        //testcnt++;   
    } 
    else if (work_op==TEST_DMAR2)
    {
        //_XDMA_RESET_ALL();
        _xdmaWrite(0x1108,0xffffff);
        //testcnt++;   
    }
    else if (work_op==DUMP_DMA)
    {
        int i;
        int dumpbyte=256;
        printk("dump dma (%d bytes) buffer:\n",dumpbyte);
        for(i=0;i<dumpbyte/8;i+=2){
            printk("0x%016llx 0x%016llx\n",_dmaRead(i),_dmaRead(i+1));
        }
        //_XDMA_RESET_ALL();
    }
    else if (work_op==FULL_DMA)
    {
        int i;
        for(i=0;i<phyDev->dma_size/8;i++)
            _dmaWrite(i,0xFFFFFFFFFFFFFFFF);
        //_XDMA_RESET_ALL();
    }
    else if (work_op==TEST_DMA_TP)
    {
        int i;
        spin_lock(&test_lock);
	    if(testcnt==0)
		    g_kstartt=ktime_get();
        if(testcnt<xdmatestThold)
        {
		    testcnt++;
#if 0 //rewrite all block for throughput test
            for(i=0;i<phyDev->dma_size/8;i++)
            {   
                if((testcnt%2)==0)
                    _dmaWrite(i,0xdeadbeef00000000|i);
                else
                    _dmaWrite(i,(start_time<<32)|i);
            }
#endif
            _XDMA_RESET_ALL();
            _xdmaWrite(0x0108,0xffffff);
            spin_unlock(&test_lock);
            return;
        }
        else
        {  
	        testing =false;
            g_kendt = ktime_get();
            printk("dma test tp done! cnt is %llu byte= %llu last stat 0x%x\n",
            testcnt,testcnt*phyDev->dma_size,
            g_intrstat);
            printk("spend timming is %llu ns\n",ktime_to_ns(ktime_sub(g_kendt,g_kstartt)));
            _XDMA_RESET_ALL();  
        }
        spin_unlock(&test_lock);
    }
    else
        printk("nothing to work\n");
    end:
    testing = false;
}
static irqreturn_t my_pcie_irq_handler(int irq, void *dev_id) {
    struct phy_pcie_dev *dev = (struct phy_pcie_dev *)dev_id;
    //uint32_t rval;
    uint32_t stat;
    uint32_t descnt;
    REG_debug0 xdadbug;
    #if 0
     printk(KERN_INFO "PCIe IRQ triggered with irq %d\n 0x2040 is %u\n 0x2044 is %u\n",
    irq,
    _xdmaRead(0x2040),
    _xdmaRead(0x2044));
    #else
    _xdmaRead(0x2040);
    _xdmaRead(0x2044);
    #endif
    if ((irq==dev->xdma_test_ch[0].vector))
    {   
        stat=_xdmaRead(0x44);
        descnt=_xdmaRead(0x48);
        if (stat!=0x6)
        debug_printk("get test tx chr0 intr status 0x%x\n",stat);
        #if HL_TEST
        xdadbug.dWordVal=_pifRead(0x90000/8);
        xdadbug.irq1_get=0x0;
        xdadbug.wdma1_done=1;
        _pifWrite(0x90000/8,xdadbug.dWordVal);
        #endif
        _XDMA_RESET_ALL();
    }
    else if ((irq==dev->xdma_test_ch[1].vector))
    {    
	    //printk("testing dma tp");

        stat=_xdmaRead(0x144);
        descnt=_xdmaRead(0x148);
	    if(testing)
        {
            if (testcnt==xdmatestThold)
                g_intrstat=stat;
            queue_work(test_wq, &test_work);
        }
        else
            _XDMA_RESET_ALL(); 
        }
    else if ((irq==dev->xdma_test_ch[2].vector))
    {    
	    printk("trigger rxdmach0 fi irq\n");
        stat=_xdmaRead(0x1044);
        descnt=_xdmaRead(0x1048);
        // work_op=DUMP_DMA;
        // queue_work(test_wq, &test_work);
        _XDMA_RESET_ALL();  
        
    }
    else if ((irq==dev->xdma_test_ch[3].vector))
    {    
	    printk("trigger rxdmach1 fi irq\n");
        stat=_xdmaRead(0x1144);
        descnt=_xdmaRead(0x1148);
        // work_op=DUMP_DMA;
        // queue_work(test_wq, &test_work);
        _XDMA_RESET_ALL();  
    }
    else if ((irq==dev->vecusr1))
    {    
#if HL_TEST
        g_fpgaintr_0++;
        tmp=(_pifRead(0x80000/8)>>0)&0xffff;
        if ((g_fpgaintr_0&0xffff)!= tmp )
            debug_printk("err cnt between fpga and drv usr1 %d %llu\n",g_fpgaintr_0,tmp);
        xdadbug.dWordVal=_pifRead(0x90000/8);
        xdadbug.irq0_get=0x1;
        xdadbug.wdma0_done=0;
        _pifWrite(0x90000/8,xdadbug.dWordVal);
#endif
        
    }
    else if ((irq == dev->vecusr2))
    { 
#if HL_TEST   
        g_fpgaintr_1++;
        tmp=(_pifRead(0x80000/8)>>16)&0xffff;
        if ((g_fpgaintr_1&0xffff)!= tmp )
            debug_printk("err cnt between fpga and drv usr2 %d %llu\n",g_fpgaintr_1,tmp);
        xdadbug.dWordVal=_pifRead(0x90000/8);
        xdadbug.irq1_get=0x1;
        xdadbug.wdma1_done=0;
        _pifWrite(0x90000/8,xdadbug.dWordVal);
        work_op=TEST_DMAW_NORESET;
        queue_work(test_wq, &test_work);
#endif
        work_op=TEST_DMAW_NORESET;
        queue_work(test_wq, &test_work);
    }
    
    return IRQ_HANDLED;
}

DEFINE_KOBJ_ATTR(phy_control_t,_phy_control_show_t,_phy_control_store_t)

int phy_pcie_probe_t(struct pci_dev *pdev, const struct pci_device_id *id) {
    int ret;
    int i;


    //create sysfs op
    phyKobj = kobject_create_and_add("pcie_phy", NULL);
    if (!phyKobj)
        return -ENOMEM;
    ret = sysfs_create_file(phyKobj, &phy_control_t_attr.attr);
    if (ret) {
        kobject_put(phyKobj);
        return ret;
    }

    // dev init
    phyDev = kzalloc(sizeof(*phyDev), GFP_KERNEL);
    if (!phyDev)
        return -ENOMEM;

    phyDev->pdev = pdev;

    // dev enable
    ret = pci_enable_device(pdev);
    if (ret)
        goto err_enable;
    pci_check_intr_pend(pdev);
    printk("check intr pend ok\n");
    /* enable relaxed ordering */
	pci_enable_capability(pdev, PCI_EXP_DEVCTL_RELAX_EN);
	/* enable extended tag */
	pci_enable_capability(pdev, PCI_EXP_DEVCTL_EXT_TAG);
    pcie_set_readrq(pdev, 512);
	/* enable bus master capability */
	pci_set_master(pdev);

    // alloc io mem
    ret = pci_request_regions(pdev, "my_pcie");
    if (ret)
        goto err_regions;

    printk("base0 srart:0x%llx end 0x%llx len 0x%llx",
        pci_resource_start(pdev, 0),
        pci_resource_end(pdev, 0),
        pci_resource_end(pdev, 0)-pci_resource_start(pdev, 0)
    );
    printk("base1 srart:0x%llx end 0x%llx len 0x%llx",
        pci_resource_start(pdev, 1),
        pci_resource_end(pdev, 1),
        pci_resource_end(pdev, 1)-pci_resource_start(pdev, 1)
    );

    phyDev->mmio_base0 =
    ioremap(pci_resource_start(pdev, 0), pci_resource_end(pdev, 0)-pci_resource_start(pdev, 0));
    if (!phyDev->mmio_base0) {
        ret = -EIO;
        goto err_iomap;
    }
    phyDev->mmio_base1 =
    ioremap(pci_resource_start(pdev, 1), pci_resource_end(pdev, 1)-pci_resource_start(pdev, 1));
    if (!phyDev->mmio_base1) {
        ret = -EIO;
        goto err_iomap;
    }
    phyDev->mmio_base2 =
    ioremap(pci_resource_start(pdev, 2), pci_resource_end(pdev, 2)-pci_resource_start(pdev, 2));
    if (!phyDev->mmio_base2) {
        ret = -EIO;
        goto err_iomap;
    }
    #if PCIE_BASE_PIF_RFDC
    g_pPifReg=phyDev->mmio_base2;
    #else
    g_pPifReg=phyDev->mmio_base1;
    #endif

    // req irq
    _xdmaWrite(0x200c,0xffffffff);
    _xdmaWrite(0x2018,0xffffffff);

    if (!dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64))) 
	{
		printk("Using a 64-bit DMA mask.\n");
	} 
    else if (!dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32))) 
	{
		printk("Could not set 64-bit DMA mask.\n");
		printk("Using a 32-bit DMA mask.\n");
	}
    else 
    {
		printk("No suitable DMA possible.\n");
		ret= -EINVAL;
        	goto err_irq;
	}
    printk("try enable msi irq\n");
    pci_keep_intx_enabled(pdev);
    printk("keep intx irq\n");


    for (i = 0; i < MSIX_VEC_COUNT; i++) {
        phyDev->msix_entries[i].entry = i; 
    }

    ret = pci_enable_msix_range(pdev, phyDev->msix_entries, 0, MSIX_VEC_COUNT);
    if (ret < 0) {
        pr_err("Failed to enable MSI-X\n");
        return ret;
    }   
    g_msix_vec_cnt=ret;
    for (i = 0; i < g_msix_vec_cnt; i++) {
        pr_info("MSI-X Entry %d assigned to vector %u\n",
                phyDev->msix_entries[i].entry, phyDev->msix_entries[i].vector);
    }
    for (i = 0; i < g_msix_vec_cnt; i++) 
    {

		ret = request_irq(phyDev->msix_entries[i].vector, my_pcie_irq_handler, 0, "my_pcie_irq", phyDev);
		if (ret) {
			pr_info("requesti irq#%d failed %d.\n",
				phyDev->msix_entries[i].vector, ret);
			goto err_irq;
		}
		pr_info("my_pcie_irq add , irq#%d.\n", phyDev->msix_entries[i].vector);
	}
    prog_irq_msix_channel(2,2,16,0);
    prog_irq_msix_user(2,2,16,0);
    phyDev->vecusr1=phyDev->msix_entries[2+2].vector;
    phyDev->vecusr2=phyDev->msix_entries[2+2+1].vector;

    printk ("setup user/chnl irq reg");
   //_xdmaWrite(0x2004,1);
   //_xdmaWrite(0x2008,1);
   //_xdmaWrite(0x0090,0xff);
    _xdmaWrite(0x0094,0xff);
    //_xdmaWrite(0x0190,0xff);
    _xdmaWrite(0x0194,0xff);
    //_xdmaWrite(0x2010,0xff);
    _xdmaWrite(0x2014,0xffffffff);
    printk ("started user/chnl irq");

/*************************alloc dma************************* */
    phyDev->dma_size = DMA_SIZE_TEST;
    phyDev->dma_buffer = dma_alloc_coherent(&pdev->dev, phyDev->dma_size,
                                            &phyDev->dma_handle, GFP_KERNEL);
    printk(KERN_INFO "Allocated DMA buffer at 0x%llx, vmem 0x%llx\n", 
    (unsigned long long)phyDev->dma_handle,
    (unsigned long long)phyDev->dma_buffer);
    if(
        _xdma_chnl_init_t(&phyDev->xdma_test_ch[0],DMA_TO_DEVICE,0)||
        _xdma_chnl_init_t(&phyDev->xdma_test_ch[1],DMA_TO_DEVICE,1)||
        _xdma_chnl_init_t(&phyDev->xdma_test_ch[2],DMA_FROM_DEVICE,0)||
        _xdma_chnl_init_t(&phyDev->xdma_test_ch[3],DMA_FROM_DEVICE,1)
    )
    {
        goto err_xdma_init;
    }

    for(i=0;i<phyDev->dma_size/8;i++)
        _dmaWrite(i,(0xaaaabbbb00000000)|i);
    _XDMA_RESET_ALL();

/*
    pDmaBuft1 = dma_alloc_coherent(&pdev->dev, DMA_TMP_S3,
                                           &pDmaHdlt1, GFP_KERNEL);
    printk(KERN_INFO "Allocated test DMA buffer at 0x%llx, vmem 0x%llx\n", 
    (unsigned long long)pDmaHdlt1,
    (unsigned long long)pDmaBuft1);
*/
    for(i=0;i<MAX_ALLOC_DMA;i++){
        pDmaBuft1[i]= dma_alloc_coherent(&pdev->dev, DMA_TMP_SIZE ,
                                           &pDmaHdlt1[i], GFP_KERNEL);
        if(!pDmaBuft1[i])
        {
            printk(KERN_INFO "Allocated test DMA buffer faild [%d] at 0x%llx, vmem 0x%llx\n",
            i,
            (unsigned long long)pDmaHdlt1[i],
            (unsigned long long)pDmaBuft1[i]);
        }
    }
/*-------------------------------------------------------*/
    
    pci_set_drvdata(pdev, phyDev);

/**************************for test  ***********************/
    spin_lock_init(&test_lock);
    test_wq = alloc_workqueue("pcie_test_wq", WQ_UNBOUND, 0);
    if (!test_wq) {
        ret = -ENOMEM;
        goto err_twq;
    }
    INIT_WORK(&test_work, test_work_func);
    #if HL_TEST
    g_fpgaintr_reg=_pifRead(0x80000/8);
    g_fpgaintr_0=g_fpgaintr_reg&0xffff;
    g_fpgaintr_1=(g_fpgaintr_reg>>16)&0xffff;
    //_xdmaWrite(0x2008,3);
    //debug_printk("hoho test dbg printk  %d %d\n",123,456);
    #endif
/**************************for test  end*********************/
    return 0;

err_twq:
err_xdma_init:
    i=g_msix_vec_cnt;
    while (--i >= 0) {
        free_irq(phyDev->msix_entries[i].vector, phyDev);
    }
err_irq:
    pci_iounmap(pdev, phyDev->mmio_base0);
    pci_iounmap(pdev, phyDev->mmio_base1);
    pci_iounmap(pdev, phyDev->mmio_base2);
err_iomap:
    pci_release_regions(pdev);
err_regions:
    pci_disable_device(pdev);
err_enable:
    kfree(phyDev);
    sysfs_remove_file(phyKobj, &phy_control_t_attr.attr);
    kobject_put(phyKobj);
    return ret;
}

void phy_pcie_remove_t(struct pci_dev *pdev) {
    struct phy_pcie_dev *dev = pci_get_drvdata(pdev);
    int i;

    _xdmaWrite(0x200c, 0x3);
    _xdmaWrite(0x000c, 0x1);
    _xdmaWrite(0x010c, 0x1);
    _xdmaWrite(0x100c, 0x1);
    _xdmaWrite(0x110c, 0x1);

    flush_workqueue(test_wq);
    destroy_workqueue(test_wq);
    _xdma_chnl_free_t(&dev->xdma_test_ch[0]);
    _xdma_chnl_free_t(&dev->xdma_test_ch[1]);
    _xdma_chnl_free_t(&dev->xdma_test_ch[2]);
    _xdma_chnl_free_t(&dev->xdma_test_ch[3]);
    dma_free_coherent(&pdev->dev, phyDev->dma_size, phyDev->dma_buffer, phyDev->dma_handle);
    //dma_free_coherent(&pdev->dev, DMA_TMP_SIZE, pDmaBuft1, pDmaHdlt1);
    for(i=0;i<MAX_ALLOC_DMA;i++){
        dma_free_coherent(&pdev->dev, DMA_TMP_SIZE , pDmaBuft1[i], pDmaHdlt1[i]);
    }

    i=g_msix_vec_cnt;
    while (--i >= 0) {
        free_irq(dev->msix_entries[i].vector, dev);
    }

    pci_iounmap(pdev, dev->mmio_base0);
    pci_iounmap(pdev, dev->mmio_base1);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    kfree(dev);
    printk("rem test sysfs\n");
    sysfs_remove_file(phyKobj, &phy_control_t_attr.attr);
    kobject_put(phyKobj);
}

