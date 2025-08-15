#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>

 static int number ;
static char *string;
module_param(string, charp, S_IRUGO);
module_param(number, int, S_IRUGO);
static int __init vishnu_init(void)
{
printk(KERN_INFO "NUMBER IS %d AND STRING IS %s\n", number, string);
printk(KERN_INFO "SUCCESSFULLY LOADED");
  return 0;
}
static void __exit vishnu_exit(void)
{
 printk(KERN_INFO "REMOVED");
}
module_init(vishnu_init);
module_exit(vishnu_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MODULE PARAMETERS PASSING");
MODULE_AUTHOR("VISHNU");
MODULE_VERSION("0");
