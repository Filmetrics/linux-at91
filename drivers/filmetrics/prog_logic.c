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

#include <linux/io.h>

#define DEVICE_NAME	"prog_logic"

#define PORT_OUT_SHUTDOWN  AT91_PIN_PD0

#define PIO_PSR_D	0xFFFFF808
#define PIO_ABCDSR1_D	0xFFFFF870
#define PIO_ABCDSR2_D	0xFFFFF874
#define PMC_PCK1	0xFFFFFC44

#define PRINT_REG(reg) \
	printk(KERN_INFO DEVICE_NAME ": " #reg " = 0x%08X\n", read_reg(reg));

static struct clk *prog_logic_clock = NULL;

static unsigned int read_reg(unsigned int reg_addr)
{
	void *reg = ioremap(reg_addr, sizeof(unsigned int));
	return ioread32(reg);
}

static void print_regs(void)
{
	PRINT_REG(PIO_PSR_D);
	PRINT_REG(PIO_ABCDSR1_D);
	PRINT_REG(PIO_ABCDSR2_D);
	PRINT_REG(PMC_PCK1);
}

static int __init prog_logic_init(void)
{
	int err;
	struct clk *mck = NULL;

	printk(KERN_INFO DEVICE_NAME ": init [9]\n"); ////

	/* Set up Shutdown GPIO pin: "LOW" turns programmable logic chip ON */
	err = gpio_request_one(PORT_OUT_SHUTDOWN, GPIOF_OUT_INIT_LOW,
				"GPIO_SHUTDOWN");
	if (err != 0) goto gpio_fail;

	// Clock pin set by sama5d3_wm8904 sound driver (DTSI modified)

	/// gpio_free(AT91_PIN_PD31);
	/// at91_set_B_periph(AT91_PIN_PD31, 0);

	print_regs();

	/* Set up programmable logic's 66 MHz clock on PCK1 */
	prog_logic_clock = clk_get(NULL, "pck1");
	if (prog_logic_clock == NULL) {
		err = -1;
		goto clk_fail;
	}

	mck = clk_get(NULL, "mck");
	if (mck == NULL) {
		err = -1;
		goto clk_fail;
	}
	clk_set_parent(prog_logic_clock, mck);  /// TODO - check retval...
	clk_set_rate(prog_logic_clock, 66000000);  /// TODO - check retval...

	err = clk_prepare(prog_logic_clock);
	if (err != 0) goto clk_fail;

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
