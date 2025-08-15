#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/bitops.h>
#include <linux/dmapool.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of_address.h>
#include <linux/of_dma.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/dmaengine.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DMA_BASE_ADDRESS		0x40400000
//#define DMA_SIZE			0x1000

/* Register/Descriptor Offsets */
#define DMA_MM2S_CTRL_OFFSET		0x0000
#define DMA_S2MM_CTRL_OFFSET		0x0030

/* Control Registers */
//#define DMA_REG_DMACR			0x0000
/* Status Registers */
//#define DMA_REG_DMASR			0x0004


/* AXI DMA Specific Registers/Offsets */
//#define DMA_REG_SRCDSTADDR	0x18
//#define DMA_REG_BTT		0x28

/* HW specific definitions */

//#define DMA_MAX_CHANS_PER_DEVICE	0x2

#define DRIVER_NAME "DMA_driver"
#define DEVICE_NAME "DMA_device"
/*#define BUFFER_SIZE 1024*/

// Variables for sysfs attributes
static char srcaddr[100] = "0x00000000";
static char destaddr[100] = "0x00000000";
static char srclen[100] = "0x00000000";
static char destlen[100] = "0x00000000";
static char dmaon[100] = "0x00000000";
static char dmaoff[100] = "0x00000000";

static struct class *sysfs_class;
static struct device *sysfs_device;

static int major_number;
struct custom_dma_channel{
	struct custom_dma_device *sdev;
	//struct dma_chan common;
	struct device *dev;
	bool idle;
	u32 ctrl_offset;
	// bool err;
};

struct custom_dma_device{
  	struct device *dev;
	//struct dma_device common;
	struct platform_device *pdev;
	void __iomem *regs;
//	int mm2s_id;
//	int s2mm_id;
};

static int dev_open(struct inode *inode, struct file *file);
static ssize_t dev_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset);
static ssize_t dev_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset);
static int dev_release(struct inode *inode, struct file *file);

// Define file operations structure
static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static ssize_t attr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (strcmp(attr->attr.name, "srcaddr") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", srcaddr);
    else if (strcmp(attr->attr.name, "dmaon") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", dmaon);
    else if (strcmp(attr->attr.name, "dmaoff") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", dmaoff);
    else if (strcmp(attr->attr.name, "destaddr") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", destaddr);
    else if (strcmp(attr->attr.name, "srclen") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", srclen);
    else if (strcmp(attr->attr.name, "destlen") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", destlen);

    return -EINVAL;
}

 void dma_write(void __iomem *reg, u32 value)
{
	iowrite32(value, reg);
}
unsigned long dma_read(void __iomem *reg) {
    if (!reg) {
        pr_err("Invalid register base address\n");
        return 0;  // Return 0 to indicate an error
    }

    return ioread32(reg);  // Read a 32-bit value from the register
}

void dma_on(unsigned long offset){
	u32 run = 0x40400000 + offset ;
    	u32 value;
  	  // Map the physical address to kernel virtual space
  	void __iomem *reg = ioremap(run, 4);
	pr_info("Address mapping in dma_on :%u", run);
  	  if (!reg) {
   	     pr_err("Failed to map DMA register for run\n");
  	      return;
 	  }
        value = ioread32(reg);
	pr_info("Checking before writing value: %x", value);	
	value |= (1 << 0);
 	   // Write value to the register
   	dma_write(reg, value);
	value = ioread32(reg);
  	  // Unmap the memory after use
	pr_info("Checking the written value: %x", value);
  	iounmap(reg);
}

void dma_off(unsigned long offset){
	u32 run = 0x40400000 + offset;
    	u32 value;
    // Map the physical address to kernel virtual space
    void __iomem *reg = ioremap(run, 4);
    pr_info("Address mapping in dma_off :%x", run);
    if (!reg) {
        pr_err("Failed to map DMA register for run\n");
        return;
    }
        value = ioread32(reg);
	pr_info("Checking before writing value: %x", value);	
	value &=~(1 << 0);


    // Write value to the register
    dma_write(reg, value);
    value = ioread32(reg);
    pr_info("Checking the written value: %x", value);    
    // Unmap the memory after use
    iounmap(reg);
}

void poll(u32 channel_address) {
    u32 reset = channel_address;//address of channel status registers
    u32 value;
    int timeout = 1000;  // Timeout in iterations (adjust as needed)
    void __iomem *reg = ioremap(reset, 4);  // Map register to virtual memory
    pr_info("Address mapping in polling :%x", reset);
    if (!reg) {
        pr_err("Failed to map status register in poll function\n");
        return;
    }

    // Polling loop with timeout

    while (timeout-- > 0) {
        value = ioread32(reg);  // Read the value from the register
        
        if (value & (1 << 1)) {  // Check if the 1th bit is set (channel idle)
            pr_info("1th bit is set, condition met, channel idle!\n");
            break;  // Exit polling loop once the condition is met
        }
        pr_info("Still Polling\n");	
        msleep(10);  // Sleep for 10 ms to avoid busy-waiting
    }

    if (timeout <= 0) {
        pr_err("Timeout reached while polling the 1th bit\n");
    }

    iounmap(reg);  // Unmap the register after use
}


void dma_srcaddr(unsigned long buf){
	u32 srcaddr = 0x40400000 + 0x00000000 + 0x00000018;
    
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(srcaddr, 4);
	pr_info("Mapping address in dma_srcaddr function :%x", srcaddr);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return;
    }
	pr_info("Data from userspace : %lu", buf);
    // Write value to the register
    dma_write(reg_base, buf);

    // Unmap the memory after use
    iounmap(reg_base);
}
void dma_destaddr(unsigned long buf){
	u32 destaddr =0x40400000 + 0x00000030 + 0x00000018;
    
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(destaddr, 4);
    pr_info("Data from userspace : %lu", buf);
    pr_info("Mapping address in dma_destaddr function :%x", destaddr);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return;
    }

    // Write value to the register
    dma_write(reg_base, buf);

    // Unmap the memory after use
    iounmap(reg_base);
}
int dma_srclen(unsigned long data){
	u32 srclen = 0x40400000 + 0x00000000 + 0x00000028;
    	unsigned long buf = data;
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(srclen, 4);
    pr_info("Mapping address in dma_srclen function :%x", srclen);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return -ENOMEM;
    }

    // Write value to the register
    dma_write(reg_base, buf);
	if (ioread32(reg_base) == buf) {
        	pr_info("Successfully wrote 0x%lx to DDR address 0x%p\n", buf, reg_base);
		iounmap(reg_base);
		return 0;
    	} else {
        	pr_err("Write verification failed at DDR address 0x%p\n", reg_base);
		iounmap(reg_base);
		return -EIO; // Input/Output error
    	}  
}
int dma_destlen(unsigned long data){
	u32 destlen = 0x40400000 + 0x00000030 + 0x00000028;
    	unsigned long buf = data;
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(destlen, 4);
	pr_info("Mapping address in dma_destlen function :%x", destlen);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return -EIO;
    }
		
    // Write value to the register
    dma_write(reg_base, buf);
    if (ioread32(reg_base) == buf) {
        pr_info("Successfully wrote 0x%lx to DDR address 0x%p\n", buf, reg_base);
	return 0;
    } else {
        pr_err("Write verification failed at DDR address 0x%p for data %x\n", reg_base, buf);
	return -1;
    }    // Unmap the memory after use
    iounmap(reg_base);
}



int mm2s_stransfer(unsigned long data){
	u32 reset = 0x40400000 + 0x00000000 + 0x00000004;
	u32 value;
	int ret;
	unsigned long buf = data;
	void __iomem *reg = ioremap(reset, 4);
	if(!reg){
	pr_err("Failed to map status register for mm2s");
	 return -1;
	}
	
	value = ioread32(reg);
//	value &= ~(1 << 0);
	pr_info("Verifying does the channel is running or not (last bit 0):%x", value);
	dma_write(reg, value);
	iounmap(reg);
	ret = dma_srclen(buf);
	if (ret ==0){
		return 0;
	}
	else {
		pr_err("Failed in mm2s transfer\n");
	
		return ret;
	}
	
}
int s2mm_stransfer(unsigned long data){
	u32 reset =0x40400000 + 0x00000030 + 0x00000004;
	u32 value;
	int ret;
	unsigned long buf = data;
	void __iomem *reg = ioremap(reset, 4);
	pr_info("Mapping address in s2mm_stransfer :%x", reset);
	if(!reg){
	pr_err("Failed to map status register for s2mm");
	 return -EBUSY;
	}
	value = ioread32(reg);
	//value &= ~(1<<0);
	pr_info("Verifying does the channel is running or not (last bit 0):%x", value);
	dma_write(reg, value);
	iounmap(reg);
	ret = dma_destlen(buf);
	if (ret ==0){
		return 0;
	}
	else {
		pr_err("Failed in s2mm transfer\n");
		return ret;
	}

}

/*void error_check(u32 channel_address){
    u32 reset = DMA_BASE_ADDRESS + channel_address + DMA_REG_DMASR;
    u32 value;
    
    // Map the physical address to kernel virtual space
    void __iomem *reg = ioremap(reset, 4);
    if (!reg) {
        pr_err("Failed to map status register in error check");
        return;
    }
    
    // Read the value from the status register
    value = ioread32(reg);
    
    // Check if bit 6 is set (indicating an error)
    if (value & (1 << 6)) {
        pr_err("Error detected in bit 6 of status register");
        dma_stop();  // Stop the DMA transfer
        iounmap(reg);
        return;
    }
    
    pr_info("No error");

    // Unmap the memory after use
    iounmap(reg);
}*/



// Generic write attribute function
static ssize_t attr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{   
	unsigned long value;
	int ret;
    if (strcmp(attr->attr.name, "srcaddr") == 0){
        snprintf(srcaddr, sizeof(srcaddr), "%.*s", (int)count, buf);
	ret = kstrtoul(buf, 0, &value);
        if (ret){
          return ret;}
	dma_srcaddr(value);
	}
    else if (strcmp(attr->attr.name, "dmaon") == 0){
        snprintf(dmaon, sizeof(dmaon), "%.*s", (int)count, buf);
	ret = kstrtoul(buf, 0, &value);
        if (ret){
          return ret;}
	dma_on(value);
	}
    else if (strcmp(attr->attr.name, "dmaoff") == 0){
        snprintf(dmaoff, sizeof(dmaoff), "%.*s", (int)count, buf);
	ret = kstrtoul(buf, 0, &value);
        if (ret){
          return ret;}
	dma_off(value);
	}


    else if (strcmp(attr->attr.name, "destaddr") == 0){
        snprintf(destaddr, sizeof(destaddr), "%.*s", (int)count, buf);
	
	ret = kstrtoul(buf, 0, &value);
        if (ret){
          return ret;}
	dma_destaddr(value);
	}
	
    else if (strcmp(attr->attr.name, "srclen") == 0){
	
        snprintf(srclen, sizeof(srclen), "%.*s", (int)count, buf);
	ret = kstrtoul(buf, 0, &value);
        if (ret){
          return ret;}
	ret = mm2s_stransfer(value);
        msleep(1000);  // Sleep for 10 ms to avoid busy-waiting
        if (ret ==0){	
		poll(0x40400004);
		pr_info("mm2s transfer completed");
		}
	   else{
		pr_err("Error detected in mm2s transfer");	
		}
	}
    else if (strcmp(attr->attr.name, "destlen") == 0){
        snprintf(destlen, sizeof(destlen), "%.*s", (int)count, buf);
		ret = kstrtoul(buf, 0, &value);
        if (ret){
          return ret;}
	ret = s2mm_stransfer(value);
        if (ret ==0){	
		poll(0x40400034);
		pr_info("s2mm transfer completed");
		
		}
	   else{
		pr_err("Error detected in s2mm transfer");	
		}
	}
    else
        return -EINVAL;

    return count;
}

// Declare the device attributes
static DEVICE_ATTR(srcaddr, 0664, attr_show, attr_store);
static DEVICE_ATTR(destaddr, 0664, attr_show, attr_store);
static DEVICE_ATTR(srclen, 0664, attr_show, attr_store);
static DEVICE_ATTR(destlen, 0664, attr_show, attr_store);
static DEVICE_ATTR(dmaon, 0664, attr_show, attr_store);
static DEVICE_ATTR(dmaoff, 0664, attr_show, attr_store);

// Device file operations implementation
static int dev_open(struct inode *inode, struct file *file)
{
    pr_info("Device file opened\n");
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset)
{
 	pr_info("Device file read\n");
	    return 0;

}

static ssize_t dev_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset)
{

	pr_info("Device file write\n");
	    return 0;

}


static int dev_release(struct inode *inode, struct file *file)
{
    pr_info("Device file closed\n");
    return 0;
}


static int custom_dma_chan_probe(struct custom_dma_device *ddev,
				  struct device_node *node)
{
	struct custom_dma_channel *chan;
	void __iomem *reg_base;
	u32 value;


	/* Allocate and initialize the channel structure */
	chan = devm_kzalloc(ddev->dev, sizeof(*chan), GFP_KERNEL);
	if (!chan)
		return -ENOMEM;
	chan->idle = true;
	if(of_device_is_compatible(node, "xlnx,axi-dma-mm2s-channel")){
	chan->ctrl_offset = DMA_MM2S_CTRL_OFFSET;
	
	}
	else if(of_device_is_compatible(node, "xlnx,axi-dma-s2mm-channel")){
	chan->ctrl_offset = DMA_S2MM_CTRL_OFFSET;
	}else {
		dev_err(ddev->dev, "Invalid channel compatible node\n");
		return -EINVAL;
	}
	
//	chan->common.device = &ddev->common;
	// Reset the dma engine
	reg_base = ioremap(DMA_BASE_ADDRESS + DMA_MM2S_CTRL_OFFSET , 4);
    	// Read the current value of the register
    	value = ioread32(reg_base);

    	// Set bit 2 (counting from 0) to 1
    	value |= (1 << 2);

    	// Write the updated value back to the register
    	iowrite32(value, reg_base); 
	
	return 0;
}

static int custom_dma_child_probe(struct custom_dma_device *ddev,
				    struct device_node *node)
{
	int ret, i, nr_channels = 1;

	ret = of_property_read_u32(node, "dma-channels", &nr_channels);
	for (i = 0; i < nr_channels; i++){
	ret = custom_dma_chan_probe(ddev, node);
	if(ret < 0){ 
		return -EINVAL;
	}
	}
	return 0;
}
static const struct of_device_id custom_dma_of_ids[] = {
	{ .compatible = "prototype-1", },
	{}
};
MODULE_DEVICE_TABLE(of, custom_dma_of_ids);

 
static int custom_dma_probe(struct platform_device *pdev){
	struct device_node *child; //pointer to the node in device tree
	struct custom_dma_device *ddev; //pointer to custom_dma_device structure
	u32 addr_width;
	struct resource *res;
	int err, ret;
	struct device_node *node = pdev->dev.of_node; //pointer to the node in device tree	
	ddev = devm_kzalloc(&pdev->dev, sizeof(*ddev), GFP_KERNEL);
	if (!ddev)
		return -ENOMEM;
	ddev->dev = &pdev->dev;
	
	/* Dma address finding */
	   res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
   	 if (!res) {
       	 dev_err(&pdev->dev, "Failed to get platform resource\n");
  	      return -EINVAL;
	}

  	  pr_info("my_driver: AXI DMA physical address start = 0x%pa\n", &res->start);

	/* Request and map I/O memory */
	ddev->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ddev->regs))
		return PTR_ERR(ddev->regs);
	pr_info("my_driver: AXI DMA remaped address in probing= 0x%pa\n", &ddev->regs);

	err = of_property_read_u32(node, "xlnx,addrwidth", &addr_width);
	if (err < 0) {
    		addr_width = 32;  // Default value
    		dev_warn(ddev->dev, "Missing xlnx,addrwidth property, using default value of %d\n", addr_width);
	}

	/* Set the dma mask bits */
	
	if (dma_set_mask(ddev->dev, DMA_BIT_MASK(addr_width))) {
   		 dev_err(ddev->dev, "Failed to set DMA mask for %d-bit addressing\n", addr_width);
		 return -EIO;
	}

	dev_info(ddev->dev, "DMA mask set to %d-bit successfully\n", addr_width);

	/* Initialize the channels */
	for_each_child_of_node(node, child) {
	err = custom_dma_child_probe(ddev, child);
	if (err < 0){
		pr_err("Error at channel probing");
		}
	}
	 major_number = register_chrdev(0, DEVICE_NAME, &fops);
        if (major_number < 0) {
        pr_err("Failed to register a major number\n");
        return major_number;
        }

        // Create a class in /sys/class/
        sysfs_class = class_create(THIS_MODULE, DRIVER_NAME);
        if (IS_ERR(sysfs_class)) {
        pr_err("Failed to create class\n");
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sysfs_class);
        }

        // Create a device in the class
        sysfs_device = device_create(sysfs_class, NULL, MKDEV(major_number, 0), NULL, DRIVER_NAME);
        if (IS_ERR(sysfs_device)) {
        pr_err("Failed to create device\n");
        class_destroy(sysfs_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sysfs_device);
        }

        // Create the attribute files
        ret = device_create_file(sysfs_device, &dev_attr_srcaddr);
        if (ret)
        goto fail_attr1;

        ret = device_create_file(sysfs_device, &dev_attr_destaddr);
        if (ret)
        goto fail_attr2;

        ret = device_create_file(sysfs_device, &dev_attr_srclen);
        if (ret)
        goto fail_attr3;
	
	
        ret = device_create_file(sysfs_device, &dev_attr_destlen);
        if (ret)
        goto fail_attr4;
	
	ret = device_create_file(sysfs_device, &dev_attr_dmaon);
        if (ret)
        goto fail_attr5;
	
	ret = device_create_file(sysfs_device, &dev_attr_dmaoff);
        if (ret)
        goto fail_attr6;




	
	dev_info(&pdev->dev, "AXI DMA Engine Driver Probed!!\n");
	return 0;

	fail_attr6:
    	device_remove_file(sysfs_device, &dev_attr_dmaon);

	fail_attr5:
    	device_remove_file(sysfs_device, &dev_attr_destlen);

	fail_attr4:
    	device_remove_file(sysfs_device, &dev_attr_srclen);

	fail_attr3:
    	device_remove_file(sysfs_device, &dev_attr_destaddr);
	fail_attr2:
	    device_remove_file(sysfs_device, &dev_attr_srcaddr);
	fail_attr1:
    	device_destroy(sysfs_class, MKDEV(major_number, 0));
    	class_destroy(sysfs_class);
    	unregister_chrdev(major_number, DEVICE_NAME);
	return ret;
}

static int custom_dma_remove(struct platform_device *pdev)
{
	
    	device_remove_file(sysfs_device, &dev_attr_srclen);
	device_remove_file(sysfs_device, &dev_attr_destlen);
    	device_remove_file(sysfs_device, &dev_attr_srcaddr);
    	device_remove_file(sysfs_device, &dev_attr_destaddr);
	device_remove_file(sysfs_device, &dev_attr_dmaon);
    	device_remove_file(sysfs_device, &dev_attr_dmaoff);
	
	device_destroy(sysfs_class, MKDEV(major_number, 0));
    	class_destroy(sysfs_class);

    	unregister_chrdev(major_number, DEVICE_NAME);

    	pr_info("DMA drive Removed\n");

	return 0;
}
static struct platform_driver custom_dma_driver = {
	.driver = {
		.name = "vidma",
		.of_match_table = custom_dma_of_ids,
	},
	.probe =custom_dma_probe,
	.remove = custom_dma_remove,
};

module_platform_driver(custom_dma_driver);

MODULE_AUTHOR("Vishnu, Solo.");
MODULE_DESCRIPTION("DMA driver for simple memory transfer");
MODULE_LICENSE("GPL v2");


