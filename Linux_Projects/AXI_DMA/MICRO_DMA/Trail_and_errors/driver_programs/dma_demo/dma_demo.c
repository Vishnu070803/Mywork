/*#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define DRIVER_NAME "axi_dma"
#define DEVICE_NAME "axi_dma_dev"

#define DMA_BASE_ADDR  0x40400000  // Update this based on your FPGA address
#define DMA_SIZE       0x10000      // Size of the DMA memory-mapped region

// AXI DMA Register Offsets
#define DMA_MM2S_DMACR     0x00  // MM2S DMA Control Register
#define DMA_MM2S_DMASR     0x04  // MM2S DMA Status Register
#define DMA_MM2S_SA        0x18  // MM2S Source Address
#define DMA_MM2S_LENGTH    0x28  // MM2S Transfer Length

#define DMA_S2MM_DMACR     0x30  // S2MM DMA Control Register
#define DMA_S2MM_DMASR     0x34  // S2MM DMA Status Register
#define DMA_S2MM_DA        0x48  // S2MM Destination Address
#define DMA_S2MM_LENGTH    0x58  // S2MM Transfer Length

// Control Register Flags
#define DMA_CR_RUNSTOP     (1 << 0)  // Start DMA
#define DMA_CR_RESET       (1 << 2)  // Reset DMA

// Status Register Flags
#define DMA_SR_HALTED      (1 << 0)  // DMA is halted
#define DMA_SR_IDLE        (1 << 1)  // DMA is idle
#define DMA_SR_DONE        (1 << 12) // DMA transfer done

// Global Variables
static void __iomem *dma_base;
//static uint32_t *src_buffer;
//static uint32_t *dest_buffer;
static dma_addr_t src_dma_addr;
static dma_addr_t dest_dma_addr;
static dev_t dev_num;
static struct cdev c_dev;
static struct class *cl;
void __iomem *virt_addr;
 uint32_t value;
// Function to write to DMA register
static inline void dma_writec(uint32_t offset, uint32_t value) {
    iowrite32(value, dma_base + offset);
}

// Function to read from DMA register
static inline uint32_t dma_readc(uint32_t offset) {
    return ioread32(dma_base + offset);
}

// DMA Transfer Function
static int axi_dma_transfer(size_t length) {
   int i;

    // Reset DMA
    dma_writec(DMA_MM2S_DMACR, DMA_CR_RESET);
    dma_writec(DMA_S2MM_DMACR, DMA_CR_RESET);

    // Wait for reset to complete
    while (dma_readc(DMA_MM2S_DMACR) & DMA_CR_RESET);
    while (dma_readc(DMA_S2MM_DMACR) & DMA_CR_RESET);

    pr_info(DRIVER_NAME ": Starting MM2S transfer...\n");

    // Configure MM2S (Memory to Stream)
    dma_writec(DMA_MM2S_SA, src_dma_addr);
    dma_writec(DMA_MM2S_DMACR, DMA_CR_RUNSTOP);
    dma_writec(DMA_MM2S_LENGTH, length);

    // Poll for MM2S completion
    while (!(dma_readc(DMA_MM2S_DMASR) & DMA_SR_DONE));

    pr_info(DRIVER_NAME ": MM2S transfer completed.\n");
    pr_info(DRIVER_NAME ": Starting S2MM transfer...\n");

    // Configure S2MM (Stream to Memory)
    dma_writec(DMA_S2MM_DA, dest_dma_addr);
    dma_writec(DMA_S2MM_DMACR, DMA_CR_RUNSTOP);
    dma_writec(DMA_S2MM_LENGTH, length);

    // Poll for S2MM completion
    while (!(dma_readc(DMA_S2MM_DMASR) & DMA_SR_DONE));

    pr_info(DRIVER_NAME ": S2MM transfer completed.\n");

    // **Readback Verification**
    pr_info(DRIVER_NAME ": Verifying transferred data...\n");
   
    virt_addr = ioremap(0xa0001000, 0x1000);
    if (!virt_addr) {
        pr_err(DRIVER_NAME ": Failed to map physical address\n");
        return -ENOMEM;
    }

    // Read first 10 values
    for (i = 0; i < 1000; i++) {
        value = readl(virt_addr + (i * sizeof(uint32_t)));
        pr_info(DRIVER_NAME ": dest_add[%d] = 0x%08x\n", i, value);
    }

    return 0;
}

// Character Device Operations
static int axi_dma_open(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t axi_dma_writec(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    axi_dma_transfer(count);
    return count;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = axi_dma_open,
    .write = axi_dma_writec,
};

static int __init axi_dma_init(void) {
    int ret;

    // Map DMA registers
    dma_base = ioremap(DMA_BASE_ADDR, DMA_SIZE);
    if (!dma_base) {
        pr_err(DRIVER_NAME ": Failed to map DMA registers\n");
        return -ENOMEM;
    }

    // Allocate memory for buffers
  //src_buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
  
  //dest_buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);

  src_dma_addr = 0xa0000000; //dma_map_single(dev, src_buffer, PAGE_SIZE, DMA_TO_DEVICE);
  dest_dma_addr = 0xa0001000;//dma_map_single(dev, dest_buffer, PAGE_SIZE, DMA_FROM_DEVICE);

    if (!src_dma_addr || !dest_dma_addr) {
        pr_err(DRIVER_NAME ": Failed to allocate DMA buffers\n");
        return -ENOMEM;
    }
	
    // Fill source buffer with data
    virt_addr = ioremap(0xa0000000, 0x1000);
    if (!virt_addr) {
        pr_err("Failed to map physical address\n");
        return -ENOMEM;
    }

    memset_io(virt_addr, 0xAA, 0x1000); // Set pattern (0xAA)

    pr_info("Pattern written to physical address 0x%lx\n", 0xa0000000);
    // Register character device
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err(DRIVER_NAME ": Failed to allocate char device\n");
        return ret;
    }

    // Create device class
    cl = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(cl)) {
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(cl);
    }

    // Initialize character device
    cdev_init(&c_dev, &fops);
    ret = cdev_add(&c_dev, dev_num, 1);
    if (ret < 0) {
        class_destroy(cl);
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }


    pr_info(DRIVER_NAME ": AXI DMA driver loaded\n");
    return 0;
}

// Driver Cleanup
static void __exit axi_dma_exit(void) {
    unregister_chrdev(0, DEVICE_NAME);

//dma_unmap_single(dev, src_dma_addr, PAGE_SIZE, DMA_TO_DEVICE);
//dma_unmap_single(dev, dest_dma_addr, PAGE_SIZE, DMA_FROM_DEVICE);


    iounmap(dma_base);
	cdev_del(&c_dev);
    class_destroy(cl);
    unregister_chrdev_region(dev_num, 1);

    pr_info(DRIVER_NAME ": AXI DMA driver unloaded\n");
}

module_init(axi_dma_init);
module_exit(axi_dma_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChatGPT");
MODULE_DESCRIPTION("AXI DMA Memory-to-Memory Transfer Driver with Readback");*/





#include <linux/bitops.h>
#include <linux/dmapool.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_dma.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/dmaengine.h>


/* Macros */
#define dma_poll_timeout(chan, reg, val, cond, delay_us, timeout_us) \
	readl_poll_timeout_atomic(chan->ddev->regs + chan->ctrl_offset + reg, \
				  val, cond, delay_us, timeout_us)







/* Register/Descriptor Offsets */
#define DMA_MM2S_CTRL_OFFSET		0x0000
#define DMA_S2MM_CTRL_OFFSET		0x0030


/* Control Registers */
#define DMA_REG_DMACR			0x0000
#define DMA_DMACR_RUNSTOP		BIT(0)
#define DMA_DMACR_RESET			BIT(2)
/* Status Registers */
#define DMA_REG_DMASR			0x0004
#define DMA_DMASR_IDLE			BIT(1)
#define DMA_DMASR_HALTED		BIT(0)

/* Delay loop counter to prevent hardware failure */
#define DMA_LOOP_COUNT		1000000

/* AXI DMA Specific Registers/Offsets */
#define DMA_REG_SRCDSTADDR	0x18
#define DMA_REG_BTT		0x28

/* HW specific definitions */

#define DMA_MAX_CHANS_PER_DEVICE	0x2
#define DMA_DMASR_ERR_IRQ		BIT(14)
#define DMA_DMASR_DLY_CNT_IRQ		BIT(13)
#define DMA_DMASR_FRM_CNT_IRQ		BIT(12)


#define DMA_DMAXR_ALL_IRQ_MASK	\
		(DMA_DMASR_FRM_CNT_IRQ | \
		 DMA_DMASR_DLY_CNT_IRQ | \
		 DMA_DMASR_ERR_IRQ)

const int max_channels = 2;

struct c_dma_device{
	struct device *dev;
	struct dma_device common;
	struct c_dma_chan *chan[DMA_MAX_CHANS_PER_DEVICE];
	struct platform_device  *pdev;
	const struct c_dma_config *dma_config;
	bool ext_addr;
	struct clk *axi_clk;
	struct clk *tx_clk;
	struct clk *txs_clk;
	struct clk *rx_clk;
	struct clk *rxs_clk;
	void __iomem *regs;
	u32 s2mm_chan_id;
	u32 mm2s_chan_id;
};


struct c_dma_chan {
	int id;
	u32 ctrl_offset;
	struct c_dma_device *ddev;
	struct device *dev;
	int irq;
	struct dma_chan common;
	bool err;
	bool idle;
	bool ext_addr;
	enum dma_transfer_direction direction;
	spinlock_t lock;


};

enum dma_ip_type {
	DMA_TYPE_AXIDMA = 0,
	DMA_TYPE_CDMA,
	DMA_TYPE_VDMA,
	DMA_TYPE_AXIMCDMA
};

struct c_dma_config {
	enum dma_ip_type dmatype;
	int (*clk_init)(struct platform_device *pdev, struct clk **axi_clk,
			struct clk **tx_clk, struct clk **txs_clk,
			struct clk **rx_clk, struct clk **rxs_clk);
	//irqreturn_t (*irq_handler)(int irq, void *data);
	//const int max_channels;
};


/* IO accessors */
static inline u32 dma_read(struct c_dma_chan *chan, u32 reg)
{
	return ioread32(chan->ddev->regs + reg);
}

static inline void dma_write(struct c_dma_chan *chan, u32 reg, u32 value)
{
	iowrite32(value, chan->ddev->regs + reg);
}

static inline u32 dma_ctrl_read(struct c_dma_chan *chan, u32 reg)
{
	return dma_read(chan, chan->ctrl_offset + reg);
}

static inline void dma_ctrl_write(struct c_dma_chan *chan, u32 reg,
				   u32 value)
{
	dma_write(chan, chan->ctrl_offset + reg, value);
}

static inline void dma_ctrl_clr(struct c_dma_chan *chan, u32 reg,
				 u32 clr)
{
	dma_ctrl_write(chan, reg, dma_ctrl_read(chan, reg) & ~clr);
}

static inline void dma_ctrl_set(struct c_dma_chan *chan, u32 reg,
				 u32 set)
{
	dma_ctrl_write(chan, reg, dma_ctrl_read(chan, reg) | set);
}












/**
 * dma_chan_remove - Per Channel remove function
 * @chan: Driver specific DMA channel
 */
static void dma_chan_remove(struct c_dma_chan *chan)
{
	/* Disable all interrupts */
	dma_ctrl_clr(chan, DMA_REG_DMACR,
		      DMA_DMAXR_ALL_IRQ_MASK);

	if (chan->irq > 0)
		free_irq(chan->irq, chan);

	//tasklet_kill(&chan->tasklet);

	list_del(&chan->common.device_node);
}





static int axidma_clk_init(struct platform_device *pdev, struct clk **axi_clk,
			    struct clk **tx_clk, struct clk **rx_clk,
			    struct clk **sg_clk, struct clk **tmp_clk)
{
	int err;

	*tmp_clk = NULL;

	*axi_clk = devm_clk_get(&pdev->dev, "s_axi_lite_aclk");
	if (IS_ERR(*axi_clk)) {
		err = PTR_ERR(*axi_clk);
		if (err != -EPROBE_DEFER)
			dev_err(&pdev->dev, "failed to get axi_aclk (%d)\n",
				err);
		return err;
	}

	*tx_clk = devm_clk_get(&pdev->dev, "m_axi_mm2s_aclk");
	if (IS_ERR(*tx_clk))
		*tx_clk = NULL;

	*rx_clk = devm_clk_get(&pdev->dev, "m_axi_s2mm_aclk");
	if (IS_ERR(*rx_clk))
		*rx_clk = NULL;

	*sg_clk = devm_clk_get(&pdev->dev, "m_axi_sg_aclk");
	if (IS_ERR(*sg_clk))
		*sg_clk = NULL;

	err = clk_prepare_enable(*axi_clk);
	if (err) {
		dev_err(&pdev->dev, "failed to enable axi_clk (%d)\n", err);
		return err;
	}

	err = clk_prepare_enable(*tx_clk);
	if (err) {
		dev_err(&pdev->dev, "failed to enable tx_clk (%d)\n", err);
		goto err_disable_axiclk;
	}

	err = clk_prepare_enable(*rx_clk);
	if (err) {
		dev_err(&pdev->dev, "failed to enable rx_clk (%d)\n", err);
		goto err_disable_txclk;
	}

	err = clk_prepare_enable(*sg_clk);
	if (err) {
		dev_err(&pdev->dev, "failed to enable sg_clk (%d)\n", err);
		goto err_disable_rxclk;
	}

	return 0;

err_disable_rxclk:
	clk_disable_unprepare(*rx_clk);
err_disable_txclk:
	clk_disable_unprepare(*tx_clk);
err_disable_axiclk:
	clk_disable_unprepare(*axi_clk);

	return err;
}

/**
 * of_dma_xilinx_xlate - Translation function
 * @dma_spec: Pointer to DMA specifier as found in the device tree
 * @ofdma: Pointer to DMA controller data
 *
 * Return: DMA channel pointer on success and NULL on error
 */
static struct dma_chan *of_dma_translate(struct of_phandle_args *dma_spec,
						struct of_dma *ofdma)
{
	struct c_dma_device *ddev = ofdma->of_dma_data;
	int chan_id = dma_spec->args[0];

	if (chan_id >= max_channels)
		return NULL;

	return dma_get_slave_channel(&ddev->chan[chan_id]->common);
}



static const struct c_dma_config axidma_config = {
	.dmatype = DMA_TYPE_AXIDMA,
	.clk_init = axidma_clk_init,
//	.irq_handler = xilinx_dma_irq_handler,
	//.max_channels = DMA_MAX_CHANS_PER_DEVICE,
};



static const struct of_device_id dma_of_ids[] = {
	{ .compatible = "vishnu, prototype-1", .data = &axidma_config },
	{}
};
MODULE_DEVICE_TABLE(of, dma_of_ids);



static void disable_allclks(struct c_dma_device *ddev)
{
	clk_disable_unprepare(ddev->rxs_clk);
	clk_disable_unprepare(ddev->rx_clk);
	clk_disable_unprepare(ddev->txs_clk);
	clk_disable_unprepare(ddev->tx_clk);
	clk_disable_unprepare(ddev->axi_clk);
}

static int dma_reset(struct c_dma_chan *chan)
{
	int err;
	u32 tmp;

	dma_ctrl_set(chan, DMA_REG_DMACR, DMA_DMACR_RESET);

	/* Wait for the hardware to finish reset */
	err = dma_poll_timeout(chan, DMA_REG_DMACR, tmp,
				      !(tmp & DMA_DMACR_RESET), 0,
				      DMA_LOOP_COUNT);

	if (err) {
		dev_err(chan->dev, "reset timeout, cr %x, sr %x\n",
			dma_ctrl_read(chan, DMA_REG_DMACR),
			dma_ctrl_read(chan, DMA_REG_DMASR));
		return -ETIMEDOUT;
	}

	chan->err = false;
	chan->idle = true;
	//chan->desc_pendingcount = 0;
	//chan->desc_submitcount = 0;  related to descriptors

	return err;
}



static int dma_chan_reset(struct c_dma_chan *chan)
{
	int err;

	/* Reset VDMA */
	err = dma_reset(chan);
	if (err)
		return err;

	/* Enable interrupts */
	dma_ctrl_set(chan, DMA_REG_DMACR,
		      DMA_DMAXR_ALL_IRQ_MASK);

	return 0;
}


static int c_dma_chan_probe(struct c_dma_device *ddev,
				  struct device_node *node)
{
	struct c_dma_chan *chan;
	bool has_dre = false;
	u32 value, width;
	int err;

	/* Allocate and initialize the channel structure */
	chan = devm_kzalloc(ddev->dev, sizeof(*chan), GFP_KERNEL);
	if (!chan)
		return -ENOMEM;

	chan->dev = ddev->dev;
	chan->ddev = ddev;
	chan->ext_addr = ddev->ext_addr;
	chan->idle = true;
	//spin_lock_init(&chan->lock);
	/* Retrieve the channel properties from the device tree */
	has_dre = of_property_read_bool(node, "xlnx,include-dre");

	err = of_property_read_u32(node, "xlnx,datawidth", &value);
	if (err) {
		dev_err(ddev->dev, "missing xlnx,datawidth property\n");
		return err;
	}
	width = value >> 3; /* Convert bits to bytes */

	/* If data width is greater than 8 bytes, DRE is not in hw */
	if (width > 8)
		has_dre = false;

	if (!has_dre)
		ddev->common.copy_align = fls(width - 1);

	if (of_device_is_compatible(node, "xlnx,axi-dma-mm2s-channel")) {
		chan->direction = DMA_MEM_TO_DEV;
		chan->id = ddev->mm2s_chan_id++;
	//	chan->tdest = chan->id;

		chan->ctrl_offset = DMA_MM2S_CTRL_OFFSET;
	} else if (of_device_is_compatible(node, "xlnx,axi-dma-s2mm-channel")) {
		chan->direction = DMA_DEV_TO_MEM;
		chan->id = ddev->s2mm_chan_id++;
	//	chan->tdest = chan->id - xdev->dma_config->max_channels / 2;
		
		chan->ctrl_offset = DMA_S2MM_CTRL_OFFSET;

	} else {
		dev_err(ddev->dev, "Invalid channel compatible node\n");
		return -EINVAL;
	}
	/* Request the interrupt 
	chan->irq = irq_of_parse_and_map(node, chan->tdest);
	err = request_irq(chan->irq, xdev->dma_config->irq_handler,
			  IRQF_SHARED, "xilinx-dma-controller", chan);
	if (err) {
		dev_err(xdev->dev, "unable to request IRQ %d\n", chan->irq);
		return err;
	}
	*/
	
	
	/*
	 * Initialize the DMA channel and add it to the DMA engine channels
	 * list.
	 */
	chan->common.device = &ddev->common;

	list_add_tail(&chan->common.device_node, &ddev->common.channels);
        ddev->chan[chan->id] = chan;

	/* Reset the channel */
	err = dma_chan_reset(chan);
	if (err < 0) {
		dev_err(ddev->dev, "Reset channel failed\n");
		return err;
	}

	return 0;
}







static int dma_child_probe(struct c_dma_device *ddev,
				    struct device_node *node)
{
	int ret, i, nr_channels = 1;

	ret = of_property_read_u32(node, "dma-channels", &nr_channels);
	for (i = 0; i < nr_channels; i++)
		c_dma_chan_probe(ddev, node);

	return 0;
}

static int dma_probe(struct platform_device *pdev)
{
	int (*clk_init)(struct platform_device *, struct clk **, struct clk **,
			struct clk **, struct clk **, struct clk **)
					= axidma_clk_init;
	struct device_node *node = pdev->dev.of_node;
	struct c_dma_device *ddev;
	struct device_node *child, *np = pdev->dev.of_node;
	u32 addr_width;
      // u32 len_width;
	int i, err;

	/* Allocate and initialize the DMA engine structure */
	ddev = devm_kzalloc(&pdev->dev, sizeof(*ddev), GFP_KERNEL);
	if (!ddev)
		return -ENOMEM;

	ddev->dev = &pdev->dev;
	if (np) {
		const struct of_device_id *match;

		match = of_match_node(dma_of_ids, np);
		if (match && match->data) {
			ddev->dma_config = match->data;
			clk_init = ddev->dma_config->clk_init;
		}
	}
	if (!ddev->dma_config) {
 	   dev_err(&pdev->dev, "Missing DMA configuration from device tree\n");
 	   return -EINVAL;
	}

	clk_init = ddev->dma_config->clk_init;
	err = clk_init(pdev, &ddev->axi_clk, &ddev->tx_clk, &ddev->txs_clk,
		       &ddev->rx_clk, &ddev->rxs_clk);
	if (err)
		return err;
	
	ddev->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ddev->regs))
		return PTR_ERR(ddev->regs);
	ddev->s2mm_chan_id = max_channels / 2;
	err = of_property_read_u32(node, "xlnx,addrwidth", &addr_width);
	if (err < 0)
		dev_warn(ddev->dev, "missing xlnx,addrwidth property\n");

	if (addr_width > 32)
		ddev->ext_addr = true;
	else
		ddev->ext_addr = false;

	/* Set the dma mask bits */
	dma_set_mask(ddev->dev, DMA_BIT_MASK(addr_width));
	/* Initialize the DMA engine */
	ddev->common.dev = &pdev->dev;
	
	platform_set_drvdata(pdev, ddev);
	/* Initialize the channels */
	for_each_child_of_node(node, child) {
		err = dma_child_probe(ddev, child);
		if (err < 0)
			goto disable_clks;
	}
	
	/* Register the DMA engine with the core */
	dma_async_device_register(&ddev->common);

	err = of_dma_controller_register(node, of_dma_translate,
					 ddev);
	if (err < 0) {
		dev_err(&pdev->dev, "Unable to register DMA to DT\n");
		dma_async_device_unregister(&ddev->common);
		goto error;
	}


	

	dev_info(&pdev->dev, "Xilinx AXI DMA Engine Driver Probed!!\n");
	return 0;

disable_clks:
	disable_allclks(ddev);
error:
	for (i = 0; i < max_channels; i++)
		if (ddev->chan[i])
			dma_chan_remove(ddev->chan[i]);

	return err;

}
/**
 * dma_remove - Driver remove function
 * @pdev: Pointer to the platform_device structure
 *
 * Return: Always '0'
 */
static int dma_remove(struct platform_device *pdev)
{
	struct c_dma_device *ddev = platform_get_drvdata(pdev);
	int i;

	of_dma_controller_free(pdev->dev.of_node);

	dma_async_device_unregister(&ddev->common);

	for (i = 0; i < max_channels; i++)
		if (ddev->chan[i])
			dma_chan_remove(ddev->chan[i]);

	disable_allclks(ddev);

	return 0;
}
static struct platform_driver dma_driver = {
	.driver = {
		.name = "dma-vishnu",
		.of_match_table = dma_of_ids,
	},
	.probe = dma_probe,
	.remove = dma_remove,
};

module_platform_driver(dma_driver);

MODULE_AUTHOR("Vishnu, Solo.");
MODULE_DESCRIPTION("DMA driver for simple memory transfer");
MODULE_LICENSE("GPL v2");

