#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>                 // kmalloc()
#include <linux/uaccess.h>              // copy_to/from_user()
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/err.h>

static struct class *task_class;
dev_t dev = 0;
struct cdev character_device;
static struct kobject *pointer;
static char value1[1000] = "default_value1_changeble";
static char value2[1000] = "default_value2_unchangeble";

// Function declarations
static ssize_t task3_show(struct kobject *kobj, struct kobj_attribute *attr, char *buffer);
static ssize_t task3_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buffer, size_t count);
static ssize_t file_read(struct file *filp, char __user *buffer, size_t len, loff_t *offset);
static ssize_t file_write(struct file *filp, const char __user *buffer, size_t len, loff_t *offset);
static int file_open(struct inode *inode, struct file *file);
static int file_release(struct inode *inode, struct file *file);

// File operations structure
static struct file_operations f_ops = {
    .owner = THIS_MODULE,
    .open = file_open,
    .read = file_read,
    .write = file_write,
    .release = file_release,
};

// Sysfs attribute creation
static struct kobj_attribute attribute1 = __ATTR(value1, 0664, task3_show, task3_store);
static struct kobj_attribute attribute2 = __ATTR(value2, 0444, task3_show, NULL);

// Sysfs show function for reading
static ssize_t task3_show(struct kobject *kobj, struct kobj_attribute *attr, char *buffer) {
    if (attr == &attribute1) {
        pr_info("Sysfs value1 is being read\n");
        return sprintf(buffer, "%s\n", value1);
    } else if (attr == &attribute2) {
        pr_info("Sysfs value2 is being read\n");
        return sprintf(buffer, "%s\n", value2);
    }
    return -EINVAL; // Invalid argument
}

// Sysfs store function for writing
static ssize_t task3_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buffer, size_t count) {
    if (attr == &attribute1) {
        pr_info("Sysfs value1 is being written to\n");
        if (count > sizeof(value1) - 1)
            count = sizeof(value1) - 1; // Prevent buffer overflow

        strncpy(value1, buffer, count);
        value1[count] = '\0'; // Null-terminate the string
        return count;
    }
    return -EINVAL;
}
static int file_open(struct inode *inode, struct file *file) {
    pr_info("OPEN FUNCTION CALLED\n");
    return 0;
}

static int file_release(struct inode *inode, struct file *file) {
    pr_info("RELEASE FUNCTION CALLED\n");
    return 0;
}

// Device file read function
static ssize_t file_read(struct file *filp, char __user *buffer, size_t len, loff_t *offset) {
    char temp[100];
    if (len > sizeof(temp) - 1)
        len = sizeof(temp) - 1;

    if (copy_from_user(temp, buffer, len)) {
        pr_err("Failed to receive data from user\n");
        return -EFAULT;
    }

    temp[len] = '\0'; // Null-terminate
    if (strncmp(temp, "read attr1:", 11) == 0) {
        return task3_show(NULL, &attribute1, buffer);
    } else if (strncmp(temp, "read attr2:", 11) == 0) {
        return task3_show(NULL, &attribute2, buffer);
    }

    return -EINVAL;
}

// Device file write function
static ssize_t file_write(struct file *filp, const char __user *buffer, size_t len, loff_t *offset) {
    char temp[100];

    if (len > sizeof(temp) - 1)
        len = sizeof(temp) - 1;

    if (copy_from_user(temp, buffer, len)) {
        pr_err("Failed to receive data from user\n");
        return -EFAULT;
    }

    temp[len] = '\0'; // Null-terminate
    if (strncmp(temp, "write attr1:", 12) == 0) {
        return task3_store(NULL, &attribute1, temp + 12, len - 12);
    }

    return -EINVAL;
}

// Module initialization
static int __init start(void) {
    if ((alloc_chrdev_region(&dev, 0, 1, "task3_class")) < 0) {
        pr_err("Error at major and minor number allocation\n");
        return -1;
    }
    pr_info("Major no: %d and Minor no: %d\n", MAJOR(dev), MINOR(dev));

    // Class creation
    task_class = class_create("task_class");
    if (IS_ERR(task_class)) {
        pr_err("Error at class creation\n");
        goto c_destroy;
    }

    // Device file creation
    if (IS_ERR(device_create(task_class, NULL, dev, NULL, "mysysfs_dev"))) {
        pr_err("Error at device file creation\n");
        goto d_destroy;
    }

    // Character device initialization and registration
    cdev_init(&character_device, &f_ops);
    if ((cdev_add(&character_device, dev, 1)) < 0) {
        pr_err("Error at character device registration\n");
        goto d_destroy;
    }

    // Sysfs object and attributes creation
    pointer = kobject_create_and_add("vishnu", kernel_kobj);
    if (!pointer) {
        pr_err("Error at object creation\n");
        return -ENOMEM;
    }

    if (sysfs_create_file(pointer, &attribute1.attr)) {
        pr_err("Error at attribute 1\n");
        kobject_put(pointer);
        return -EINVAL;
    }

    if (sysfs_create_file(pointer, &attribute2.attr)) {
        pr_err("Error at attribute 2\n");
        sysfs_remove_file(pointer, &attribute1.attr);
        kobject_put(pointer);
        return -EINVAL;
    }

    pr_info("Module inserted\n");
    return 0;

d_destroy:
    class_destroy(task_class);
c_destroy:
    unregister_chrdev_region(dev, 1);
    return -1;
}

// Module cleanup
static void __exit end(void) {
    device_destroy(task_class, dev);
    class_destroy(task_class);
    unregister_chrdev_region(dev, 1);
    sysfs_remove_file(pointer, &attribute1.attr);
    sysfs_remove_file(pointer, &attribute2.attr);
    kobject_put(pointer);
    pr_info("Module removed...\n");
}

module_init(start);
module_exit(end);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("VISHNU");
MODULE_DESCRIPTION("Two attributes with different permissions");
MODULE_VERSION("1.8");

