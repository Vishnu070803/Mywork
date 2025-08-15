
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>                 //kmalloc()
#include <linux/uaccess.h>              //copy_to/from_user()
#include <linux/kthread.h>
#include <linux/wait.h>                 // Required for the wait queues
#include <linux/err.h>
 
 
uint32_t read_count = 0;
uint32_t write_count = 0;
static struct task_struct *wait_thread;
 
DECLARE_WAIT_QUEUE_HEAD(wait_queue_sample_waitqueue);
 
dev_t dev = 0;
static struct class *dev_class;
static struct cdev sample_waitqueue_cdev;
int wait_queue_flag = 0;

/*
** Function Prototypes
*/
static int      __init sample_waitqueue_driver_init(void);
static void     __exit sample_waitqueue_driver_exit(void);
 
/*************** Driver functions **********************/
static int      sample_waitqueue_open(struct inode *inode, struct file *file);
static int      sample_waitqueue_release(struct inode *inode, struct file *file);
static ssize_t  sample_waitqueue_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static ssize_t  sample_waitqueue_write(struct file *filp, const char *buf, size_t len, loff_t * off);

/*
** File operation sturcture
*/
static struct file_operations fops =
{
        .owner          = THIS_MODULE,
        .read           = sample_waitqueue_read,
        .write          = sample_waitqueue_write,
        .open           = sample_waitqueue_open,
        .release        = sample_waitqueue_release,
};

/*
** Thread function
*/
static int wait_function(void *unused)
{
        
        while(1) {
                pr_info("Waiting For Event...\n");
                wait_event_interruptible(wait_queue_sample_waitqueue, wait_queue_flag != 0 );
                if(wait_queue_flag == 2) {
                        pr_info("Event Came From Exit Function\n");
                        return 0;
                }else if(wait_queue_flag == 3){
                pr_info("Event Came From write Function - %d\n", ++write_count);
                wait_queue_flag = 0;           
                }else{
                pr_info("Event Came From read Function - %d\n", ++read_count);
                wait_queue_flag = 0;}
        }
        do_exit(0);
        return 0;
}

/*
** This function will be called when we open the Device file
*/
static int sample_waitqueue_open(struct inode *inode, struct file *file)
{
        pr_info("Device File Opened...!!!\n");
        return 0;
}

/*
** This function will be called when we close the Device file
*/
static int sample_waitqueue_release(struct inode *inode, struct file *file)
{
        pr_info("Device File Closed...!!!\n");
        return 0;
}

/*
** This function will be called when we read the Device file
*/
static ssize_t sample_waitqueue_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
        pr_info("Read Function\n");
        wait_queue_flag = 1;
        wake_up_interruptible(&wait_queue_sample_waitqueue);
        return 0;
}

/*
** This function will be called when we write the Device file
*/
static ssize_t sample_waitqueue_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
 wait_queue_flag = 3;
        wake_up_interruptible(&wait_queue_sample_waitqueue);
        pr_info("Write function\n");
        return len;
}
 
/*
** Module Init function
*/
static int __init sample_waitqueue_driver_init(void)
{
        /*Allocating Major number returns negative number incase of error */
        if((alloc_chrdev_region(&dev, 0, 1, "sample_waitqueue_Dev")) <0){
                pr_info("Cannot allocate major number\n");
                return -1;
        }
        pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));
 
        /*Creating cdev structure*/
        cdev_init(&sample_waitqueue_cdev,&fops);
        sample_waitqueue_cdev.owner = THIS_MODULE;
        sample_waitqueue_cdev.ops = &fops;
 
        /*Adding character device to the system*/
        if((cdev_add(&sample_waitqueue_cdev,dev,1)) < 0){
            pr_info("Cannot add the device to the system\n");
            goto r_class;
        }
 
        /*Creating struct class*/
        if(IS_ERR(dev_class = class_create("sample_waitqueue_class"))){
            pr_info("Cannot create the struct class\n");
            goto r_class;
        }
 
        /*Creating device*/
        if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"sample_waitqueue_device"))){
            pr_info("Cannot create the Device 1\n");
            goto r_device;
        }
 
        //Create the kernel thread with name 'mythread'
        wait_thread = kthread_create(wait_function, NULL, "WaitThread");
        if (wait_thread) {
                pr_info("Thread Created successfully\n");
                wake_up_process(wait_thread);
        } else
                pr_info("Thread creation failed\n");
 
        pr_info("Device Driver Insert...Done!!!\n");
        return 0;
 
r_device:
        class_destroy(dev_class);
r_class:
        unregister_chrdev_region(dev,1);
        return -1;
}

/*
** Module exit function
*/ 
static void __exit sample_waitqueue_driver_exit(void)
{
        wait_queue_flag = 2;
        wake_up_interruptible(&wait_queue_sample_waitqueue);
        device_destroy(dev_class,dev);
        class_destroy(dev_class);
        cdev_del(&sample_waitqueue_cdev);
        unregister_chrdev_region(dev, 1);
        pr_info("Device Driver Remove...Done!!!\n");
}
 
module_init(sample_waitqueue_driver_init);
module_exit(sample_waitqueue_driver_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("VISHNU");
MODULE_DESCRIPTION("Simple linux driver (Waitqueue Static method)");
MODULE_VERSION("1.7");
