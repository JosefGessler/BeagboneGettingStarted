#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/kmod.h>  // Needed for call_usermodehelper


//this module starts the PWM


#define AIN4_PATH "/sys/bus/iio/devices/iio:device0/in_voltage4_raw"
#define REFERENCE_VOLTAGE_V 3.3
#define MAX_NUMBEROF_DIGITS 4095

#define INITIAL_TIMER_DELAY_MS 100

static float currentAdcValue_Volt;

static struct timer_list my_timer;


static void read_ain4(struct timer_list *t) {
    struct file *f;
    char buf[16];
    int ain4_value_digits;
    mm_segment_t fs;
    fs = get_fs();
    set_fs(KERNEL_DS);
    f = filp_open(AIN4_PATH, O_RDONLY, 0);
    if (IS_ERR(f)) {
        printk(KERN_ALERT "Failed to read AIN4\n");
    } else {
        kernel_read(f, buf, sizeof(buf) - 1, &f->f_pos);
        buf[sizeof(buf) - 1] = '\0';
        ain4_value_digits = simple_strtol(buf, NULL, 10);
        currentAdcValue_Volt = ain4_value_digits * REFERENCE_VOLTAGE_V / (MAX_NUMBEROF_DIGITS-1);
        filp_close(f, NULL);
    }
    set_fs(fs);

    // Restart the timer
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(100));

}


static ssize_t read_voltage(struct file* file, char __user* buffer, size_t count, loff_t* ppos) {
   
    char adc_str[32];

    snprintf(adc_str, sizeof(adc_str), "%f\n", currentAdcValue_Volt);

    if (copy_to_user(buffer, adc_str, strlen(adc_str)) != 0) {
        return -EFAULT;
    }
    return strlen(adc_str);
}



static const struct file_operations ain4_fops = {
    .owner = THIS_MODULE,
    .read = read_voltage,
};


static struct miscdevice ain4_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "ain4_input",
    .fops = &ain4_fops,
};

static int __init my_module_init(void) {
    int ret;
    struct file *f;

    timer_setup(&my_timer, read_ain4, 0);
    ret = mod_timer(&my_timer, jiffies + msecs_to_jiffies(INITIAL_TIMER_DELAY_MS));

    if (ret)
        printk(KERN_ALERT "Failed to initialize timer\n");


    // Register the AIN4 device
    ret = misc_register(&ain4_device);
    if (ret) {
        pr_err("Failed to register AIN4 device\n");
        misc_deregister(&ain4_device);

        return ret;
    }


    printk(KERN_INFO "Module initialized\n");

    return 0;
}


static void __exit my_module_exit(void) {
    struct file* f;

    del_timer(&my_timer);


    // Unregister the devices
    misc_deregister(&ain4_device);


    printk(KERN_INFO "Module exited\n");
}


module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Josef Gessler");
MODULE_DESCRIPTION("Reads the ADC and offers an Interface to poll the current valeu");
