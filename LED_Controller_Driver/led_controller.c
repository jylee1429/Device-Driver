#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

static int led_state = 0;
static int led_pin, switch_pin;
static int switch_irq;

static irqreturn_t switch_irq_handler(int irq, void *dev_id) {
    led_state = !led_state;
    gpio_set_value(led_pin, led_state);
    pr_info("LED state changed to: %s\n", led_state ? "ON" : "OFF");
    
    return IRQ_HANDLED;
}

static int led_controller_probe(struct platform_device* pdev) {
    struct device* dev = &pdev->dev;
    struct device_node* node = dev->of_node;
    struct device_node* led_node;
    struct device_node* switch_node;

    if (!node) {
        dev_err(dev, "Device tree node not found\n");
        return -EINVAL;
    }
    // set led
    led_node = of_get_child_by_name(node, "led");
    if (!led_node) {
        dev_err(dev, "LED node not found\n");
        return -EINVAL;
    }

    led_pin = of_get_named_gpio(led_node, "gpios", 0);
    if (!gpio_is_valid(led_pin)) {
        dev_err(dev, "Invaild LED gpio\n");
        return -EINVAL;
    }

    if (gpio_request(led_pin, "LED GPIO")) {
        dev_err(dev, "Failed to request LED GPIO\n");
        gpio_free(led_pin); 
        return -EBUSY;
    }
    gpio_direction_output(led_pin, led_state); 

    // set switch gpio
    switch_node = of_get_child_by_name(node, "switch");
    if (!switch_node) {
        dev_err(dev, "Switch node not found\n");
        gpio_free(led_pin);
        return -EINVAL;
    }

    switch_pin = of_get_named_gpio(switch_node, "gpios", 0);
    if (!gpio_is_valid(switch_pin)) {
        dev_err(dev, "Invalid Switch GPIO\n");
        gpio_free(led_pin);
        return -EINVAL;
    }

    if (gpio_request(switch_pin, "Switch GPIO")) {
        dev_err(dev, "Failed to request Switch GPIO\n");
        gpio_free(led_pin);
        return -EBUSY;
    }
    gpio_direction_input(switch_pin);

    // set switch interrupt
    switch_irq = gpio_to_irq(switch_pin);
    if (switch_irq < 0) {
        dev_err(dev, "Failed to get IRQ for Switch GPIO\n");
        gpio_free(switch_pin);
        gpio_free(led_pin);
        return switch_irq;
    }

    if (request_irq(switch_irq, switch_irq_handler, IRQF_TRIGGER_FALLING, "switch_irq", NULL)) {
        dev_err(dev, "Failed to request IRQ\n");
        gpio_free(switch_pin);
        gpio_free(led_pin);
        return -EBUSY;
    }

    printk(KERN_INFO "LED Controller initialized successfully\n");

    return 0;
}

static int led_controller_remove(struct platform_device* pdev) {
    free_irq(switch_irq, NULL);
    gpio_free(switch_pin);
    gpio_set_value(led_pin, 0);
    gpio_free(led_pin);

    printk(KERN_INFO "GPIO Driver removed\n");

    return 0;
}

static const struct of_device_id led_controller_id_match[] = {
    {.compatible = "led-controller", },
    {},
};
MODULE_DEVICE_TABLE(of, led_controller_id_match);

static struct platform_driver led_controller_driver = {
    .probe = led_controller_probe,
    .remove = led_controller_remove,
    .driver = {
        .name = "led controller driver",
        .of_match_table = led_controller_id_match,
    },
};

module_platform_driver(led_controller_driver);

MODULE_LICENSE("GPL");
