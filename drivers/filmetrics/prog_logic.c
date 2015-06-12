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
#include <linux/clk.h>
#include <linux/gpio.h>

#define DEVICE_NAME	"prog_logic"

#define PORT_OUT_SHUTDOWN  AT91_PIN_PD0

static struct clk *prog_logic_clock = NULL;

static int __init prog_logic_init(void)
{
	int err;

	printk(KERN_INFO DEVICE_NAME ": init [4]\n"); ////

	/* Set up Shutdown GPIO pin: "LOW" turns programmable logic chip ON */
	err = gpio_request_one(PORT_OUT_SHUTDOWN, GPIOF_OUT_INIT_LOW,
				"GPIO_SHUTDOWN");
	if (err != 0) goto gpio_fail;

	/* Set up programmable logic's 66 MHz clock on PCK1 */
	prog_logic_clock = clk_get(NULL, "pck1");
	if (prog_logic_clock == NULL) {
		err = -1;
		goto clk_fail;
	}

	err = clk_enable(prog_logic_clock);
	if (err != 0) goto clk_fail;

	return 0;  /* Init successful */

clk_fail:
	gpio_set_value(PORT_OUT_SHUTDOWN, 1);
	gpio_direction_input(PORT_OUT_SHUTDOWN);
	gpio_free(PORT_OUT_SHUTDOWN);

gpio_fail:
	printk(KERN_WARNING DEVICE_NAME ": init returns error %d\n", err);
	return err;
}

static void __exit prog_logic_exit(void)
{
	clk_disable(prog_logic_clock);
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
