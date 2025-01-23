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
#include "phy_driver_test.h"


#define ENABLE_PHY_TEST 1
#define PCI_VENDOR_ID_TEST 0x10EE
#define PCI_DEVICE_ID_TEST1 0x9048
#define PCI_DEVICE_ID_TEST2 0x9038
#define PHY_RECV_QUEUE_NUM 64
#define PHY_RECV_MESSAGE_NUM 10

struct phy_pcie_dev *phyDev;

static struct pci_device_id pci_ids[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_TEST, PCI_DEVICE_ID_TEST1)  },
    { PCI_DEVICE(PCI_VENDOR_ID_TEST, PCI_DEVICE_ID_TEST2) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

struct kobject *phyKobj;
uint64_t __iomem *g_pPifReg;
int g_msix_vec_cnt;


uint64_t _pifRead(int offst)
{
    uint64_t value;
    value=readq(g_pPifReg + offst);
    return value;
}
void _pifWrite(int offst, uint64_t value)
{
    writeq(value, g_pPifReg + offst);
}

uint64_t _dmaRead(int offst)
{
    uint64_t value;
    uint64_t __iomem *pdma;
    pdma=(uint64_t __iomem *)phyDev->dma_buffer;
    value=readq(pdma + offst);
    return value;
}

void _dmaWrite(int offst, uint64_t value)
{
    uint64_t __iomem *pdma;
    pdma=(uint64_t __iomem *)phyDev->dma_buffer;
    writeq(value, pdma + offst);
}
uint32_t _xdmaRead(int offst)
{
    uint32_t value;
    uint8_t __iomem *pxdma;
    #if PCIE_BASE_PIF_RFDC
    pxdma=(uint8_t __iomem *)phyDev->mmio_base1;
    #else
    pxdma=(uint8_t __iomem *)phyDev->mmio_base0;
    #endif
    value=readl(pxdma + offst);
    return value;
}
void _xdmaWrite(int offst, uint32_t value)
{
    uint8_t __iomem *pxdma;
    #if PCIE_BASE_PIF_RFDC
    pxdma=(uint8_t __iomem *)phyDev->mmio_base1;
    #else
    pxdma=(uint8_t __iomem *)phyDev->mmio_base0;
    #endif
    writel(value, pxdma + offst);
}

static struct pci_driver phy_pcie_driver = {
    .name = "phy_pcie",
    .id_table = pci_ids,
#if ENABLE_PHY_TEST
    .probe = phy_pcie_probe_t,
    .remove = phy_pcie_remove_t,
#else
    .probe = phy_pcie_probe_t,
    .remove = phy_pcie_remove_t,
#endif
};

int pci_phy_init(void)
{
    return pci_register_driver(&phy_pcie_driver);
}

void pci_phy_uninit(void)
{
	printk("pci_phy_uninit\n");
    pci_unregister_driver(&phy_pcie_driver);
}

