
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
#include <linux/poll.h>
#include <linux/wait.h>

/* Base address */
#define DMA_BASE_ADDRESS		0x40400000

/* Register/Descriptor Offsets */
#define DMA_MM2S_CTRL_OFFSET		0x00000000
#define DMA_S2MM_CTRL_OFFSET		0x00000030

/* Control Registers */
#define DMA_REG_DMACR			0x00000000
/* Status Registers */
#define DMA_REG_DMASR			0x00000004


/* AXI DMA Specific Registers/Offsets */
#define DMA_REG_SRCDSTADDR	0x18
#define DMA_REG_BTT		0x28

/* Bit Manipulation */
#define SET_BIT(value, bit) ((value) |= (1 << (bit)))
#define CLEAR_BIT(value, bit) ((value) &= ~(1 << (bit)))
#define CHECK_BIT(value, bit) ((value) & (1 << (bit)))

#define DRIVER_NAME "DMA_driver"
#define DEVICE_NAME "DMA_device"
#define BUFFER_SIZE 1000


// Variables for sysfs attributes
static char srcaddr[100] = "0x00000000";
static char destaddr[100] = "0x00000000";
static char srclen[100] = "0x00000000";
static char destlen[100] = "0x00000000";
static char dmaon[100] = "0x00000000";
static char dmaoff[100] = "0x00000000";
static char errcheck[100] = "0x00000000";

static DECLARE_WAIT_QUEUE_HEAD(my_waitqueue);  // A wait queue for poll
static bool my_condition_met = false;         // The condition to check for polling

static struct class *sysfs_class;
static struct device *sysfs_device;

static int major_number;
struct custom_dma_channel{
	struct custom_dma_device *sdev;
	struct device *dev;
	bool idle;
	u32 ctrl_offset;
};

struct custom_dma_device{
  	struct device *dev;
	struct platform_device *pdev;
	void __iomem *regs;
	u32 base_address;
	u32 dma_size;
};

static int dev_open(struct inode *inode, struct file *file);
static ssize_t dev_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset);
static ssize_t dev_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset);
static int dev_release(struct inode *inode, struct file *file);
static unsigned int dev_poll(struct file *file, struct poll_table_struct *poll_table);

// Define file operations structure 
static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
    .poll = dev_poll,
};

static ssize_t attr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (strcmp(attr->attr.name, "srcaddr") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", srcaddr);
    else if (strcmp(attr->attr.name, "errcheck") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", errcheck);
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

 int dma_write(void __iomem *reg, u32 value, char *array)
{
 	u32 dummy;
	char *string = array;
	u32 check = value;
	iowrite32(value, reg);
	dummy =	ioread32(reg);
	if (dummy == check) {
        	pr_info("%s-success and value on register is 0x%x \n",string, dummy);
		iounmap(reg);
		return 0;
    	} else {
        	pr_err("%s-failed and value on register is 0x%x\n",string, dummy);
		iounmap(reg);
		return -EIO; // Input/Output error
    	}  

}
unsigned long dma_read(void __iomem *reg) {
    if (!reg) {
        pr_err("Invalid register base address\n");
        return 0;  // Return 0 to indicate an error
    }

    return ioread32(reg);  // Read a 32-bit value from the register
}

void dma_on(unsigned long offset){
	u32 run = DMA_BASE_ADDRESS + offset ;
    	u32 value;
	char *desc = " DMA ON BIT";
  	  // Map the physical address to kernel virtual space
  	void __iomem *reg = ioremap(run, 4);
	pr_info("Address mapping in dma_on :%u", run);
  	  if (!reg) {
   	     pr_err("Failed to map DMA register for DMA_ON\n");
  	      return;
 	  }
        value = ioread32(reg);
	pr_info("Checking before writing value: 0x%x\n", value);	
	SET_BIT(value, 0);
 	   // Write value to the register
   	dma_write(reg, value, desc);
	value = ioread32(reg);
  	  // Unmap the memory after use
	pr_info("Checking the written value: 0x%x\n", value);
  	iounmap(reg);
}

void dma_off(unsigned long offset){
	u32 run = DMA_BASE_ADDRESS + offset;
    	u32 value;
	char *desc = " DMA OFF BIT";	
    // Map the physical address to kernel virtual space
    void __iomem *reg = ioremap(run, 4);
    pr_info("Address mapping in dma_off :%x", run);
    if (!reg) {
        pr_err("Failed to map DMA register for run\n");
        return;
    }
        value = ioread32(reg);
	pr_info("Checking before writing value: %x", value);	
	CLEAR_BIT(value, 0);

    // Write value to the register
    dma_write(reg, value, desc);
    value = ioread32(reg);
    pr_info("Checking the written value: %x", value);    
    // Unmap the memory after use
    iounmap(reg);
}

void error_check(u32 channel_address){
    u32 error = DMA_BASE_ADDRESS + channel_address + DMA_REG_DMASR;
    u32 value;
    
    // Map the physical address to kernel virtual space
    void __iomem *reg = ioremap(error, 4);
    if (!reg) {
        pr_err("Failed to map status register in error check");
        return;
    }
    
    // Read the value from the status register
    value = ioread32(reg);
    
    // Check if bit 6 is set (indicating an error)
    if (CHECK_BIT(value, 6)) {
        pr_err("Error detected in bit 6 of status register");
        dma_off(channel_address);  // Stop the DMA transfer
        iounmap(reg);
        return;
    }
    
    pr_info("No error");

    iounmap(reg);
}


int poll(u32 channel_address) {
    
    u32 check = channel_address;//address of channel status registers
    u32 value;
    int timeout = 10000;  // Timeout in iterations (adjust as needed)
    char *desc = " DMA INTERRUPT BIT CLEARING ";
    void __iomem *reg = ioremap(check, 4);  // Map register to virtual memory
    pr_info("Address mapping in polling :0x%x", check);
    if (!reg) {
        pr_err("Failed to map status register in poll function\n");
        return -EIO;
    }

    // Polling loop with timeout

    while (timeout-- > 0) {
        value = ioread32(reg);  // Read the value from the register
        
        if (CHECK_BIT(value, 1)) {  // Check if the 1th bit is set (channel idle)
            pr_info("1th bit is set, condition met, channel idle!\n");
	    my_condition_met = true;
	    msleep(10);
	    // clearing interrupt bit after transfer for reuse the dma
	    SET_BIT(value, 12); 
	    check = value;
	    iowrite32(value, reg);
	    value = ioread32(reg);
	    if (value != check) {
        	pr_info("%s-success and value on register is 0x%x \n",desc, value);
		iounmap(reg);
		return 0;
    	    } else {
        	pr_err("%s-failed and value on register is 0x%x\n",desc, value);
		iounmap(reg);
		return -EIO; // Input/Output error
	    }  
            return 0;  // Exit polling loop once the condition is met
        }
        msleep(1);  // Sleep for 1 ms to avoid busy-waiting
    }

    if (timeout <= 0) {
        pr_err("Timeout reached while polling the 1th bit\n");
	pr_info("Checking for error");
	error_check(channel_address - DMA_REG_DMASR);
    }

    iounmap(reg);  // Unmap the register after use
    return -EIO;
}


void dma_srcaddr(unsigned long buf){
	u32 srcaddr = DMA_BASE_ADDRESS + DMA_MM2S_CTRL_OFFSET + DMA_REG_SRCDSTADDR;
	char *desc = " DMA SOURCE ADDRESS WRITING ";    
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(srcaddr, 4);
	pr_info("Mapping address in dma_srcaddr function :%x", srcaddr);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return;
    }
	pr_info("Data from userspace : %lu", buf);
    // Write value to the register
    dma_write(reg_base, buf, desc);

    // Unmap the memory after use
    iounmap(reg_base);
}
void dma_destaddr(unsigned long buf){
	u32 destaddr =DMA_BASE_ADDRESS + DMA_S2MM_CTRL_OFFSET + DMA_REG_SRCDSTADDR;
	char *desc = " DMA SOURCE ADDRESS WRITING ";        
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(destaddr, 4);
    pr_info("Data from userspace : %lu", buf);
    pr_info("Mapping address in dma_destaddr function :%x", destaddr);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return;
    }

    // Write value to the register
    dma_write(reg_base, buf, desc);

    // Unmap the memory after use
    iounmap(reg_base);
}
int dma_srclen(unsigned long data){
	int ret;
	u32 srclen = DMA_BASE_ADDRESS + DMA_MM2S_CTRL_OFFSET + DMA_REG_BTT;
    	unsigned long buf = data;
	char *desc = " DMA SOURCE LENGTH WRITING ";    
	
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(srclen, 4);
    pr_info("Mapping address in dma_srclen function :%x", srclen);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return -ENOMEM;
    }

    // Write value to the register
    ret = dma_write(reg_base, buf, desc);
    iounmap(reg_base);
	return ret;
}
int dma_destlen(unsigned long data){
	int ret;
	u32 destlen = DMA_BASE_ADDRESS + DMA_S2MM_CTRL_OFFSET + DMA_REG_BTT;
    	unsigned long buf = data;
	char *desc = " DMA DESTINATION LENGTH WRITING ";    	
    // Map the physical address to kernel virtual space
    void __iomem *reg_base = ioremap(destlen, 4);
	pr_info("Mapping address in dma_destlen function :%x", destlen);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return -EIO;
    }
		
    // Writing value
    ret = dma_write(reg_base, buf, desc);
    iounmap(reg_base);
	return ret;
}



int mm2s_stransfer(unsigned long data){
	u32 check = DMA_BASE_ADDRESS + DMA_MM2S_CTRL_OFFSET + DMA_REG_DMASR;
	u32 value;
	int ret;
	unsigned long buf = data;
	void __iomem *reg = ioremap(check, 4);
	pr_info("Mapping address in s2mm_stransfer :%x", check);	
	if(!reg){
	pr_err("Failed to map status register for mm2s");
	 return -1;
	}
	
	value = ioread32(reg);
	pr_info("Verifying does the channel is running or not (last bit 0):%x", value);
	iounmap(reg);
	ret = dma_srclen(buf);
	if (ret == 0){
		return 0;
	}
	else {
		pr_err("Failed in mm2s transfer\n");
	
		return ret;
	}
	
}
int s2mm_stransfer(unsigned long data){
	u32 check =DMA_BASE_ADDRESS + DMA_S2MM_CTRL_OFFSET + DMA_REG_DMASR;
	u32 value;
	int ret;
	unsigned long buf = data;
	void __iomem *reg = ioremap(check, 4);
	pr_info("Mapping address in s2mm_stransfer :%x", check);
	if(!reg){
	pr_err("Failed to map status register for s2mm");
	 return -EBUSY;
	}
	value = ioread32(reg);
	pr_info("Verifying does the channel is running or not (last bit 0):%x", value);
	iounmap(reg);
	ret = dma_destlen(buf);
	if (ret == 0){
		return 0;
	}
	else {
		pr_err("Failed in s2mm transfer\n");
		return ret;
	}

}

static ssize_t attr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{   
    unsigned long value;
    int ret;

    if (strcmp(attr->attr.name, "srcaddr") == 0) {
        snprintf(srcaddr, sizeof(srcaddr), "%.*s", (int)count, buf);
        ret = kstrtoul(buf, 0, &value);
        if (ret)
            return ret;
        dma_srcaddr(value);
    } 
    else if (strcmp(attr->attr.name, "dmaon") == 0) {
        snprintf(dmaon, sizeof(dmaon), "%.*s", (int)count, buf);
        ret = kstrtoul(buf, 0, &value);
        if (ret)
            return ret;
        dma_on(value);
    } 
    else if (strcmp(attr->attr.name, "dmaoff") == 0) {
        snprintf(dmaoff, sizeof(dmaoff), "%.*s", (int)count, buf);
        ret = kstrtoul(buf, 0, &value);
        if (ret)
            return ret;
        dma_off(value);
    } 
    else if (strcmp(attr->attr.name, "errcheck") == 0) {
        snprintf(errcheck, sizeof(errcheck), "%.*s", (int)count, buf);
        ret = kstrtoul(buf, 0, &value);
        if (ret)
            return ret;
        error_check(value);
    } 
    else if (strcmp(attr->attr.name, "destaddr") == 0) {
        snprintf(destaddr, sizeof(destaddr), "%.*s", (int)count, buf);
        ret = kstrtoul(buf, 0, &value);
        if (ret)
            return ret;
        dma_destaddr(value);
    } 
    else if (strcmp(attr->attr.name, "srclen") == 0) {
        snprintf(srclen, sizeof(srclen), "%.*s", (int)count, buf);
        ret = kstrtoul(buf, 0, &value);
        if (ret)
            return ret;
        ret = mm2s_stransfer(value);
        msleep(1000);  // Sleep for 1 second to avoid busy-waiting
        if (ret == 0) {	
            poll(DMA_BASE_ADDRESS + DMA_MM2S_CTRL_OFFSET + DMA_REG_DMASR);
            pr_info("MM2S transfer completed");
        } else {
            pr_err("Error detected in MM2S transfer");	
        }
    } 
    else if (strcmp(attr->attr.name, "destlen") == 0) {
        snprintf(destlen, sizeof(destlen), "%.*s", (int)count, buf);
        ret = kstrtoul(buf, 0, &value);
        if (ret)
            return ret;
        ret = s2mm_stransfer(value);
        msleep(1000);  // Sleep for 1 second to avoid busy-waiting	
        if (ret == 0) {	
            poll(DMA_BASE_ADDRESS + DMA_S2MM_CTRL_OFFSET + DMA_REG_DMASR);
            pr_info("S2MM transfer completed");
        } else {
            pr_err("Error detected in S2MM transfer");	
        }
    } 
    else {
        return -EINVAL;
    }

    return count;
}

// Declare the device attributes
static DEVICE_ATTR(errcheck, 0664, attr_show, attr_store);
static DEVICE_ATTR(srcaddr, 0664, attr_show, attr_store);
static DEVICE_ATTR(destaddr, 0664, attr_show, attr_store);
static DEVICE_ATTR(srclen, 0664, attr_show, attr_store);
static DEVICE_ATTR(destlen, 0664, attr_show, attr_store);
static DEVICE_ATTR(dmaon, 0664, attr_show, attr_store);
static DEVICE_ATTR(dmaoff, 0664, attr_show, attr_store);



static int dev_open(struct inode *inode, struct file *file)
{
    pr_info("DMA device opened\n");
    return 0;
}


// Device fileooperations implementation
static ssize_t dev_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset)
{
    // Fixed data buffer with pre-defined contents
    static char temp_buffer[4096];  // static to ensure it retains the same data between reads
    int bytes_to_copy;
    size_t total_data_length;

    // Combine all attributes into a single buffer (this will remain the same unless changed explicitly)
    snprintf(temp_buffer, sizeof(temp_buffer),
             "These are the last written parameters \n Source address: %s\nSource length: %s\nDestination address: %s\nDestination length: %s\nDMA_ON: %s\nDMA_OFF: %s\nERROR_CHECK: %s\n",
             srcaddr, srclen, destaddr, destlen, dmaon, dmaoff, errcheck);

    total_data_length = strlen(temp_buffer);  // Length of the string in temp_buffer

    // Check if we're at the end of the data
    if (*offset >= total_data_length)
        return 0; // EOF (no more data to read)

    // Determine how many bytes to copy to the user space
    bytes_to_copy = min(len, total_data_length - (size_t)*offset);

    // Copy data from kernel space to user space
    if (copy_to_user(user_buffer, temp_buffer + *offset, bytes_to_copy))
        return -EFAULT;

    // Update the offset after the read
    *offset += bytes_to_copy;

    pr_info("Device file read: %d bytes\n", bytes_to_copy);

    return bytes_to_copy;
}

static ssize_t dev_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset)
{   int ret; 
    char temp_buffer[1000];
    char data[1000];
    unsigned long num;
    pr_info("Device file write\n");
    if (len >= sizeof(temp_buffer))
        return -EINVAL;

    // Copy data from user space to kernel space
    if (copy_from_user(temp_buffer, user_buffer, len))
        return -EFAULT;

    temp_buffer[len] = '\0'; // Null-terminate the string

    // Check which parameter userspace sent
    if (sscanf(temp_buffer, "SA: %99s", data) == 1) {
        strncpy(srcaddr, data, sizeof(srcaddr));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for source address register and the data is %s\n", data);
        return -EINVAL;
	}
	dma_srcaddr(num);
    } else if (sscanf(temp_buffer, "SL: %99s", data) == 1) {
        strncpy(srclen, data, sizeof(srclen));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for source length register and the data from user is %s\n", data);
        return -EINVAL;
	}      
	ret = mm2s_stransfer(num);
        msleep(1000);  // Sleep for 1 second to avoid busy-waiting
        
	if (ret == 0) {	
            poll(DMA_BASE_ADDRESS + DMA_MM2S_CTRL_OFFSET + DMA_REG_DMASR);
            pr_info("MM2S transfer completed");
        } else {
            pr_err("Error detected in MM2S transfer");	
        }
    } else if (sscanf(temp_buffer, "DA: %99s", data) == 1) {
        strncpy(destaddr, data, sizeof(destaddr));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for destination address register and the data from user is %s\n", data);
        return -EINVAL;
	}
	dma_destaddr(num);
    } else if (sscanf(temp_buffer, "DL: %99s", data) == 1) {
        strncpy(destlen, data, sizeof(destlen));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for destination length register and the data from user is %s\n", data);
        return -EINVAL;
	}
	ret = s2mm_stransfer(num);
        msleep(1000);  // Sleep for 1 second to avoid busy-waiting
        if (ret == 0) {	
            poll(DMA_BASE_ADDRESS + DMA_MM2S_CTRL_OFFSET + DMA_REG_DMASR);
            pr_info("MM2S transfer completed");
        } else {
            pr_err("Error detected in MM2S transfer");	
        }

    } else if (sscanf(temp_buffer, "DR: %99s", data) == 1) {
        strncpy(dmaon, data, sizeof(dmaon));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for startig DMA and the channel offset data from user is %s\n", data);
        return -EINVAL;
	}
	dma_on(num);
    } else if (sscanf(temp_buffer, "DS: %99s", data) == 1) {
        strncpy(dmaoff, data, sizeof(dmaoff));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for source address register and the data is %s\n", data);
	return -EINVAL;
	}
	dma_off(num);
    } else if (sscanf(temp_buffer, "DE: %99s", data) == 1) {
        strncpy(errcheck, data, sizeof(errcheck));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for source address register and the data is %s\n", data);
        return -EINVAL;
	}
	error_check(num);
    } else {
        pr_err("Invalid input format.'\n");
        return -EINVAL;
    } 		
   return len;
}


static int dev_release(struct inode *inode, struct file *file)
{
        msleep(10);
  	pr_info("Device file closed\n");
            return 0;
}	

// Poll method for the device
static unsigned int dev_poll(struct file *file, struct poll_table_struct *poll_table) {
    unsigned int mask = 0;

    // Add the current task to the wait queue
    poll_wait(file, &my_waitqueue, poll_table);

    // Check if the condition is met
    if (my_condition_met) {
        mask |= POLLIN | POLLRDNORM;  // Data is available for reading
    }

    return mask;
}

int reset(u32 offset) {
    void __iomem *reg_base;
    u32 value;

    reg_base = ioremap(DMA_BASE_ADDRESS + offset , 4);
    if (!reg_base)
        return -ENOMEM;  // Return error if mapping fails

    value = ioread32(reg_base);

    SET_BIT(value, 2);

    iowrite32(value, reg_base);

    if (ioread32(reg_base) != value) {
        iounmap(reg_base);
	pr_info("Error at resetting and address of channel 0x%x", (DMA_BASE_ADDRESS + offset));
        return -EIO;  
    }

    iounmap(reg_base);
    return 0;
}



static int custom_dma_chan_probe(struct custom_dma_device *ddev,
				 struct device_node *node)
{
	struct custom_dma_channel *chan;
	int ret;

	/* Allocate and initialize the channel structure */
	chan = devm_kzalloc(ddev->dev, sizeof(*chan), GFP_KERNEL);
	if (!chan)
		return -ENOMEM;

	chan->idle = true;

	if (of_device_is_compatible(node, "xlnx,axi-dma-mm2s-channel")) {
		chan->ctrl_offset = DMA_MM2S_CTRL_OFFSET;
		ret = reset(DMA_MM2S_CTRL_OFFSET);
		if (ret) { 
			pr_err("MM2S Channel reset failed: %d\n", ret);
			return ret;
		}
	} else if (of_device_is_compatible(node, "xlnx,axi-dma-s2mm-channel")) {
		chan->ctrl_offset = DMA_S2MM_CTRL_OFFSET;
		ret = reset(DMA_S2MM_CTRL_OFFSET);
		if (ret) {
			pr_err("S2MM Channel reset failed: %d\n", ret);
			return ret;
		}
	} else {
		dev_err(ddev->dev, "Invalid channel compatible node\n");
		return -EINVAL;
	}

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

static int custom_dma_probe(struct platform_device *pdev)
{
	struct device_node *child; // Pointer to the node in device tree
	struct custom_dma_device *ddev; // Pointer to custom_dma_device structure
	u32 addr_width;
	struct resource *res;
	int err, ret;
	struct device_node *node = pdev->dev.of_node; // Pointer to the node in device tree

	/* Allocate and initialize the custom DMA device structure */
	ddev = devm_kzalloc(&pdev->dev, sizeof(*ddev), GFP_KERNEL);
	if (!ddev)
		return -ENOMEM;

	ddev->dev = &pdev->dev;

	/* Get DMA address from device tree */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get platform resource\n");
		return -EINVAL;
	}

	ddev->base_address = res->start;
	pr_info("my_driver: AXI DMA physical address start = 0x%x\n", ddev->base_address);

	/* Request and map I/O memory */
	ddev->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ddev->regs))
		return PTR_ERR(ddev->regs);

	pr_info("my_driver: AXI DMA remapped address in probing = 0x%pa\n", &ddev->regs);

	/* Get address width from device tree */
	err = of_property_read_u32(node, "xlnx,addrwidth", &addr_width);
	if (err < 0) {
		addr_width = 32;  // Default value
		dev_warn(ddev->dev, "Missing xlnx,addrwidth property, using default value of %d\n", addr_width);
	}

	/* Set the DMA mask bits */
	if (dma_set_mask(ddev->dev, DMA_BIT_MASK(addr_width))) {
		dev_err(ddev->dev, "Failed to set DMA mask for %d-bit addressing\n", addr_width);
		return -EIO;
	}

	dev_info(ddev->dev, "DMA mask set to %d-bit successfully\n", addr_width);

	/* Store driver data for future */
	platform_set_drvdata(pdev, ddev);

	/* Initialize the channels */
	for_each_child_of_node(node, child) {
		err = custom_dma_child_probe(ddev, child);
		if (err < 0) {
			pr_err("Error at channel probing\n");
			return err;
		}
	}

	/* Register character device */
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0) {
		pr_err("Failed to register a major number\n");
		return major_number;
	}

	/* Create a class in /sys/class/ */
	sysfs_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(sysfs_class)) {
		pr_err("Failed to create class\n");
		unregister_chrdev(major_number, DEVICE_NAME);
		return PTR_ERR(sysfs_class);
	}

	/* Create a device in the class */
	sysfs_device = device_create(sysfs_class, NULL, MKDEV(major_number, 0), NULL, DRIVER_NAME);
	if (IS_ERR(sysfs_device)) {
		pr_err("Failed to create device\n");
		class_destroy(sysfs_class);
		unregister_chrdev(major_number, DEVICE_NAME);
		return PTR_ERR(sysfs_device);
	}

	/* Create the attribute files */
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
	ret = device_create_file(sysfs_device, &dev_attr_errcheck);
	if (ret)
		goto fail_attr7;

	dev_info(&pdev->dev, "AXI DMA Engine Driver Probed!!\n");
	return 0;

	/* Cleanup on failure */
fail_attr7:
	device_remove_file(sysfs_device, &dev_attr_dmaoff);

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
	
	/* Cleanup */
    	device_remove_file(sysfs_device, &dev_attr_srclen);
	device_remove_file(sysfs_device, &dev_attr_destlen);
    	device_remove_file(sysfs_device, &dev_attr_srcaddr);
    	device_remove_file(sysfs_device, &dev_attr_destaddr);
	device_remove_file(sysfs_device, &dev_attr_dmaon);
    	device_remove_file(sysfs_device, &dev_attr_dmaoff);
	device_remove_file(sysfs_device, &dev_attr_errcheck);
	
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



