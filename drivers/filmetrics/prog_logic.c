/*******************************************************************************
*
*  prog_logic.c - Filmetrics Programmable Logic IC Driver
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
#include <linux/gpio.h>

#define DEVICE_NAME	"prog_logic"

#define PORT_OUT_SHUTDOWN  AT91_PIN_PD0

static int __init prog_logic_init(void)
{
	int err;

	/* Set up Shutdown GPIO pin: "LOW" turns programmable logic chip ON */
	err = gpio_request_one(PORT_OUT_SHUTDOWN, GPIOF_OUT_INIT_LOW,
				"GPIO_SHUTDOWN");

	if (err != 0)
		printk(KERN_WARNING DEVICE_NAME
				": init returns error %d\n", err);

	printk(KERN_INFO DEVICE_NAME ": init [2]\n"); ////
	return err;
}

static void __exit prog_logic_exit(void)
{
	gpio_set_value(PORT_OUT_SHUTDOWN, 1);
	gpio_direction_input(PORT_OUT_SHUTDOWN);
	gpio_free(PORT_OUT_SHUTDOWN);
	printk(KERN_INFO DEVICE_NAME ": exit\n"); ////
}

module_init(prog_logic_init);
module_exit(prog_logic_exit);

MODULE_AUTHOR("Raphael Oberson, Filmetrics Inc.");
MODULE_DESCRIPTION("Filmetrics Programmable Logic IC Driver");
MODULE_LICENSE("GPL");
