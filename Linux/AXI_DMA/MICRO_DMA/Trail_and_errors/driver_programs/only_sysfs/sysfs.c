#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "my_sysfs_driver"
#define DEVICE_NAME "my_sysfs_device"
#define BUFFER_SIZE 1024

// Variables for sysfs attributes
static char address1[100] = "0x1234";
static char address2[100] = "0x5678";
static char size[100] = "256";
static char device_buffer[BUFFER_SIZE];

static struct class *sysfs_class;
static struct device *sysfs_device;

static int major_number;

// Device file operations prototypes
static int dev_open(struct inode *inode, struct file *file);
static ssize_t dev_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset);
static ssize_t dev_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset);
static int dev_release(struct inode *inode, struct file *file);

// Define file operations structure
static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// Generic read attribute function
static ssize_t attr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (strcmp(attr->attr.name, "address1") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", address1);
    else if (strcmp(attr->attr.name, "address2") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", address2);
    else if (strcmp(attr->attr.name, "size") == 0)
        return scnprintf(buf, PAGE_SIZE, "%s\n", size);

    return -EINVAL;
}

// Generic write attribute function
static ssize_t attr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    if (strcmp(attr->attr.name, "address1") == 0)
        snprintf(address1, sizeof(address1), "%.*s", (int)count, buf);
    else if (strcmp(attr->attr.name, "address2") == 0)
        snprintf(address2, sizeof(address2), "%.*s", (int)count, buf);
    else if (strcmp(attr->attr.name, "size") == 0)
        snprintf(size, sizeof(size), "%.*s", (int)count, buf);
    else
        return -EINVAL;

    return count;
}

// Declare the device attributes
static DEVICE_ATTR(address1, 0664, attr_show, attr_store);
static DEVICE_ATTR(address2, 0664, attr_show, attr_store);
static DEVICE_ATTR(size, 0664, attr_show, attr_store);

// Device file operations implementation
static int dev_open(struct inode *inode, struct file *file)
{
    pr_info("Device file opened\n");
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset)
{
    char temp_buffer[BUFFER_SIZE];
    int bytes_to_copy;

    // Combine all attributes into a single buffer
    snprintf(temp_buffer, sizeof(temp_buffer),
             "address1: %s\naddress2: %s\nsize: %s\n",
             address1, address2, size);

    if (*offset >= strlen(temp_buffer))
        return 0; // EOF

    // Copy data from kernel space to user space
    bytes_to_copy = min(len, strlen(temp_buffer) - (size_t)*offset);
    if (copy_to_user(user_buffer, temp_buffer + *offset, bytes_to_copy))
        return -EFAULT;

    *offset += bytes_to_copy;
    pr_info("Device file read: %d bytes\n", bytes_to_copy);
    return bytes_to_copy;
}

static ssize_t dev_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset)
{
    char temp_buffer[BUFFER_SIZE];
    char new_address1[100], new_address2[100], new_size[100];

    if (len >= sizeof(temp_buffer))
        return -EINVAL;

    // Copy data from user space to kernel space
    if (copy_from_user(temp_buffer, user_buffer, len))
        return -EFAULT;

    temp_buffer[len] = '\0'; // Null-terminate the string

    // Parse the input into three attributes
    if (sscanf(temp_buffer, "address1: %99s address2: %99s size: %99s", 
               new_address1, new_address2, new_size) == 3) {
        // Update the attributes
        strncpy(address1, new_address1, sizeof(address1));
        strncpy(address2, new_address2, sizeof(address2));
        strncpy(size, new_size, sizeof(size));

        pr_info("Device file written: address1=%s, address2=%s, size=%s\n",
                address1, address2, size);
        return len;
    }

    pr_err("Invalid input format. Expected: address1: <value> address2: <value> size: <value>\n");
    return -EINVAL;
}


static int dev_release(struct inode *inode, struct file *file)
{
    pr_info("Device file closed\n");
    return 0;
}

// Module init function
static int __init sysfs_driver_init(void)
{
    int ret;

    // Register a major number
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        pr_err("Failed to register a major number\n");
        return major_number;
    }

    // Create a class in /sys/class/
    sysfs_class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(sysfs_class)) {
        pr_err("Failed to create class\n");
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sysfs_class);
    }

    // Create a device in the class
    sysfs_device = device_create(sysfs_class, NULL, MKDEV(major_number, 0), NULL, DRIVER_NAME);
    if (IS_ERR(sysfs_device)) {
        pr_err("Failed to create device\n");
        class_destroy(sysfs_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sysfs_device);
    }

    // Create the attribute files
    ret = device_create_file(sysfs_device, &dev_attr_address1);
    if (ret)
        goto fail_attr1;

    ret = device_create_file(sysfs_device, &dev_attr_address2);
    if (ret)
        goto fail_attr2;

    ret = device_create_file(sysfs_device, &dev_attr_size);
    if (ret)
        goto fail_attr3;

    pr_info("Sysfs driver with device file initialized\n");
    return 0;

fail_attr3:
    device_remove_file(sysfs_device, &dev_attr_address2);
fail_attr2:
    device_remove_file(sysfs_device, &dev_attr_address1);
fail_attr1:
    device_destroy(sysfs_class, MKDEV(major_number, 0));
    class_destroy(sysfs_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    return ret;
}

// Module exit function
static void __exit sysfs_driver_exit(void)
{
    // Remove the attribute files
    device_remove_file(sysfs_device, &dev_attr_size);
    device_remove_file(sysfs_device, &dev_attr_address2);
    device_remove_file(sysfs_device, &dev_attr_address1);

    // Destroy the device and class
    device_destroy(sysfs_class, MKDEV(major_number, 0));
    class_destroy(sysfs_class);

    // Unregister the major number
    unregister_chrdev(major_number, DEVICE_NAME);

    pr_info("Sysfs driver with device file removed\n");
}

module_init(sysfs_driver_init);
module_exit(sysfs_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChatGPT");
MODULE_DESCRIPTION("Example sysfs driver with device file operations");

