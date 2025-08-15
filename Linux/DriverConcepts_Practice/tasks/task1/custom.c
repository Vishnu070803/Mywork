#include<linux/module.h>
#include<linux/moduleparam.h>
#include<linux/init.h>
#include<linux/fs.h>
#include<linux/kernel.h>
#include<linux/err.h>
#include<linux/device.h>
#include<linux/kdev_t.h>
static char *string;
static struct class *task_class;
module_param(string, charp, S_IRUSR | S_IWUSR);//for argument 
dev_t dev =0 ;
static int __init start(void){
   
// automatic major number allocation
  if((alloc_chrdev_region(&dev, 0, 1, string))<0){
pr_err("Error at major and minor number allocation");
return -1;
}
pr_info("Major no : %d and Minor no %d", MAJOR(dev), MINOR(dev));
//class creation
task_class=class_create("task_class");
if(IS_ERR(task_class)){
pr_err("Error at class creation");
goto c_destroy;
}
//device file creation
if(IS_ERR(device_create(task_class, NULL, dev, NULL, string))){
pr_err("Error at Device file creation");
goto d_destroy;
}
pr_info("Module inserted");
return 0;
d_destroy: 
class_destroy(task_class);
c_destroy:
unregister_chrdev_region(dev, 1);
return -1;
}

static void __exit end(void){
device_destroy(task_class, dev);
class_destroy(task_class);
unregister_chrdev_region(dev, 1);
pr_info("Module removed ");
}

module_init(start);
module_exit(end);

MODULE_AUTHOR("VISHNU");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TASK 1 TO CREATE A DEV NODE AND GET DEV NODE NAME AS ARGUMENT ");
