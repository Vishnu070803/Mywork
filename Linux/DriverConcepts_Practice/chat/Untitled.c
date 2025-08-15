#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/moduleparam.h>
static char *string;
static int times = 1;
static int count;
static int Array[5];

// Correct permission settings
module_param(times, int, S_IRUGO | S_IWUSR);
module_param(string, charp, S_IRUGO);
module_param_array(Array, int, &count, S_IRUGO | S_IWUSR);

static int __init own_init(void)
{
    int i; // Declare variable 'i'
    // Check if times is negative to avoid invalid loop count
    if (times < 0) {
        printk(KERN_ALERT "Invalid times value: %d\n", times);
        return -EINVAL; // Return an error
    }

    for (i = 0; i < times; i++)
    {
        printk(KERN_ALERT "THIS IS THE %d PRINTING STRING %s\n", i, string);
    }

    for (i = 0; i < count; i++)
    {
        printk(KERN_ALERT "THIS IS POSITION %d AND THE ELEMENT IS %d\n", i, Array[i]);
    }

    return 0; 
}

static void __exit own_exit(void)
{
    printk(KERN_ALERT "Removed\n");
}

module_init(own_init);
module_exit(own_exit);

MODULE_AUTHOR("vishnu");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A module that accepts array as parameter");

