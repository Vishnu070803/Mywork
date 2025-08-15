#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>  // For memory-mapped I/O
#include <linux/ioport.h>

#define MY_IRQ 11            // Hypothetical interrupt number
#define MY_REGISTER_ADDR 0xF00F0000 // Hypothetical register address

static void __iomem *my_register;

// Interrupt handler
static irqreturn_t my_interrupt_handler(int irq, void *dev_id)
{
    uint32_t reg_value;

    // Read value from the hypothetical register
    reg_value = ioread32(my_register);

    // Print the register value
    printk(KERN_INFO "Interrupt! Register value: %u\n", reg_value);

    return IRQ_HANDLED; // Acknowledge interrupt
}

// Init function
static int __init my_module_init(void)
{
    int result;

    // Request memory region for the register
    if (!request_mem_region(MY_REGISTER_ADDR, sizeof(uint32_t), "my_register")) {
        printk(KERN_ERR "Failed to request memory region for register\n");
        return -EBUSY;
    }

    // Map the register address to kernel virtual address space
    my_register = ioremap(MY_REGISTER_ADDR, sizeof(uint32_t));
    if (!my_register) {
        printk(KERN_ERR "Failed to map register address\n");
        release_mem_region(MY_REGISTER_ADDR, sizeof(uint32_t));
        return -ENOMEM;
    }

    // Register the interrupt handler
    result = request_irq(MY_IRQ, my_interrupt_handler, IRQF_SHARED, "my_interrupt", (void *)(my_interrupt_handler));
    if (result) {
        printk(KERN_ERR "Failed to request IRQ %d\n", MY_IRQ);
        iounmap(my_register);
        release_mem_region(MY_REGISTER_ADDR, sizeof(uint32_t));
        return result;
    }

    printk(KERN_INFO "Module loaded, interrupt handler registered\n");
    return 0;
}

// Cleanup function
static void __exit my_module_exit(void)
{
    // Free the IRQ
    free_irq(MY_IRQ, (void *)(my_interrupt_handler));

    // Unmap and release the register memory
    iounmap(my_register);
    release_mem_region(MY_REGISTER_ADDR, sizeof(uint32_t));

    printk(KERN_INFO "Module unloaded, interrupt handler unregistered\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VISHNU");
MODULE_DESCRIPTION("Example module to read register and trigger interrupt");

