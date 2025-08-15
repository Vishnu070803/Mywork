#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/fs.h>
static dev_t dev;
static int __init own_init(void)
{
//dev = MKDEV(200, 0);
  if (alloc_chrdev_region(&dev, 0, 1, "own_bro_dynamic") < 0) {
        printk(KERN_ALERT "Failed to register character device region\n");
        return -EINVAL; // Return error if registration fails
    }
printk(KERN_INFO "Major = %d Minor = %d \n", MAJOR(dev), MINOR(dev));
    printk(KERN_INFO "Kernel Module Inserted Successfully...\n");
    return 0;
}

static void __exit own_exit(void)
{
unregister_chrdev_region(dev, 1);
    printk(KERN_INFO " Removed  \n");
}

module_init(own_init);
module_exit(own_exit);

MODULE_AUTHOR("vishnu");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A module that accepts array as parameter");
