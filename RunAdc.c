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


#define AIN4_PATH "/sys/bus/iio/devices/iio:device0/in_voltage4_raw"
#define PWM_PERIOD_NS 1000000  // 1ms period (1kHz frequency)
#define PWM_PATH "/sys/class/pwm/pwmchip5/pwm0"

static struct timer_list my_timer;
static int pwm_duty_cycle = 0;


static int run_shell_command(const char *command) {
    struct subprocess_info *sub_info;
    char *argv[] = { "/bin/sh", "-c", (char *)command, NULL };
    char *envp[] = { "HOME=/", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };

    sub_info = call_usermodehelper_setup(argv[0], argv, envp, GFP_KERNEL, NULL, NULL, NULL);
    if (sub_info == NULL) {
        pr_err("Failed to setup usermodehelper\n");
        return -ENOMEM;
    }

    return call_usermodehelper_exec(sub_info, UMH_WAIT_PROC);
}
static void __exit my_module_exit(void) {
    struct file *f;
    char buf[16];
    del_timer(&my_timer);

    f = filp_open(PWM_PATH "/enable",O_WRONLY,0);
    if( !IS_ERR(f) ){
        snprintf(buf, sizeof(buf), "%d", 0);
        kernel_write(f, buf, strlen(buf), &f->f_pos);
        filp_close(f,NULL);
    }

    // Disable PWM
    f = filp_open(PWM_PATH "/unexport", O_WRONLY, 0);
    if (!IS_ERR(f)) {
        kernel_write(f, "0", 2, &f->f_pos);
        filp_close(f, NULL);
    }

    printk(KERN_INFO "Module exited\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("PWM Control Based on AIN4 Input");


static int pwm_init_fn(void) {
   char buf[16];
    struct file *f;
    f = filp_open(PWM_PATH "/enable",O_WRONLY,0);
    if( !IS_ERR(f) ){
        snprintf(buf, sizeof(buf), "%d", 1);
        kernel_write(f, buf, strlen(buf), &f->f_pos);
        filp_close(f,NULL);
    }


    return 0;
}

static void read_ain4_and_set_pwm(struct timer_list *t) {
    struct file *f;
    char buf[16];
    int ain4_value;
    mm_segment_t fs;
    fs = get_fs();
    set_fs(KERNEL_DS);
    f = filp_open(AIN4_PATH, O_RDONLY, 0);
    if (IS_ERR(f)) {
        printk(KERN_ALERT "Failed to read AIN4\n");
    } else {
        kernel_read(f, buf, sizeof(buf) - 1, &f->f_pos);
        buf[sizeof(buf) - 1] = '\0';
        ain4_value = simple_strtol(buf, NULL, 10);
        pwm_duty_cycle = ain4_value * PWM_PERIOD_NS / 4095;
        filp_close(f, NULL);
    }
    set_fs(fs);

    // Set PWM duty cycle
    f = filp_open(PWM_PATH "/duty_cycle", O_WRONLY, 0);
    if (!IS_ERR(f)) {
        snprintf(buf, sizeof(buf), "%d", pwm_duty_cycle);
        kernel_write(f, buf, strlen(buf), &f->f_pos);
        filp_close(f, NULL);
    }

    // Restart the timer
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(10
        0));

}

static int __init my_module_init(void) {
    int ret;
    struct file *f;
    char buf[16];

    // Enable PWM
    f = filp_open(PWM_PATH "/export", O_WRONLY, 0);
    if (!IS_ERR(f)) {
        kernel_write(f, "0", 2, &f->f_pos);
        filp_close(f, NULL);
    }

    // Set PWM period
    f = filp_open(PWM_PATH "/period", O_WRONLY, 0);
    if (!IS_ERR(f)) {
        snprintf(buf, sizeof(buf), "%d", PWM_PERIOD_NS);
        kernel_write(f, buf, strlen(buf), &f->f_pos);
        filp_close(f, NULL);
    }

    timer_setup(&my_timer, read_ain4_and_set_pwm, 0);
    ret = mod_timer(&my_timer, jiffies + msecs_to_jiffies(10));
    pwm_init_fn();
    if (ret)
        printk(KERN_ALERT "Failed to initialize timer\n");


    run_shell_command("config-pin P9_14 pwm");
    printk(KERN_INFO "Module initialized\n");

    return 0;
}
