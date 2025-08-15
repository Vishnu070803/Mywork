#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>


int __init vishnu_ini(void){
			printk(KERN_INFO"Loaded");
			 return 0;
			}
			
void __exit vishnu_exi(void){
			printk(KERN_INFO"unLoaded");
		
			}
module_init(vishnu_ini);
module_exit(vishnu_exi);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("VISHNU");
MODULE_DESCRIPTION("STARTING BROO");			  
			
