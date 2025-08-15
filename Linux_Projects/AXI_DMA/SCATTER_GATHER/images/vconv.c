
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
#include <linux/string.h>
#include <linux/memremap.h>

/* Base address */
#define DMA_BASE_ADDRESS	0x80000000	

/* Register/Descriptor Offsets */
#define DMA_MM2S_OFFSET		0x00000000
#define DMA_S2MM_OFFSET		0x00000030

/* Control Registers */
#define DMA_REG_CONTROL		0x00000000

/* Status Registers */
#define DMA_REG_STATUS		0x00000004

/* Descriptor Registers */
#define DMA_REG_CURDES		0x08
#define DMA_REG_TAILDES		0x10

#define DESC_REGION_BASE  0x80010000 
#define DESC_REGION_SIZE (num_descriptors * DESC_SIZE)
/* Descriptor Size */
#define DESC_SIZE		0x40      //descriptor size
#define DESC_ALIGNMENT	 	0x40  // 0x40 bytes


/* IOCTL Commands */
#define MAGIC_NUMBER 'a'
#define BD_WRITE _IOW(MAGIC_NUMBER, 1, char[200])
#define BD_CREATE _IOW(MAGIC_NUMBER, 2, char[200])
#define BD_CHECK _IOR(MAGIC_NUMBER, 3, char[500])

/* Bit Manipulation */
#define SET_BIT(value, bit) ((value) |= (1 << (bit)))
#define CLEAR_BIT(value, bit) ((value) &= ~(1 << (bit)))
#define CHECK_BIT(value, bit) ((value) & (1 << (bit)))

#define DRIVER_NAME "vconv_driver"
#define DEVICE_NAME "DMA_device"
#define BUFFER_SIZE 10240


// Variables for sysfs attributes
static char mm2scur[100] = "0x00000000";
static char s2mmcur[100] = "0x00000000";
static char mm2stail[100] = "0x00000000";
static char s2mmtail[100] = "0x00000000";
static char dmaon[100] = "0x00000000";
static char dmareset[100] = "0x00000000";
static char dmaoff[100] = "0x00000000";
static char errcheck[100] = "0x00000000";
//static void *dma_vaddr_s2mm = NULL;
//static void *dma_vaddr_mm2s = NULL;
static void *aligned_dma_vaddr_mm2s = NULL;
static void *aligned_dma_vaddr_s2mm = NULL;
//static dma_addr_t dma_paddr_mm2s;
//static dma_addr_t dma_paddr_s2mm;
static size_t s2mm_bd_size;
static size_t mm2s_bd_size;
static uintptr_t mm2s_cbd;
static uintptr_t mm2s_tbd;
static uintptr_t s2mm_cbd;
static uintptr_t s2mm_tbd;
static int num_descriptors;
static int option = 0;
static struct device *dma_device;
static char temp_buffer[500] = {0};
static DECLARE_WAIT_QUEUE_HEAD(my_waitqueue);  
static bool transfer_success = false;         
static bool transfer_failed = false;         
static struct class *sysfs_class;
static struct device *sysfs_device;

static int major_number;

/* Descriptor fields */
struct descriptor {
	uint32_t nxtdesc;
	uint32_t nxtdesc_msb;
	uint32_t buffer_address;
	uint32_t buffer_address_msb;
	uint32_t reserved[2];
	uint32_t control;
	uint32_t status;
	uint32_t app[5];
};

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

static void __iomem *dma_vaddr_s2mm = NULL;
static void __iomem *dma_vaddr_mm2s = NULL;
static dma_addr_t dma_paddr_s2mm = DESC_REGION_BASE + 0xB000;
static dma_addr_t dma_paddr_mm2s = DESC_REGION_BASE;

int s2mm_bd_creation(void) {
    size_t i;

    if (!num_descriptors)
        return -EINVAL;

    s2mm_bd_size = DESC_REGION_SIZE;

    if (dma_vaddr_s2mm) {
        memunmap(dma_vaddr_s2mm);
        dma_vaddr_s2mm = NULL;
        pr_info("Freed mapped memory for S2MM channel before reallocation");
    }

    dma_vaddr_s2mm = memremap(dma_paddr_s2mm, s2mm_bd_size, MEMREMAP_WT);
    if (!dma_vaddr_s2mm) {
        pr_err("memremap failed for S2MM\n");
        return -ENOMEM;
    }
    aligned_dma_vaddr_s2mm = (void *)(uintptr_t)dma_vaddr_s2mm;

    for (i = 0; i < num_descriptors; i++) {
	struct descriptor *desc = (struct descriptor *)((uintptr_t)aligned_dma_vaddr_s2mm + i * DESC_SIZE);
        desc->nxtdesc = (i == num_descriptors - 1) ? 0 : dma_paddr_s2mm + (i + 1) * DESC_SIZE;
        desc->nxtdesc_msb = 0;
        desc->buffer_address = 0;
        desc->buffer_address_msb = 0;
        memset(desc->reserved, 0, sizeof(desc->reserved));
        desc->control = 0;
        desc->status = 0;
        memset(desc->app, 0, sizeof(desc->app));
    }

    s2mm_cbd = (uintptr_t)dma_paddr_s2mm;
    s2mm_tbd = (uintptr_t)dma_paddr_s2mm + (i - 1) * DESC_SIZE;

    pr_info("S2MM Descriptor Start: 0x%lX; Tail: 0x%lX\n", s2mm_cbd, s2mm_tbd);
    num_descriptors = 0;
    return 0;
}

int mm2s_bd_creation(void) {
    size_t i;
    if (!num_descriptors)
        return -EINVAL;

    mm2s_bd_size = DESC_REGION_SIZE;

    if (dma_vaddr_mm2s) {
        memunmap(dma_vaddr_mm2s);
        dma_vaddr_mm2s = NULL;
        pr_info("Freed mapped memory for MM2S channel before reallocation");
    }

    dma_vaddr_mm2s = memremap(dma_paddr_mm2s, mm2s_bd_size, MEMREMAP_WT);
    if (!dma_vaddr_mm2s) {
        pr_err("memremap failed for MM2S\n");
        return -ENOMEM;
    }
    aligned_dma_vaddr_mm2s = (void *)(uintptr_t)dma_vaddr_mm2s;
    for (i = 0; i < num_descriptors; i++) {
        struct descriptor *desc = (struct descriptor *)((uintptr_t)aligned_dma_vaddr_mm2s + i * DESC_SIZE);
        desc->nxtdesc = (i == num_descriptors - 1) ? 0 : dma_paddr_mm2s + (i + 1) * DESC_SIZE;
        desc->nxtdesc_msb = 0;
        desc->buffer_address = 0;
        desc->buffer_address_msb = 0;
        memset(desc->reserved, 0, sizeof(desc->reserved));
        desc->control = (i == 0) ? (1 << 27) : 0;  
        if (i == num_descriptors - 1)
            desc->control |= (1 << 26);  
        desc->status = 0;
        memset(desc->app, 0, sizeof(desc->app));
    }

    mm2s_cbd = (uintptr_t)dma_paddr_mm2s;
    mm2s_tbd = (uintptr_t)dma_paddr_mm2s + (i - 1) * DESC_SIZE;
    pr_info("MM2S Descriptor Start: 0x%lX; Tail: 0x%lX\n", mm2s_cbd, mm2s_tbd);
    num_descriptors = 0;
    return 0;
}
/*
 int s2mm_bd_creation(void){
     struct descriptor *desc;
     size_t i;
     static dma_addr_t aligned_dma_paddr;;
     struct device *dev = dma_device;
     if (!num_descriptors)
        return -EINVAL;

     s2mm_bd_size = num_descriptors * DESC_SIZE + DESC_ALIGNMENT;

  	if (dma_vaddr_s2mm){
       		dma_free_coherent(dev, s2mm_bd_size, dma_vaddr_s2mm, dma_paddr_s2mm);
		dma_vaddr_s2mm = NULL;
        pr_info("Freed dma descriptor memory of s2mm channel, before allocating");
	}
     dma_vaddr_s2mm= dma_alloc_coherent(dev, s2mm_bd_size, &dma_paddr_s2mm, GFP_DMA32);
     pr_info("Aligned Physical Address for descriptor creation is 0x%llX;", dma_paddr_s2mm);
     if (dma_vaddr_s2mm == NULL) {
            pr_info("DMA Allocation Failed for s2mm, Virtual Address: 0X%p, Physical Address: 0X%llX\n",
            dma_vaddr_s2mm, (unsigned long long)dma_paddr_s2mm);
        pr_err("dma_alloc_coherent failed\n");
        return -ENOMEM;
     }
    // descriptor alignment 
    aligned_dma_paddr = ALIGN(dma_paddr_s2mm, DESC_ALIGNMENT);
    aligned_dma_vaddr_s2mm = (void *)ALIGN((uintptr_t)dma_vaddr_s2mm, DESC_ALIGNMENT);
    // Initialize descriptors
    desc = (struct descriptor *)aligned_dma_vaddr_s2mm;
    for (i = 0; i < num_descriptors; i++) {
        desc->nxtdesc = (i == num_descriptors - 1) ? 0 : aligned_dma_paddr + (i + 1) * DESC_SIZE;
	desc->nxtdesc_msb = 0;
	desc->buffer_address = 0;
	desc->buffer_address_msb = 0;
        memset(desc->reserved, 0, sizeof(desc->reserved));
	desc->control = 0;
        // Set Start of Frame (SOF) and End of Frame (EOF) bits
        desc->control = 0; // SOF for first descriptor
	desc->status = 0;
        memset(desc->app, 0, sizeof(desc->app));
    }
	s2mm_cbd = (uintptr_t)aligned_dma_paddr;
	s2mm_tbd = (uintptr_t)aligned_dma_paddr + (i - 1) * DESC_SIZE;
        pr_info("Address of Current descriptor in s2mm 0x%lX;", s2mm_cbd);
	pr_info("Address of Tail descriptor in s2mm 0x%lX;", s2mm_tbd);
    return 0;
}

 int mm2s_bd_creation(void){
    struct descriptor *desc;
    size_t i;
    static dma_addr_t aligned_dma_paddr;;
    struct device *dev = dma_device;
    if (!num_descriptors)
        return -EINVAL;

    mm2s_bd_size = num_descriptors * DESC_SIZE + DESC_ALIGNMENT;

  	if (dma_vaddr_mm2s){
       		dma_free_coherent(dev, mm2s_bd_size, dma_vaddr_mm2s, dma_paddr_mm2s);
		dma_vaddr_mm2s = NULL;
        pr_info("Freed dma descriptor memory of mm2s channel, before allocating");
	}
     dma_vaddr_mm2s= dma_alloc_coherent(dev, mm2s_bd_size, &dma_paddr_mm2s, GFP_DMA32);
     pr_info("Aligned Physical Address for descriptor creation is 0x%llX;", dma_paddr_mm2s);
     if (dma_vaddr_mm2s == NULL) {
            pr_info("DMA Allocation Failed for mm2s, Virtual Address: 0X%p, Physical Address: 0X%llX\n",
            dma_vaddr_mm2s, (unsigned long long)dma_paddr_mm2s);
        pr_err("dma_alloc_coherent failed\n");
        return -ENOMEM;
     }
    // descriptor alignment 
    aligned_dma_paddr = ALIGN(dma_paddr_mm2s, DESC_ALIGNMENT);
    aligned_dma_vaddr_mm2s = (void *)ALIGN((uintptr_t)dma_vaddr_mm2s, DESC_ALIGNMENT);
    // Initialize descriptors
    desc = (struct descriptor *)aligned_dma_vaddr_mm2s;
    for (i = 0; i < num_descriptors; i++) {
        desc->nxtdesc = (i == num_descriptors - 1) ? 0 : aligned_dma_paddr + (i + 1) * DESC_SIZE;
	desc->nxtdesc_msb = 0;
	desc->buffer_address = 0;
	desc->buffer_address_msb = 0;
        memset(desc->reserved, 0, sizeof(desc->reserved));
	desc->control = 0;
        // Set Start of Frame (SOF) and End of Frame (EOF) bits
        desc->control = (i == 0) ? (1 << 27) : 0; // SOF for first descriptor
	if (i == num_descriptors - 1) {
	    desc->control |= (1 << 26); // EOF bit
	}
	desc->status = 0;
        memset(desc->app, 0, sizeof(desc->app));
    }
	mm2s_cbd = (uintptr_t)aligned_dma_paddr;
	mm2s_tbd = (uintptr_t)aligned_dma_paddr + (i - 1) * DESC_SIZE;
        pr_info("Address of Current descriptor in mm2s 0x%lX;", mm2s_cbd);
	pr_info("Address of Tail descriptor in mm2s 0x%lX;", mm2s_tbd);
    return 0;
}
*/
char* read_back_buffer_descriptor(int option) {
    struct descriptor *read;
    static char temp_buffer_1[500];

    if (option == 5) {
          if (!aligned_dma_vaddr_mm2s) {
	    snprintf(temp_buffer_1, sizeof(temp_buffer_1), "Error: MM2S descriptor memory not allocated.\n");
            return temp_buffer_1;
          }
        read = (struct descriptor *)(aligned_dma_vaddr_mm2s + (num_descriptors - 1) * DESC_SIZE);
    } else if (option == 6) {
          if (!aligned_dma_vaddr_s2mm) {
	    snprintf(temp_buffer_1, sizeof(temp_buffer_1), "Error: S2MM descriptor memory not allocated.\n");
            return temp_buffer_1;
          }
        read = (struct descriptor *)(aligned_dma_vaddr_s2mm + (num_descriptors - 1) * DESC_SIZE);
    } else {
	 snprintf(temp_buffer_1, sizeof(temp_buffer_1), "Error: Invalid parameter in descriptor reading.\n");
         return temp_buffer_1;
    }

    snprintf(temp_buffer_1, sizeof(temp_buffer_1),
             "Buffer descriptor details\n"
             "Next-Descriptor-Address: 0x%X\n"
             "Next-Descriptor-Address-Msb: 0x%X\n"
             "Buffer-address: 0x%X\n"
             "Buffer-address-Msb: 0x%X\n"
             "Reserved-1: 0x%X\n"
             "Reserved-2: 0x%X\n"
             "Control-register: 0x%X\n"
             "Status-register: 0x%X\n"
             "App-1: 0x%X\n"
             "App-2: 0x%X\n"
             "App-3: 0x%X\n"
             "App-4: 0x%X\n"
             "App-5: 0x%X\n",
             read->nxtdesc, read->nxtdesc_msb,
             read->buffer_address, read->buffer_address_msb,
             read->reserved[0], read->reserved[1], read->control, read->status,
             read->app[0], read->app[1], read->app[2], read->app[3], read->app[4]);

    return temp_buffer_1;
}

int buffer_address_writing_into_buffer_register_in_descriptor(u32 buffer_addr, void* descriptor_address, uint32_t buffer_length){
    uint32_t mask = 0x03FFFFFF;
    uint32_t value;
    struct descriptor *write;
    uint32_t length = buffer_length;
    write = ((struct descriptor *)(descriptor_address));
    write->buffer_address = buffer_addr;
    value = write->buffer_address;
    if (value != buffer_addr) {
        pr_err("Error at writing buffer address to descriptor buffer register\n");
        return -EIO;  
    }
    value = write->control;
    value = (value &~ mask) | (length & mask);
    write->control = value;
    value = write->control;
    value = (value & mask);
    length = (buffer_length & mask);
    if(value != length) {
        pr_err("Error at writing buffer length to descriptor control register\nBuffer_length in the register 0x%X \n", value);
        return -EIO;  

    }
    pr_info("Successfully written Buffer address and Buffer Length into descriptor\n"); 
    return 0;
}

void descriptor_checking(unsigned long num_bd, unsigned long buffer_address, unsigned long buffer_length, uint32_t choice ){
    uint32_t desc_addr = ((num_bd - 1) * DESC_SIZE);
    void* virt_desc_addr;

    if (choice == 3) {
	if (aligned_dma_vaddr_mm2s){ 
		virt_desc_addr = (void *)((char *)aligned_dma_vaddr_mm2s + desc_addr);}
	else{
		pr_err("ERROR! There is No Buffer Descriptor to Write\n");
		return;}
    } else if (choice == 4) {
	if (aligned_dma_vaddr_s2mm) {
        	virt_desc_addr = (void*)((char*)aligned_dma_vaddr_s2mm + desc_addr);}
	else{
		pr_err("ERROR! There is No Buffer Descriptor to Write\n");
		return;}
    } else {
        pr_err("ERROR! INVALID PASS NUMBER IN DESCRIPTOR CHECKING\n");
        return;
    }
    buffer_address_writing_into_buffer_register_in_descriptor(buffer_address, virt_desc_addr, buffer_length);
}

static int dev_open(struct inode *inode, struct file *file);
static ssize_t dev_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset);
static ssize_t dev_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset);
static int dev_release(struct inode *inode, struct file *file);
static unsigned int dev_poll(struct file *file, struct poll_table_struct *poll_table);
static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
// Define file operations structure 
static struct file_operations fops = {
    .open	 	= dev_open,
    .read		= dev_read,
    .write	 	= dev_write,
    .release	 	= dev_release,
    .poll	 	= dev_poll,
    .unlocked_ioctl     = dev_ioctl,
};

static ssize_t attr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (strcmp(attr->attr.name, "mm2scur") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", mm2scur);
    else if (strcmp(attr->attr.name, "errcheck") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", errcheck);
    else if (strcmp(attr->attr.name, "dmaon") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", dmaon);
    else if (strcmp(attr->attr.name, "dmaoff") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", dmaoff);
    else if (strcmp(attr->attr.name, "s2mmcur") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", s2mmcur);
    else if (strcmp(attr->attr.name, "mm2stail") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", mm2stail);
    else if (strcmp(attr->attr.name, "s2mmtail") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", s2mmtail);

    return -EINVAL;
}

 int dma_write(void __iomem *reg, u32 value, char *array)
{
 	u32 dummy;
	char *string = array;
	pr_info("Value writing:- 0x%X\n", value);
	iowrite32(value, reg);
	dummy =	ioread32(reg);// Write a 32-bit value to the register
	pr_info("Value writing:- 0x%X\n", dummy);
	if (dummy == value) {
        	pr_info("%s-success and value in register is 0x%x \n",string, dummy);
		iounmap(reg);
		return 0;
    	} else {
        	pr_err("%s-failed and value in register is 0x%x\n",string, dummy);
		iounmap(reg);
		return -EIO; // Input/Output error
    	}  

}

/*unsigned long dma_read(void __iomem *reg) {
    if (!reg) {
        pr_err("Invalid register base address\n");
        return 0;  
    }
    return ioread32(reg);  // Read a 32-bit value from the register
}*/

int reset(u32 offset) {
    void __iomem *reg_base;
    u32 value;

    reg_base = ioremap(DMA_BASE_ADDRESS + offset , 4);
    if (!reg_base)
        return -ENOMEM;  // Return error if mapping fails

    value = ioread32(reg_base);
    SET_BIT(value, 2);
    iowrite32(value, reg_base);
/*
    if (ioread32(reg_base) != value) {
        iounmap(reg_base);
	pr_info("Error at resetting and address of channel 0x%x", (DMA_BASE_ADDRESS + offset));
        return -EIO;  
    }
*/
    iounmap(reg_base);
    return 0;
}

void dma_on(unsigned long offset){
	
    	u32 value;
	u32 run = DMA_BASE_ADDRESS + offset ;
	char *description = "DMA-ON-BIT";
  	void __iomem *reg = ioremap(run, 4);
  	  if (!reg) {
   	     pr_err("Failed to map DMA register for DMA_ON\n");
  	      return;
 	  }
        value = ioread32(reg);
	SET_BIT(value, 0);

   	dma_write(reg, value, description);
	return;
}

void dma_off(unsigned long offset){

    	u32 value;
	u32 run = DMA_BASE_ADDRESS + offset;
	char *description = " DMA-OFF-BIT";	

    void __iomem *reg = ioremap(run, 4);
    if (!reg) {
        pr_err("Failed to map DMA register for run\n");
        return;
    }
        value = ioread32(reg);
	CLEAR_BIT(value, 0);


    dma_write(reg, value, description);
    return;
}

void error_check(u32 channel_address){

    u32 error = DMA_BASE_ADDRESS + channel_address + DMA_REG_STATUS;
    u32 value;
    

    void __iomem *reg = ioremap(error, 4);
    if (!reg) {
        pr_err("Failed to map status register in error check");
        return;
    } 
    value = ioread32(reg);
    
    if (value & (BIT(14))) {
	    transfer_failed = true;
            wake_up_interruptible(&my_waitqueue);
        // Check if bit 6 is set (indicating an DMA buffer address decode error)
        /*if (CHECK_BIT(value, 6)) {
            pr_err("DMADecErr : Invalid address detected (bit 6 set)\n");
        }
        // Check if bit 5 is set (indicating an DMA slave error, may be from ddr side)
        if (CHECK_BIT(value, 5)) {
            pr_err("DMASlvErr: Slave device issue detected (bit 5 set)\n");
        }
        // Check if bit 4 is set (indicating an DMA buffer length error)
        if (CHECK_BIT(value, 4)) {
            pr_err("DMAIntErr: Invalid buffer length detected (bit 4 set)\n");
        } 
        if (CHECK_BIT(value, 8)) {
            pr_err("SGIntErr: Invalid Buffer descriptor, Complete bit already set is fetched (bit 8 set)\n");
        }
        if (CHECK_BIT(value, 9)) {
            pr_err("SGSlvErr: Slave device issue detected (bit 9 set)\n");
        }
        if (CHECK_BIT(value, 10)) {
            pr_err("SGDecErr: CURDESC_PTR and/or NXTDESC_PTR points to an invalid address. (bit 10 set)\n");
        }*/
	goto err;
    }
    transfer_success = true;
    wake_up_interruptible(&my_waitqueue);
err:    
    iounmap(reg);
    return;

}

int poll(u32 channel_address) {
    u32 check = channel_address;//address of channel status registers
    u32 value;
    int timeout = 10000;  
    char *description = " DMA INTERRUPT BIT CLEARING ";
    void __iomem *reg = ioremap(check, 4);  
    pr_info("Address mapping in polling :0x%x", check);
    if (!reg) {
        pr_err("Failed to map status register in poll function\n");
        return -EIO;
    }
    // Polling loop with timeout

    while (timeout-- > 0)
    {
        value = ioread32(reg);  
        if (CHECK_BIT(value, 12))
	 {  
            pr_info("12th bit is set, Interrupt generated on completion of transfer!\n");
	    transfer_success = true;
            wake_up_interruptible(&my_waitqueue);
	    /* clearing interrupt bit after transfer for reusing the dma*/
	    SET_BIT(value, 12); 
	    check = value;
	    iowrite32(value, reg);
	    value = ioread32(reg);
	    if (value != check) {
        	pr_info("%s-success and value on register is 0x%x \n",description, value);
		iounmap(reg);
		return 0;
    	    } else {
        	pr_err("%s-failed and value on register is 0x%x\n",description, value);
		iounmap(reg);
		return -EIO; 
	    }
         }
       // msleep(1);  // Sleep for 1 ms to avoid busy-waiting
    }
    if (timeout <= 0) {
        pr_err("Timeout reached while polling for the idle bit, Checking errors\n");
	iounmap(reg); 
	error_check(channel_address - (DMA_REG_STATUS + DMA_BASE_ADDRESS)); // Error check accept channel offset only.
        return -EIO;
    }
}


void dma_mm2scur(unsigned long buf){
	pr_info("WRITTEN INTO MM2S CURRENT DESCRIPTOR REGISTER AND DATA WRITTEN :0x%lX\n", buf);
	u32 mm2s_cur = DMA_BASE_ADDRESS + DMA_MM2S_OFFSET + DMA_REG_CURDES;
	char *description = " DMA MM2S CURRENT DECRIPTOR ADDRESS WRITING ";    

    void __iomem *reg_base = ioremap(mm2s_cur, 4);
    pr_info("Data from userspace : 0x%lX", buf);
    pr_info("Mapping address in dma_mm2scur function :0x%X", mm2s_cur);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return;
   }
    dma_write(reg_base, buf, description);
}
void dma_s2mmcur(unsigned long buf){
	pr_info("WRITTEN INTO S2MM CURRENT DESCRIPTOR REGISTER AND DATA WRITTEN :0x%lX\n", buf);
    u32 s2mm_cur =DMA_BASE_ADDRESS + DMA_S2MM_OFFSET + DMA_REG_CURDES;
    char *description = " DMA S2MM CURRENT DECRIPTOR ADDRESS WRITING ";        

    void __iomem *reg_base = ioremap(s2mm_cur, 4);
    pr_info("Data from userspace : 0x%lX", buf);
    pr_info("Mapping address in dma_s2mmcur function :0x%X", s2mm_cur);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return;
    }

    dma_write(reg_base, buf, description);

}
int dma_mm2stail(unsigned long data){
    pr_info("WRITTEN INTO MM2S TAIL DESCRIPTOR REGISTER AND DATA WRITTEN :0x%lX\n", data);
    int ret;
    u32 mm2s_tail = DMA_BASE_ADDRESS + DMA_MM2S_OFFSET + DMA_REG_TAILDES;
    unsigned long buf = data;
    char *description = " DMA MM2S TAIL DESCRIPTOR WRITING ";    
	
    void __iomem *reg_base = ioremap(mm2s_tail, 4);
    pr_info("Mapping address in dma_mm2stail function :%x", mm2s_tail);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return -ENOMEM;
    }
    ret = dma_write(reg_base, buf, description);
    return ret;

}
int dma_s2mmtail(unsigned long data){
    int ret;
    u32 s2mm_tail = DMA_BASE_ADDRESS + DMA_S2MM_OFFSET + DMA_REG_TAILDES ;
    unsigned long buf = data;
    char *description = "DMA S2MM TAIL DESCRIPTOR WRITING ";    	

    void __iomem *reg_base = ioremap(s2mm_tail, 4);
    if (!reg_base) {
        pr_err("Failed to map DMA register\n");
        return -EIO;
    }
    ret = dma_write(reg_base, buf, description);
    return ret;

}



int mm2s_stransfer(unsigned long data){
	unsigned long buf = data;
	int ret;
/*	u32 check = DMA_BASE_ADDRESS + DMA_MM2S_OFFSET + DMA_REG_STATUS;
	u32 value;
	unsigned long buf = data;
	void __iomem *reg = ioremap(check, 4);
	pr_info("Mapping address in s2mm_stransfer :%x", check);	
	if(!reg){
	pr_err("Failed to map status register for mm2s");
	 return -1;
	}
	value = ioread32(reg);
        Checking onceagain whether the channel is running or not 
	if(!CHECK_BIT(value, 0)){
		iounmap(reg);
		pr_info("Error Channel was not running:%x", value);
		return -1;
	}
	iounmap(reg);*/
	ret = dma_mm2stail(buf);
	return ret;
}
int s2mm_stransfer(unsigned long data){
	unsigned long buf = data;
	int ret;
/*	u32 check =DMA_BASE_ADDRESS + DMA_S2MM_OFFSET + DMA_REG_STATUS;
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
	if(!CHECK_BIT(value, 0)){
		pr_info("Error Channel was not running:%x", value);
		iounmap(reg);
		return -1;
	}
	iounmap(reg);*/
	ret = dma_s2mmtail(buf);
	return ret;
}

static ssize_t attr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{   
    unsigned long value;
    int ret;

    if (strcmp(attr->attr.name, "mm2scur") == 0) {
        snprintf(mm2scur, sizeof(mm2scur), "%.*s", (int)count, buf);
        ret = kstrtoul(buf, 0, &value);
        if (ret)
            return ret;
        dma_mm2scur(value);
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
        error_check(value);// user providing channel offset
    } 
    else if (strcmp(attr->attr.name, "s2mmcur") == 0) {
        snprintf(s2mmcur, sizeof(s2mmcur), "%.*s", (int)count, buf);
        ret = kstrtoul(buf, 0, &value);
        if (ret)
            return ret;
        dma_s2mmcur(value);
    } 
    else if (strcmp(attr->attr.name, "mm2stail") == 0) {
        snprintf(mm2stail, sizeof(mm2stail), "%.*s", (int)count, buf);
        ret = kstrtoul(buf, 0, &value);
        if (ret)
            return ret;
        ret = mm2s_stransfer(value);
        msleep(100);  // Sleep for 1 second to avoid busy-waiting
        if (ret == 0) {	
            poll(DMA_BASE_ADDRESS + DMA_MM2S_OFFSET + DMA_REG_STATUS);
        } else {
            pr_err("Error detected in MM2S_transfer function \n");	
        }
    } 
    else if (strcmp(attr->attr.name, "s2mmtail") == 0) {
        snprintf(s2mmtail, sizeof(s2mmtail), "%.*s", (int)count, buf);
        ret = kstrtoul(buf, 0, &value);
        if (ret)
            return ret;
        ret = s2mm_stransfer(value);
        msleep(100);  	
        if (ret == 0) {	
            poll(DMA_BASE_ADDRESS + DMA_S2MM_OFFSET + DMA_REG_STATUS);
        } else {
            pr_err("Error detected in S2MM_transfer function\n");	
        }
    } 
    else {
        return -EINVAL;
    }

    return count;
}

// Declare the device attributes
static DEVICE_ATTR(errcheck, 0664, attr_show, attr_store);
static DEVICE_ATTR(mm2scur, 0664, attr_show, attr_store);
static DEVICE_ATTR(s2mmcur, 0664, attr_show, attr_store);
static DEVICE_ATTR(mm2stail, 0664, attr_show, attr_store);
static DEVICE_ATTR(s2mmtail, 0664, attr_show, attr_store);
static DEVICE_ATTR(dmaon, 0664, attr_show, attr_store);
static DEVICE_ATTR(dmaoff, 0664, attr_show, attr_store);

static int dev_open(struct inode *inode, struct file *file)
{
    pr_info("DMA device opened\n");
    return 0;
}

/* Device file operations implementation */
static ssize_t dev_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset)
{
    static char temp_buffer[4096];  // Static buffer to retain data across reads
    int bytes_to_copy;
    size_t total_data_length;
    void __iomem *reg_base;

    reg_base = ioremap(DMA_BASE_ADDRESS, 0x60);
    if (!reg_base) {
        pr_err("Failed to map I/O memory\n");
        return -ENOMEM;
    }

    // Format the output buffer safely
    total_data_length = snprintf(temp_buffer, sizeof(temp_buffer),
             "These are last created buffer descriptor addresses\n"
             "MM2S Current Descriptor: 0x%lX\nMM2S Tail Descriptor: 0x%lX\n"
             "S2MM Current Descriptor: 0x%lX\nS2MM Tail Descriptor: 0x%lX\n"
             "These are the last written parameters from userspace\n"
             "MM2S Current Descriptor: %s\nMM2S Tail Descriptor: %s\n"
             "S2MM Current Descriptor: %s\nS2MM Tail Descriptor: %s\n"
             "DMA_ON: %s\nDMA_OFF: %s\nERROR_CHECK: %s\n"
             "These are values in the DMA registers\n"
             "MM2S Channel Registers\n"
             "Control Register: 0x%X\nStatus Register: 0x%X\n"
             "Current Descriptor: 0x%X\nCurrent Descriptor MSB: 0x%X\n"
             "Tail Descriptor: 0x%X\nTail Descriptor MSB: 0x%X\n"
             "S2MM Channel Registers\n"
             "Control Register: 0x%X\nStatus Register: 0x%X\n"
             "Current Descriptor: 0x%X\nCurrent Descriptor MSB: 0x%X\n"
             "Tail Descriptor: 0x%X\nTail Descriptor MSB: 0x%X\n",
             mm2s_cbd, mm2s_tbd, s2mm_cbd, s2mm_tbd,
             mm2scur, mm2stail, s2mmcur, s2mmtail,
             dmaon, dmaoff, errcheck,
             (unsigned)ioread32(reg_base), (unsigned)ioread32(reg_base + 0x4),
             (unsigned)ioread32(reg_base + 0x8), (unsigned)ioread32(reg_base + 0xC),
             (unsigned)ioread32(reg_base + 0x10), (unsigned)ioread32(reg_base + 0x14),
             (unsigned)ioread32(reg_base + 0x30), (unsigned)ioread32(reg_base + 0x34),
             (unsigned)ioread32(reg_base + 0x38), (unsigned)ioread32(reg_base + 0x3C),
             (unsigned)ioread32(reg_base + 0x40), (unsigned)ioread32(reg_base + 0x44));

    iounmap(reg_base);

    // Ensure we don't exceed buffer size
    if (total_data_length >= sizeof(temp_buffer))
        total_data_length = sizeof(temp_buffer) - 1;

    // End-of-file condition
    if (*offset >= total_data_length)
        return 0;

    // Determine how many bytes to copy
    bytes_to_copy = min(len, total_data_length - (size_t)*offset);

    // Copy data to user space safely
    if (copy_to_user(user_buffer, temp_buffer + *offset, bytes_to_copy))
        return -EFAULT;

    // Update offset
    *offset += bytes_to_copy;

    pr_info("Device file read: %d bytes\n", bytes_to_copy);

    return bytes_to_copy;
}
static ssize_t dev_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset)
{   int ret; 
    char temp_buffer[1000];
    char data[100];
    unsigned long num;
    unsigned long num1;
    pr_info("Device file write\n");
    if (len >= sizeof(temp_buffer))
        return -EINVAL;

    if (copy_from_user(temp_buffer, user_buffer, len))
        return -EFAULT;

    temp_buffer[len] = '\0';
 
    if (sscanf(temp_buffer, "SCR: %99s", data) == 1) {
        strncpy(mm2scur, data, sizeof(mm2scur));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for source address register and the data is %s\n", data);
        return -EINVAL;
	}
	dma_mm2scur(num);
    } else if (sscanf(temp_buffer, "STL: %99s", data) == 1) {
	pr_info("Received STL: %s %s\n", data, data1);
        strncpy(mm2stail, data, sizeof(mm2stail));
       ret = kstrtoul(data, 0, &num);
        if (ret){
	pr_info("Error at converting string to integer for source length register and the data from user is %s\n", data);
        return -EINVAL;
	}        
	ret = mm2s_stransfer(num);
        msleep(100);  // Sleep for 1 second to avoid busy-waiting 
	if (ret == 0) {	
            poll(DMA_BASE_ADDRESS + DMA_MM2S_OFFSET + DMA_REG_STATUS);
        } else {
            pr_err("Error detected in MM2S transfer\n");	
	    return -EINVAL;	
        }
    } else if (sscanf(temp_buffer, "DCR:%99s", data) == 1) {
        strncpy(s2mmcur, data, sizeof(s2mmcur));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for destination address register and the data from user is %s\n", data);
        return -EINVAL;
	}
	dma_s2mmcur(num);
    } else if (sscanf(temp_buffer, "DTL: %99s", data) == 1) {
        strncpy(s2mmtail, data, sizeof(s2mmtail));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for destination length register and the data from user is %s\n", data);
        return -EINVAL;
	}
	ret = s2mm_stransfer(num);
        msleep(100);  // Sleep for 1 second to avoid busy-waiting
        if (ret == 0) {	
            poll(DMA_BASE_ADDRESS + DMA_S2MM_OFFSET + DMA_REG_STATUS);
        } else {
            pr_err("Error detected in MM2S transfer\n");	
        }

    } else if (sscanf(temp_buffer, "DMARUN: %99s", data) == 1) {
        strncpy(dmaon, data, sizeof(dmaon));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for startig DMA and the channel offset data from user is %s\n", data);
        return -EINVAL;
	}
	dma_on(num);
    } else if (sscanf(temp_buffer, "DMARESET: %99s", data) == 1) {
        strncpy(dmareset, data, sizeof(dmareset));
        ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for startig DMA and the channel offset data from user is %s\n", data);
        return -EINVAL;
	}
	reset(num);
    }  else if (sscanf(temp_buffer, "DMASTOP: %99s", data) == 1) {
        strncpy(dmaoff, data, sizeof(dmaoff));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for source address register and the data is %s\n", data);
	return -EINVAL;
	}
	dma_off(num);
    } else if (sscanf(temp_buffer, "DMAERROR: %99s", data) == 1) {
        strncpy(errcheck, data, sizeof(errcheck));
       ret = kstrtoul(data, 0, &num);// string to number 
        if (ret){
	pr_info("Error at converting string to integer for source address register and the data is %s\n", data);
        return -EINVAL;
	}
	error_check(num);
    } else {
        pr_err("Invalid input format.\n");
        return -EINVAL;
    } 		
   return len;
}
/* For Buffer Descriptor Sector */
static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{   int ret; 
    char data[64] = {0};
    char data1[64] = {0};
    char data2[64] = {0};
    char data3[64] = {0};
    unsigned long num;
    unsigned long num1;
    unsigned long num2;
    unsigned long num3;
         switch(cmd) {
                case BD_CREATE:
                        if( copy_from_user(temp_buffer ,(char*) arg, sizeof(temp_buffer)) )
                        {
                                pr_err("Data Write : Err!\n");
                        }
			if (sscanf(temp_buffer, "BDC: %99s %99s", data, data1) == 2) {
		        	ret = kstrtoul(data, 0, &num);// string to number 
				if (ret){
					pr_err("Error at converting string to integer-1 %s\n", data);
 		     	  		return -EINVAL;
				}
 		     	  num_descriptors = num ;
 		     	  ret = kstrtoul(data1, 0, &num1);
 		     		if (ret){
					   pr_err("Error at converting string to integer-2 %s\n", data1);
 		    			   return -EINVAL;
				}
				if (num1 == 1){
					mm2s_bd_creation();
				}else {
					s2mm_bd_creation();
				}
			};
                        break;
                case BD_WRITE:
                        if( copy_from_user(temp_buffer ,(char*) arg, sizeof(temp_buffer)) )
                         {
                                pr_err("Data Write : Err!\n");
                         }
		 	if (sscanf(temp_buffer, "BDW: %99s %99s %99s %99s", data, data1, data2, data3) == 4) {
			        ret = kstrtoul(data, 0, &num);// string to number 
			        if (ret){
					pr_info("Error at converting string to integer %s\n", data);
				        return -EINVAL;
				}
		          ret = kstrtoul(data1, 0, &num1); 
		          if (ret){
				pr_info("Error at converting string to integer %s\n", data1);
 			       return -EINVAL;
		          }
  		          ret = kstrtoul(data2, 0, &num2); 
		          if (ret){
				pr_info("Error at converting string to integer %s\n", data2);
 			       return -EINVAL;
			  }
  		          ret = kstrtoul(data3, 0, &num3); 
		          if (ret){
				pr_info("Error at converting string to integer %s\n", data3);
 			       return -EINVAL;
			  }

			  descriptor_checking(num, num1, num2, num3);
		        }else {
			        pr_err("Invalid input format.'\n");
 			       return -EINVAL;
  			}
			break;
	       case BD_CHECK:
                        if( copy_from_user(temp_buffer ,(char*) arg, sizeof(temp_buffer)) )
                         {
                                pr_err("Data Write : Err!\n");
                         }
		 	if (sscanf(temp_buffer, "BDR: %99s %99s ", data, data1) == 2) {
			        ret = kstrtoul(data, 0, &num);// string to number 
			        if (ret){
					pr_info("Error at converting string to integer %s\n", data);
				        return -EINVAL;
				}
		         	ret = kstrtoul(data1, 0, &num1); 
		        	if (ret){
				       pr_info("Error at converting string to integer %s\n", data1);
 			               return -EINVAL;
		                }
				num_descriptors = num;
				option = num1;
		        }else {
			        pr_err("Invalid input format.'\n");
 			       return -EINVAL;
  			}
			memcpy(temp_buffer, read_back_buffer_descriptor(option), sizeof(temp_buffer));
            		if (copy_to_user((char *)arg, temp_buffer, sizeof(temp_buffer))) {
        	  	      return -EFAULT;
     		        }
			break;
                default:
                        pr_info("Default\n");
                        break;
        }
        return 0;
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
    if (transfer_success) {
        mask |= POLLIN | POLLRDNORM;  // Data is available for reading
    }
    if (transfer_failed) {
        mask |= POLLERR;  // Data is available for reading
    }

    return mask;
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
		chan->ctrl_offset = DMA_MM2S_OFFSET;
		ret = reset(DMA_MM2S_OFFSET);
		if (ret) { 
			pr_err("MM2S Channel reset failed: %d\n", ret);
			return ret;
		}
	} else if (of_device_is_compatible(node, "xlnx,axi-dma-s2mm-channel")) {
		chan->ctrl_offset = DMA_S2MM_OFFSET;
		ret = reset(DMA_S2MM_OFFSET);
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
	dma_device = &(pdev->dev);
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
	if (dma_set_mask_and_coherent(ddev->dev, DMA_BIT_MASK(addr_width))) {
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
	ret = device_create_file(sysfs_device, &dev_attr_mm2scur);
	if (ret)
		goto fail_attr1;

	ret = device_create_file(sysfs_device, &dev_attr_s2mmcur);
	if (ret)
		goto fail_attr2;

	ret = device_create_file(sysfs_device, &dev_attr_mm2stail);
	if (ret)
		goto fail_attr3;

	ret = device_create_file(sysfs_device, &dev_attr_s2mmtail);
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
	device_remove_file(sysfs_device, &dev_attr_s2mmtail);

fail_attr4:
	device_remove_file(sysfs_device, &dev_attr_mm2stail);

fail_attr3:
	device_remove_file(sysfs_device, &dev_attr_s2mmcur);

fail_attr2:
	device_remove_file(sysfs_device, &dev_attr_mm2scur);

fail_attr1:
	device_destroy(sysfs_class, MKDEV(major_number, 0));
	class_destroy(sysfs_class);
	unregister_chrdev(major_number, DEVICE_NAME);

	return ret;
}

static int custom_dma_remove(struct platform_device *pdev)
{      
	/*if(dma_vaddr_s2mm){
      		dma_free_coherent(dma_device, s2mm_bd_size, dma_vaddr_s2mm, dma_paddr_s2mm);
        	pr_info("Freed dma descriptor memory of mm2s channel on exit\n");
        }
        if (dma_vaddr_mm2s){
       		dma_free_coherent(dma_device, mm2s_bd_size, dma_vaddr_mm2s, dma_paddr_mm2s);
                pr_info("Freed dma descriptor memory of mm2s channel on exit \n");
        }*/
	if(dma_vaddr_s2mm){
		memunmap(dma_vaddr_s2mm);	
        	pr_info("Freed dma descriptor memory of mm2s channel on exit\n");
        }
        if (dma_vaddr_mm2s){
		memunmap(dma_vaddr_mm2s);	
		pr_info("Freed dma descriptor memory of mm2s channel on exit \n");
        }
	/* Cleanup */
    	device_remove_file(sysfs_device, &dev_attr_mm2stail);
	device_remove_file(sysfs_device, &dev_attr_s2mmtail);
    	device_remove_file(sysfs_device, &dev_attr_mm2scur);
    	device_remove_file(sysfs_device, &dev_attr_s2mmcur);
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
MODULE_DESCRIPTION("AXI-DMA Driver, which supports scatter gather mode and controlled by userspace application");
MODULE_LICENSE("GPL v2");



