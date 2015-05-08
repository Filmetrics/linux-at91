/*******************************************************************************
*
*  fan_tach.c - Fan Tach Measurement Driver
*
*  Author: Raphael Oberson
*
*  Cross-Compiler: arm-linux-gcc (Buildroot 2012.11.1) 4.7.2
*
*  License: GNU General Public License
*
*  Copyright (C) 2015, Filmetrics Inc.
*
*******************************************************************************/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/gpio.h>

#define DEVICE_NAME		"fan_tach"
#define FAN_TACH_MAJOR		240


/* CONFIG_FAN_TACH_* parameters are set in the kernel's make menuconfig */
#ifdef CONFIG_FAN_TACH_TIMER_PERIOD_ms
#define TIMER_PERIOD_ms		CONFIG_FAN_TACH_TIMER_PERIOD_ms
#else
#define TIMER_PERIOD_ms		100
#endif

#ifdef CONFIG_FAN_TACH_REFRESH_RATE_FAN1_ms
#define REFRESH_RATE_FAN1_ms	CONFIG_FAN_TACH_REFRESH_RATE_FAN1_ms
#else
#define REFRESH_RATE_FAN1_ms	2000
#endif

#ifdef CONFIG_FAN_TACH_REFRESH_RATE_FAN2_ms
#define REFRESH_RATE_FAN2_ms	CONFIG_FAN_TACH_REFRESH_RATE_FAN2_ms
#else
#define REFRESH_RATE_FAN2_ms	2000
#endif

#define TIMER_PERIOD_JIFFIES	(TIMER_PERIOD_ms * HZ / 1000)

struct fan {
	const unsigned int gpio;	/* GPIO pin for this fan              */
	const char *const label;	/* GPIO description                   */
	const int			/* Number of TIMER_PERIOD_ms between  */
	    num_periods_per_refresh;	/* fan speed refreshes, for this fan  */
	int speed;			/* Fan speed, expressed as number     */
					/* of transitions in                  */
					/* REFRESH_RATE_FAN<n>_ms             */
	int num_transitions;		/* Increments every time the FAN_TACH */
					/* pin transitions low-to-high or     */
					/* high-to-low                        */
	int old_gpio_fan_tach;		/* State of FAN_TACH pin last call    */
	int timer_count;		/* Increments every TIMER_PERIOD_ms   */
};

static struct fan fan1 = {
	.gpio			 = AT91_PIN_PD10,
	.label			 = "GPIO_FAN_TACH_1",
	.num_periods_per_refresh = REFRESH_RATE_FAN1_ms / TIMER_PERIOD_ms,
	.speed			 = 0,
	.num_transitions	 = 0,
	.old_gpio_fan_tach	 = 0,
	.timer_count		 = 0,
};

static struct fan fan2 = {
	.gpio			 = AT91_PIN_PD11,
	.label			 = "GPIO_FAN_TACH_2",
	.num_periods_per_refresh = REFRESH_RATE_FAN2_ms / TIMER_PERIOD_ms,
	.speed			 = 0,
	.num_transitions	 = 0,
	.old_gpio_fan_tach	 = 0,
	.timer_count		 = 0,
};

/*
 * fan_tach_read()
 * Called whenever the user reads the device file.
 * Returns the current transition count (fan speed) as a string.
 */
static ssize_t fan_tach_read(struct file *filp, char __user *user_buf,
			     size_t count, loff_t *f_pos)
{
	enum { BUF_SIZE = 16 };
	static char buf[BUF_SIZE];
	int fan_speed;
	int len;

	/* 2nd read: tell the client there is no more data */
	if (*f_pos != 0)
		return 0;

	/* Get the speed for the requested fan, indicated by the minor number */
	switch (iminor(filp->f_dentry->d_inode)) {
	/* /dev/fan_tach1 */
	case 1:
		fan_speed = fan1.speed;
		break;

	/* /dev/fan_tach2 */
	case 2:
		fan_speed = fan2.speed;
		break;

	default:
		fan_speed = -1;
		break;
	}

	/* Convert return value to string */
	len = snprintf(buf, BUF_SIZE, "%d\n", fan_speed);

	/* Make sure the data fits */
	if (len > count)
		len = count;

	if (copy_to_user(user_buf, buf, len) != 0)
		return -EFAULT;

	*f_pos += len;
	return len;
}

static struct file_operations fan_tach_fops = {
	.owner	= THIS_MODULE,
	.read	= fan_tach_read,
};

/*
 * update_fan()
 * Fan update function: gets called by the timer update function twice
 * every TIMER_PERIOD_ms -- once for each fan. Updates fan measurements.
 */
static void update_fan(struct fan *fan)
{
	int gpio_fan_tach;	/* Current state of FAN_TACH pin */

	fan->timer_count++;
	if (fan->timer_count >= fan->num_periods_per_refresh) {
		/* This gets executed every REFRESH_RATE_FAN<n>_ms (for each fan) */
		fan->timer_count = 0;
		fan->speed = fan->num_transitions;
		fan->num_transitions = 0;
	}

	/* Check whether the FAN_TACH pin just changed state */
	gpio_fan_tach = gpio_get_value(fan->gpio);
	if (gpio_fan_tach != fan->old_gpio_fan_tach) {
		fan->old_gpio_fan_tach = gpio_fan_tach;
		fan->num_transitions++;
	}
}

static void timer_cb(unsigned long data);

/*
 * Kernel timer structure, used to give kernel which function to call back when
 * timer expires.
 */
static struct timer_list timer = {
	.function	= timer_cb,
	.data		= 0
};

/*
 * timer_cb()
 * Timer callback function: gets called by the kernel every TIMER_PERIOD_ms.
 */
static void timer_cb(unsigned long data)
{
	/* Update the fans */
	update_fan(&fan1);
	update_fan(&fan2);

	/* Reload timer */
	timer.expires += TIMER_PERIOD_JIFFIES;
	add_timer(&timer);
}

static inline int init_fan(const struct fan *fan)
{
	/* Set up GPIO pin */
	return gpio_request_one(fan->gpio, GPIOF_IN, fan->label);
}

static int __init fan_tach_init(void)
{
	int err = 0;

	/* Register char device */
	err = register_chrdev(FAN_TACH_MAJOR, DEVICE_NAME, &fan_tach_fops);
	if (err < 0)
		goto err_out;

	/* Initialize fans */
	err = init_fan(&fan1);
	if (err)
		goto err_out;

	err = init_fan(&fan2);
	if (err)
		goto err_out_free_fan1;

	/* Initialize timer and add first event */
	init_timer(&timer);

	timer.expires = jiffies + TIMER_PERIOD_JIFFIES;
	add_timer(&timer);
	return err;

err_out_free_fan1:
	gpio_free(fan1.gpio);

err_out:
	printk(KERN_WARNING DEVICE_NAME ": init returns error %d\n", err);
	return err;
}

static void __exit fan_tach_exit(void)
{
	del_timer(&timer);
	gpio_free(fan2.gpio);
	gpio_free(fan1.gpio);
	unregister_chrdev(FAN_TACH_MAJOR, DEVICE_NAME);
}

module_init(fan_tach_init);
module_exit(fan_tach_exit);

MODULE_AUTHOR("Raphael Oberson, Filmetrics Inc.");
MODULE_DESCRIPTION("Fan Tach Measurement Driver");
MODULE_LICENSE("GPL");
