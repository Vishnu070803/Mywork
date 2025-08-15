

#define DMA_BASE_ADDRESS		0x40400000

#define DMA_SIZE			0x1000
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

#define DRIVER_NAME "DMA_driver"
#define DEVICE_NAME "DMA_device"
/*#define BUFFER_SIZE 1024*/

// Variables for sysfs attributes
static char srcaddr[100] = "0x00000000";
static char destaddr[100] = "0x00000000";
static char srclen[100] = "0x00000000";
static char destlen[100] = "0x00000000";

static struct class *sysfs_class;
static struct device *sysfs_device;

static int major_number;
struct custom_dma_channel{
	struct custom_dma_device *sdev;
	struct dma_chan common;
	struct device *dev;
	bool idle;
	u32 ctrl_offset;
	// bool err;
};

struct custom_dma_device{
  	struct device *dev;
	struct dma_device common;
	struct custom_dma_chan *channel[DMA_MAX_CHANS_PER_DEVICE];
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
    else if (strcmp(attr->attr.name, "destaddr") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", destaddr);
    else if (strcmp(attr->attr.name, "srclen") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", srclen);
    else if (strcmp(attr->attr.name, "destlen") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", destlen);

    return -EINVAL;
}
void dma_on(){
	u32 run = DMA_BASE_ADDRESS + DMA_REG_DMACR;
    	u32 value;
    // Map the physical address to kernel virtual space
    void __iomem *reg = ioremap(run, 4);
    if (!reg) {
        pr_err("Failed to map DMA register for run\n");
        return;
    }
        value = ioread32(reg);
	value |= (1 << 0);


    // Write value to the register
    dma_write(reg, value);

    // Unmap the memory after use
    iounmap(reg);
}

void dma_stop(){
	u32 run = DMA_BASE_ADDRESS + DMA_REG_DMACR;
    	u32 value;
    // Map the physical address to kernel virtual space
    void __iomem *reg = ioremap(run, 4);
    if (!reg) {
        pr_err("Failed to map DMA register for run\n");
        return;
    }
        value = ioread32(reg);
	value &=~(1 << 0);


    // Write value to the register
    dma_write(reg, value);

    // Unmap the memory after use
    iounmap(reg);
}

void poll(u32 channel_address) {
    u32 reset = DMA_BASE_ADDRESS + DMA_REG_DMASR + channel_address;
    u32 value;
    void __iomem *reg = ioremap(reset, 4);  // Map register to virtual memory
    
    if (!reg) {
        pr_err("Failed to map status register in poll function\n");
        return;
    }

    // Polling loop with timeout
    int timeout = 1000;  // Timeout in iterations (adjust as needed)
    while (timeout-- > 0) {
        value = ioread32(reg);  // Read the value from the register
        
        if (value & (1 << 0)) {  // Check if the 0th bit is set (DMA done or condition met)
            pr_info("0th bit is set, condition met!\n");
            break;  // Exit polling loop once the condition is met
        }
        pr_info("Still Polling\n");	
        msleep(10);  // Sleep for 1 ms to avoid busy-waiting
    }

    if (timeout <= 0) {
        pr_err("Timeout reached while polling the 0th bit\n");
    }

    iounmap(reg);  // Unmap the register after use
	dma_stop();
}


int mm2s_stransfer(unsigned long data){
	u32 reset = DMA_BASE_ADDRESS + DMA_MM2S_CTRL_OFFSET + DMA_REG_DMASR;
	u32 value;
	int ret;
	void __iomem *reg = ioremap(reset, 4);
	if(!reg){
	pr_err("Failed to map status register for mm2s");
	 return -1;
	}
	value = ioread32(reg);
	value &= ~(1 << 0);
	dma_write(reg, value);
	iounmap(reg);
	ret = dma_srclen(data);
	if (ret ==0){
		return 0;
	}
	else {
		pr_err("Failed in mm2s transfer\n");
	
		return ret;
	}
	
}
int s2mm_stransfer(unsigned long data){
	u32 reset = DMA_BASE_ADDRESS + DMA_S2MM_CTRL_OFFSET + DMA_REG_DMASR;
	u32 value;
	int ret;
	void __iomem *reg = ioremap(reset, 4);
	if(!reg){
	pr_err("Failed to map status register for mm2s");
	 return -EBUSY;
	}
	value = ioread32(reg);
	value &= ~(1<<0);
	dma_write(reg, value);
	iounmap(reg);
	dma_destlen(data);
	if (ret ==0){
		return 0;
	}
	else {
		pr_err("Failed in mm2s transfer\n");
	
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
	dma_on();
    if (strcmp(attr->attr.name, "srcaddr") == 0){
        snprintf(srcaddr, sizeof(srcaddr), "%.*s", (int)count, buf);
	ret = kstrtoul(buf, 0, &value);
        if (ret){
          return ret;}
	dma_srcaddr(value);
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
        if (ret ==0){	
		poll(DMA_MM2S_CTRL_OFFSET);
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
		poll(DMA_S2MM_CTRL_OFFSET);
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


static inline void dma_write(void __iomem *reg, u32 value)
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

void dma_srcaddr(unsigned long buf){
	u32 srcaddr = DMA_BASE_ADDRESS + DMA_MM2S_CTRL_OFFSET + DMA_REG_SRCDSTADDR;
    
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(srcaddr, 4);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return;
    }

    // Write value to the register
    dma_write(reg_base, buf);

    // Unmap the memory after use
    iounmap(reg_base);
}
void dma_destaddr(unsigned long buf){
	u32 destaddr = DMA_BASE_ADDRESS + DMA_S2MM_CTRL_OFFSET + DMA_REG_SRCDSTADDR;
    
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(destaddr, 4);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return;
    }

    // Write value to the register
    dma_write(reg_base, buf);

    // Unmap the memory after use
    iounmap(reg_base);
}
int dma_srclen(unsigned long buf){
	u32 srclen = DMA_BASE_ADDRESS + DMA_MM2S_CTRL_OFFSET + DMA_REG_BTT;
    
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(srclen, 4);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return;
    }

    // Write value to the register
    dma_write(reg_base, buf);
	if (ioread32(reg_base) == buf) {
        	pr_info("Successfully wrote 0x%x to DDR address 0x%llx\n", value, (unsigned long long)reg_base);
		return 0;
    	} else {
        	pr_err("Write verification failed at DDR address 0x%llx\n", (unsigned long long)reg_base);
		return -EIO; // Input/Output error
    	} 

    // Unmap the memory after use
    iounmap(reg_base);
}
int dma_destlen(unsigned long buf){
	int ret;
	u32 destlen = DMA_BASE_ADDRESS + DMA_S2MM_CTRL_OFFSET + DMA_REG_BTT;
    
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(destlen, 4);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return -EIO;
    }
		
    // Write value to the register
    dma_write(reg_base, buf);
    if (ioread32(reg_base) == buf) {
        pr_info("Successfully wrote 0x%x to DDR address 0x%llx\n", buf, (unsigned long long)reg_base);
	return 0;
    } else {
        pr_err("Write verification failed at DDR address 0x%llx\n", (unsigned long long)reg_base);
	return -1;
    }    // Unmap the memory after use
    iounmap(reg_base);
}


// Device file operations implementation
static int dev_open(struct inode *inode, struct file *file)
{
    pr_info("Device file opened\n");
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset)
{
    /*char temp_buffer[BUFFER_SIZE];
    int bytes_to_copy;

    // Combine all attributes into a single buffer
    snprintf(temp_buffer, sizeof(temp_buffer),
             "address1: %s\naddress2: %s\nsize: %s\n",
             address1, address2, size);

    if (*offset >= strlen(temp_buffer))
        return 0; // EOF

    // Copy data from kernel space to user space
    bytes_to_copy = min(len, strlen(temp_buffer) - (size_t)*offset);
    if (copy_to_user(user_buffer, temp_buffer + *offset, bytes_to_copy))
        return -EFAULT;

    *offset += bytes_to_copy;
    pr_info("Device file read: %d bytes\n", bytes_to_copy);
    return bytes_to_copy;*/
	pr_info("Device file read\n");
	    return 0;

}

static ssize_t dev_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset)
{
  /*  char temp_buffer[BUFFER_SIZE];
    char new_address1[100], new_address2[100], new_size[100];

    if (len >= sizeof(temp_buffer))
        return -EINVAL;

    // Copy data from user space to kernel space
    if (copy_from_user(temp_buffer, user_buffer, len))
        return -EFAULT;

    temp_buffer[len] = '\0'; // Null-terminate the string

    // Parse the input into three attributes
    if (sscanf(temp_buffer, "address1: %99s address2: %99s size: %99s", 
               new_address1, new_address2, new_size) == 3) {
        // Update the attributes
        strncpy(srcaddr, new_address1, sizeof(srcaddr));
        strncpy(destaddr, new_address2, sizeof(destaddr));
        strncpy(srclen, new_size, sizeof(srclen));

        pr_info("Device file written: srcaddr=%s, destaddr2=%s, srcaddr=%s\n",
                srcaddr, destaddr, srclen);
        return len;
    }

    pr_err("Invalid input format. Expected: srcaddr: <value> destaddr: <value> srclen: <value>\n");
    return -EINVAL; */
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
	int err;
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
	
	chan->common.device = &ddev->common;
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
	if(ret == 0){ 
		return 0;
	}
	else{
	return -EINVAL;
	}
}

 
static int custom_dma_probe(struct platform_device *pdev){
	struct device_node *node = pdev->dev.of_node; //pointer to the node in device tree
	struct custom_dma_device *ddev; //pointer to custom_dma_device structure
	u32 addr_width;
	struct resource *res;
	int err, ret;
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

  	  pr_info("my_driver: AXI DMA physical address start = 0x%pa\n", &res->start)
	/* Request and map I/O memory */
	ddev->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ddev->regs))
		return PTR_ERR(ddev->regs);
	err = of_property_read_u32(node, "xlnx,addrwidth", &addr_width);
	if (err < 0)
		dev_warn(ddev->dev, "missing xlnx,addrwidth property\n");
	/* Set the dma mask bits */
	dma_set_mask(ddev->dev, DMA_BIT_MASK(addr_width));

	/* Initialize the DMA engine */
	ddev->common.dev = &pdev->dev;

	platform_set_drvdata(pdev, ddev); //storing data for future use in remove function
	/* Initialize the channels */
	for_each_child_of_node(node, child) {
	err = custom_dma_child_probe(ddev, child);
	if (err < 0){
		pr_err("Error at channel probing");
		}
	}
	/* Register the DMA engine with the core */
	dma_async_device_register(&ddev->common);
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


	
	dev_info(&pdev->dev, "AXI DMA Engine Driver Probed!!\n");
	return 0;
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
	struct custom_dma_device *ddev = platform_get_drvdata(pdev);
	dma_async_device_unregister(&ddev->common);
	
    	device_remove_file(sysfs_device, &dev_attr_srclen);
	device_remove_file(sysfs_device, &dev_attr_destlen);
    	device_remove_file(sysfs_device, &dev_attr_srcaddr);
    	device_remove_file(sysfs_device, &dev_attr_destaddr);

    	// Destroy the device and class
    	device_destroy(sysfs_class, MKDEV(major_number, 0));
    	class_destroy(sysfs_class);

    	// Unregister the major number
    	unregister_chrdev(major_number, DEVICE_NAME);

    	pr_info("Sysfs driver with device file removed\n");

	return 0;
}

static const struct of_device_id custom_dma_of_ids[] = {
	{ .compatible = "prototype-1", },
	{}
};
MODULE_DEVICE_TABLE(of, custom_dma_of_ids);


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

