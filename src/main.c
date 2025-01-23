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
#include "phy_driver.h"


static int __init mytest_init(void)
{
    int err;
    err = pci_phy_init();
    if (err != 0)
    {
        pr_err("%s: phy_init ... failed\n", __func__);
        return -EFAULT;
    }
    return 0;
}

/**
*  Kernel module un-initialize function.
*/
static void __exit mytest_uninit(void)
{
    pci_phy_uninit();
}

module_init(mytest_init);
module_exit(mytest_uninit);
MODULE_LICENSE( "GPL" );
