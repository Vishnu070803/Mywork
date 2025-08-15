#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/irqflags.h>

#define DRIVER_NAME "gpio-keys"

struct gpio_key_data {
    struct input_dev *input_dev;
    int gpio_pin;
    int irq;
    const char *label;
};

static irqreturn_t gpio_key_irq_handler(int irq, void *dev_id)
{
    struct gpio_key_data *gpio_key = dev_id;

    input_report_key(gpio_key->input_dev, KEY_1, 1);  // Report key press
    input_sync(gpio_key->input_dev);                  // Synchronize input event
    input_report_key(gpio_key->input_dev, KEY_1, 0);  // Report key release
    input_sync(gpio_key->input_dev);                  // Synchronize input event
    
    printk(KERN_INFO "Interrupt handles success\n");

    return IRQ_HANDLED;
}

static int gpio_key_probe(struct platform_device *pdev)
{
    struct gpio_key_data *gpio_key;
    struct device_node *np = pdev->dev.of_node;
    int ret;
	printk(KERN_INFO "Probe started\n");
    gpio_key = devm_kzalloc(&pdev->dev, sizeof(*gpio_key), GFP_KERNEL);
    if (!gpio_key)
        return -ENOMEM;

    platform_set_drvdata(pdev, gpio_key);

   struct device_node *key_node = of_get_child_by_name(np, "switch-19");
   if (!key_node) {
    dev_err(&pdev->dev, "Failed to find switch-19 node\n");
    return -EINVAL;
   }

  
   gpio_key->label = of_get_property(key_node, "label", NULL);
   if (!gpio_key->label) {
     dev_err(&pdev->dev, "Missing label property in switch-19\n");
     return -EINVAL;
   }
   of_node_put(key_node);  // Don't forget to release the reference to the node

   gpio_key->gpio_pin = of_get_named_gpio(key_node, "gpios", 0);
   if (gpio_key->gpio_pin < 0) {
      dev_err(&pdev->dev, "GPIO retrieval failed for %s\n", gpio_key->label);
      of_node_put(key_node);  // Always release reference when done
      return gpio_key->gpio_pin;
   } else {
      dev_info(&pdev->dev, "GPIO pin %d successfully retrieved for %s\n", gpio_key->gpio_pin, gpio_key->label);
   }

   ret = devm_gpio_request_one(&pdev->dev, gpio_key->gpio_pin, GPIOF_IN, gpio_key->label);
   if (ret) { // Check for ANY error, including -EBUSY
     dev_err(&pdev->dev, "Failed to request GPIO pin %d (error: %d)\n", gpio_key->gpio_pin, ret);
     return ret; // Return the error (no need to check specifically for -EBUSY)
   }

	printk(KERN_INFO "GPIO request success\n");

    gpio_key->input_dev = devm_input_allocate_device(&pdev->dev);
    if (!gpio_key->input_dev) {
        dev_err(&pdev->dev, "Failed to allocate input device\n");
        return -ENOMEM;
    }
	printk(KERN_INFO "Allocate device as input success\n");
    gpio_key->input_dev->name = DRIVER_NAME;
    gpio_key->input_dev->phys = "gpio-keys/input0";
    gpio_key->input_dev->id.bustype = BUS_HOST;
    input_set_capability(gpio_key->input_dev, EV_KEY, KEY_1); // Map GPIO to a key

    ret = input_register_device(gpio_key->input_dev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to register input device\n");
        return ret;
    }
	printk(KERN_INFO "Device register success\n");
    gpio_key->irq = gpio_to_irq(gpio_key->gpio_pin);
    if (gpio_key->irq < 0) {
        dev_err(&pdev->dev, "Failed to get IRQ for GPIO pin\n");
        return gpio_key->irq;
    }
	printk(KERN_INFO "IRQ number is %d\n", gpio_key->irq);
    ret = request_irq(gpio_key->irq, gpio_key_irq_handler, IRQF_TRIGGER_RISING, "gpio-key", gpio_key);
    if (ret) {
        dev_err(&pdev->dev, "Failed to request IRQ\n");
        return ret;
    }
	printk(KERN_INFO "request irq success\n");
	printk(KERN_INFO "probe success\n");
    return 0;
}

static int gpio_key_remove(struct platform_device *pdev)
{
    struct gpio_key_data *gpio_key = platform_get_drvdata(pdev);

    free_irq(gpio_key->irq, gpio_key);
    input_unregister_device(gpio_key->input_dev);
    return 0;
}

static const struct of_device_id gpio_key_dt_ids[] = {
    { .compatible = "gpio-keys", },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, gpio_key_dt_ids);

static struct platform_driver gpio_key_driver = {
    .probe = gpio_key_probe,
    .remove = gpio_key_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = gpio_key_dt_ids,
    },
};

module_platform_driver(gpio_key_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VISHNU");
MODULE_DESCRIPTION("Custom GPIO Keys Driver");

