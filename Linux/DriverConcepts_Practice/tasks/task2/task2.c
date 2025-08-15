#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/device.h>
#include<linux/uaccess.h>              //copy_to/from_user()
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/slab.h>  // for kmalloc and kfree
#define K_SPACE 4096

uint8_t *kernel_temp;
static char *my_char;
static struct class *task_class;
module_param(my_char, charp, S_IRUSR | S_IWUSR); // for argument
dev_t dev = 0;
struct cdev character_device;

static int __init start(void);
static void __exit end(void);
static int task2_open(struct inode *inode, struct file *file);
static int task2_release(struct inode *inode, struct file *file);
static ssize_t task2_read(struct file *filp, char __user *buf, size_t len, loff_t *off);
static ssize_t task2_write(struct file *filp, const char __user *buf, size_t len, loff_t *off);

static struct file_operations f_ops = {
    .owner = THIS_MODULE,
    .open = task2_open,
    .read = task2_read,
    .write = task2_write,
    .release = task2_release,
};

static int task2_open(struct inode *inode, struct file *file) {
    pr_info("OPEN FUNCTION CALLED\n");
    return 0;
}

static int task2_release(struct inode *inode, struct file *file) {
    pr_info("RELEASE FUNCTION CALLED\n");
    return 0;
}

static ssize_t task2_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    pr_info("Driver Write Function Called...!!!\n");


    // Copy data from user space to kernel space
    if (copy_from_user(kernel_temp , buf, len)) {
        pr_err("Error at copying from user to kernel\n");
        return -EFAULT; // Return error for copy failure
    }
    // Return the number of bytes written
    pr_info("Successfully written %zu bytes\n", len);
    return len; // Return number of bytes written
}

static ssize_t task2_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    pr_info("Driver Read Function Called...!!!\n");
    // Copy data from kernel space to user space
    if (copy_to_user(buf, kernel_temp, len)) {
        pr_err("Error at copying from kernel to user\n");
        return -EFAULT; // Return error for copy failure
    }
    pr_info("Driver Read Function Answered, read %zu bytes\n", len);
    return len; // Return number of bytes read
}

static int __init start(void) {
    // automatic major number allocation
    if ((alloc_chrdev_region(&dev, 0, 1, my_char)) < 0) {
        pr_err("Error at major and minor number allocation\n");
        return -1;
    }
    pr_info("Major no : %d and Minor no %d\n", MAJOR(dev), MINOR(dev));

    // class creation
    task_class = class_create("task_class");
    if (IS_ERR(task_class)) {
        pr_err("Error at class creation\n");
        goto c_destroy;
    }

    // device file creation
    if (IS_ERR(device_create(task_class, NULL, dev, NULL, my_char))) {
        pr_err("Error at Device file creation\n");
        goto d_destroy;
    }

    // character device initialization 
    cdev_init(&character_device, &f_ops);
    // character device registration into kernel with major number of device file
    if ((cdev_add(&character_device, dev, 1)) < 0) {
        pr_err("Error at Character device registration\n");
        goto d_destroy;
    }

    // Allocate memory for the kernel space
    kernel_temp = kmalloc(K_SPACE, GFP_KERNEL);
    if (kernel_temp == NULL) {
        pr_err("Error at Memory allocation in kernel\n");
        return -1;
    }
    strcpy(kernel_temp, "Hello_World");
    pr_info("Module inserted\n");
    return 0;

d_destroy: 
    device_destroy(task_class, dev);
c_destroy:
    class_destroy(task_class);
    unregister_chrdev_region(dev, 1);
    return -1;
}

static void __exit end(void) {
    kfree(kernel_temp);
    device_destroy(task_class, dev);
    class_destroy(task_class);
    cdev_del(&character_device);
    unregister_chrdev_region(dev, 1);
    pr_info("Module removed\n");
}

module_init(start);
module_exit(end);

MODULE_AUTHOR("VISHNU");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TASK 2 : TO APPEND FILE OPERATIONS AND ACCESS THOSE OPERATIONS FROM USER SPACE APPLICATION");

