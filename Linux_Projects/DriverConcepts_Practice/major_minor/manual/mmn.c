#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/fs.h>
static char *string;
static int times = 1;
static int count;
static int Array[5];
static dev_t dev;
module_param(times, int, S_IRUGO|S_IWUSR);
module_param(string, charp, S_IRUGO);
module_param_array(Array, int, &count, S_IRUGO);

static int __init own_init(void)
{
dev = MKDEV(200, 0);
  if (register_chrdev_region(dev, 1, "own_bro") < 0) {
        printk(KERN_ALERT "Failed to register character device region\n");
        return -EINVAL; // Return error if registration fails
    }
printk(KERN_INFO "Major = %d Minor = %d \n", MAJOR(dev), MINOR(dev));
    printk(KERN_INFO "Kernel Module Inserted Successfully...\n");
  
 int i;
 if(times<0)
  {
  printk(KERN_ALERT " THIS IS INVALID TIMES ");
  return -EINVAL;
  }
  
  for(i=0;i<=times;i++)
  {
  printk(KERN_INFO " THIS IS  %d TIME PRINTING STRING %s ", i, string);
  }
  
  for(i=0;i<count;i++)
  {
  printk(KERN_WARNING " THIS IS POSITION %d AND THE ELEMENT IS %d ", i, Array[i]);
  }
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
MODULE_DESCRIPTION("Manual major and minor allocation");
