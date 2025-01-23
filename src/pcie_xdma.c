#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include "pcie_xdma.h"
#include "phy_driver.h"
#include "phy_driver_test.h"
void xdma_desc_set(struct xdma_desc *desc, dma_addr_t rc_bus_addr,
			  u64 ep_addr, int len, int dir)
{
	/* transfer length */
	desc->bytes = cpu_to_le32(len);
	if (dir == DMA_TO_DEVICE) {
		/* read from root complex memory (source address) */
		desc->src_addr_lo = cpu_to_le32(PCI_DMA_L(rc_bus_addr));
		desc->src_addr_hi = cpu_to_le32(PCI_DMA_H(rc_bus_addr));
		/* write to end point address (destination address) */
		desc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(ep_addr));
		desc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(ep_addr));
	} else {
		/* read from end point address (source address) */
		desc->src_addr_lo = cpu_to_le32(PCI_DMA_L(ep_addr));
		desc->src_addr_hi = cpu_to_le32(PCI_DMA_H(ep_addr));
		/* write to root complex memory (destination address) */
		desc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(rc_bus_addr));
		desc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(rc_bus_addr));
	}
}
int xdma_desc_control_set(struct xdma_desc *first, u32 control_field)
{
	/* remember magic and adjacent number */
	u32 control = le32_to_cpu(first->control) & ~(0x000000FFUL);

	if (control_field & ~(0x000000FFUL)) {
		pr_err("Invalid control field\n");
		return -EINVAL;
	}
	/* merge adjacent and control field */
	control |= control_field;
	/* write control and next_adjacent */
	first->control = cpu_to_le32(control);
	return 0;
}
void prog_irq_msix_user(uint32_t h2c_chnl_max,uint32_t c2h_chnl_max, uint32_t user_max,bool clear)
{
	struct interrupt_regs int_regs ={0};
    uint32_t i = c2h_chnl_max + h2c_chnl_max;
	uint32_t max = i + user_max;
	int j;

	for (j = 0; i < max; j++) {
		uint32_t val = 0;
		int k;
		int shift = 0;
		size_t ofst;

		if (clear)
			i += 4;
		else
			for (k = 0; k < 4 && i < max; i++, k++, shift += 8)
				val |= (i & 0x1f) << shift;

		//write_register(val, &int_regs->user_msi_vector[j]);
		ofst=(size_t)&(int_regs.user_msi_vector[j])-(size_t)&int_regs + 0x2000;
        _xdmaWrite((int)ofst,val);
		printk("vector %d 0x%lx, 0x%x.\n", j,ofst,val);
	}
}

void prog_irq_msix_channel(uint32_t h2c_chnl_max, uint32_t c2h_chnl_max, uint32_t user_max, bool clear)
{
	struct interrupt_regs int_regs ={0};
	uint32_t max = c2h_chnl_max + h2c_chnl_max;
	uint32_t i;
	int j;

	/* engine */
	for (i = 0, j = 0; i < max; j++) {
		uint32_t val = 0;
		int k;
		int shift = 0;
		size_t ofst;

		if (clear)
			i += 4;
		else
			for (k = 0; k < 4 && i < max; i++, k++, shift += 8)
				val |= (i & 0x1f) << shift;

		//write_register(val, &int_regs->channel_msi_vector[j]);
		ofst=(size_t)&(int_regs.channel_msi_vector[j])-(size_t)&int_regs + 0x2000;
        _xdmaWrite((int)ofst,val);
		printk("vector %d 0x%lx, 0x%x.\n", j,ofst,val);
	}
}

int xdma_chnl_init_regs(int dir,int hw_indx)
{
	struct engine_regs int_regs ={0};
	u32 reg_value;
	int ofstchnl;
	int ofst;
	if (dir==DMA_TO_DEVICE)
		ofstchnl=0x100*hw_indx;
	else
		ofstchnl=0x100*hw_indx+0x1000;
	// write_register(XDMA_CTRL_NON_INCR_ADDR, &engine->regs->control_w1c,
	// 	       (unsigned long)(&engine->regs->control_w1c) -
	// 		       (unsigned long)(&engine->regs));

	//printk("engine_init_regs\n");
	ofst=ofstchnl+(size_t)&(int_regs.control_w1c)-(size_t)&int_regs;
	_xdmaWrite((int)ofst,XDMA_CTRL_NON_INCR_ADDR);
	/* Configure error interrupts by default */
	reg_value = XDMA_CTRL_IE_DESC_ALIGN_MISMATCH;
	reg_value |= XDMA_CTRL_IE_MAGIC_STOPPED;
	reg_value |= XDMA_CTRL_IE_MAGIC_STOPPED;
	reg_value |= XDMA_CTRL_IE_READ_ERROR;
	reg_value |= XDMA_CTRL_IE_DESC_ERROR;

	{
		/* enable the relevant completion interrupts */
		reg_value |= XDMA_CTRL_IE_DESC_STOPPED;
		reg_value |= XDMA_CTRL_IE_DESC_COMPLETED;
	}

	/* Apply engine configurations */
	// write_register(reg_value, &engine->regs->interrupt_enable_mask,
	// 	       (unsigned long)(&engine->regs->interrupt_enable_mask) -
	// 		       (unsigned long)(&engine->regs));
	ofst=ofstchnl+(size_t)&(int_regs.interrupt_enable_mask)-(size_t)&int_regs;
	_xdmaWrite((int)ofst,reg_value);

	return 0;
}
void pci_check_intr_pend(struct pci_dev *pdev)
{
	u16 v;

	pci_read_config_word(pdev, PCI_STATUS, &v);
	if (v & PCI_STATUS_INTERRUPT) {
		pr_info("%s PCI STATUS Interrupt pending 0x%x.\n",
			dev_name(&pdev->dev), v);
		pci_write_config_word(pdev, PCI_STATUS, PCI_STATUS_INTERRUPT);
	}
}

void pci_keep_intx_enabled(struct pci_dev *pdev)
{
	/* workaround to a h/w bug:
	 * when msix/msi become unavaile, default to legacy.
	 * However the legacy enable was not checked.
	 * If the legacy was disabled, no ack then everything stuck
	 */
	u16 pcmd, pcmd_new;

	pci_read_config_word(pdev, PCI_COMMAND, &pcmd);
	pcmd_new = pcmd & ~PCI_COMMAND_INTX_DISABLE;
	if (pcmd_new != pcmd) {
		pr_info("%s: clear INTX_DISABLE, 0x%x -> 0x%x.\n",
			dev_name(&pdev->dev), pcmd, pcmd_new);
		pci_write_config_word(pdev, PCI_COMMAND, pcmd_new);
	}
}
void pci_enable_capability(struct pci_dev *pdev, int cap)
{
	u16 v;
	int pos;

	pos = pci_pcie_cap(pdev);
	if (pos > 0) {
		pci_read_config_word(pdev, pos + PCI_EXP_DEVCTL, &v);
		v |= cap;
		pci_write_config_word(pdev, pos + PCI_EXP_DEVCTL, v);
	}
}